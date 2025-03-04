#include "rtsp.h"

#include "3rd/smolrtsp-all.h"

#include "audio.g711a.h"
#include "video.h264.h"

#define VIDEO_STREAM_ID 0
#if 0
#define VIDEO_FPS (app_config.mp4_enable ? app_config.mp4_fps : app_config.mjpeg_fps)
#endif
#define VIDEO_FPS 25
#define VIDEO_PAYLOAD_TYPE 96
#define VIDEO_SAMPLE_RATE 90000

#define AUDIO_STREAM_ID 1
#define AUDIO_PCMU_PAYLOAD_TYPE  0
#define AUDIO_SAMPLE_RATE        8000
#define AUDIO_SAMPLES_PER_PACKET 160
#define AUDIO_PACKETIZATION_TIME_US \
    (1e6 / (AUDIO_SAMPLE_RATE / AUDIO_SAMPLES_PER_PACKET))

typedef struct {
    uint64_t session_id;
    SmolRTSP_RtpTransport *transport;
    uev_t watch;
    SmolRTSP_Droppable ctx;
} Stream;

typedef struct {
    int sock_fd;
    struct sockaddr_storage addr;
    size_t addr_len;
    char read_buffer[RTSP_READ_BUFFER_SIZE];
    char write_buffer[RTSP_WRITE_BUFFER_SIZE];
    size_t read_pos, write_pos;
    uev_t watch;
    Stream streams[RTSP_MAXIMUM_STREAMS];
    int streams_playing;
} Client;

typedef struct {
    SmolRTSP_NalTransport *transport;
    SmolRTSP_NalStartCodeTester start_code_tester;
    uint32_t timestamp;
    U8Slice99 video;
    uint8_t *nalu_start;
    Client *client;
    int stream_id;
    int *streams_playing;
} VideoCtx;

typedef struct {
    SmolRTSP_RtpTransport *transport;
    size_t i;
    Client *client;
    int stream_id;
    int *streams_playing;
} AudioCtx;

static Client *Clients[RTSP_MAXIMUM_CONNECTIONS];
static int ClientCount = 0;
static int ServerSockFd = -1;
static uev_ctx_t ServerLoop;
static int ServerPort = 554;
static uev_t ServerWatch;

declImpl(SmolRTSP_Controller, Client);

static void handle_client_read(uev_t *w, void *arg, int evt);
static void handle_client_write(uev_t *w, void *arg, int evt);
static void listen_client(uev_t *w, void *arg, int evt);
static void close_client(Client *client);

static void handle_video(uev_t *w, void *arg, int evt);
static bool send_nalu(VideoCtx *ctx);
static SmolRTSP_Droppable play_video(Client *client, int stream_id);
static void handle_audio(uev_t *w, void *arg, int evt);
static SmolRTSP_Droppable play_audio(Client *client, int stream_id);

static bool check_client_auth(
    Client *self, SmolRTSP_Context *ctx, const SmolRTSP_Request *req);
static int setup_transport(
    Client *self, SmolRTSP_Context *ctx, const SmolRTSP_Request *req,
    SmolRTSP_Transport *t);
static int setup_tcp(
    SmolRTSP_Context *ctx, SmolRTSP_Transport *t,
    SmolRTSP_TransportConfig config);
static int setup_udp(
    const struct sockaddr *addr, SmolRTSP_Context *ctx, SmolRTSP_Transport *t,
    SmolRTSP_TransportConfig config);

static void handle_client_read(uev_t *w, void *arg, int evt) {
    Client *client = (Client *)arg;

    if (UEV_ERROR == evt || UEV_HUP == evt) {
        close_client(client);
        return;
    }
    
    ssize_t bytes_read = read(client->sock_fd, client->read_buffer + client->read_pos, 
                           sizeof(client->read_buffer) - client->read_pos);
    
    if (bytes_read <= 0) {
        if (bytes_read == 0 || (bytes_read < 0 && errno != EAGAIN && errno != EWOULDBLOCK)) {
            if (bytes_read < 0) {
                perror("Reading from the client socket failed");
            }
            close_client(client);
            return;
        }
        return;
    }
    
    client->read_pos += bytes_read;

    CharSlice99 buf = CharSlice99_new(client->read_buffer, client->read_pos);
    SmolRTSP_Writer conn = smolrtsp_string_writer(client->write_buffer);

    SmolRTSP_Request req = SmolRTSP_Request_uninit();
    const SmolRTSP_ParseResult res = SmolRTSP_Request_parse(&req, buf);

    SmolRTSP_Controller controller = DYN(Client, SmolRTSP_Controller, client);

    match(res) {
        of(SmolRTSP_ParseResult_Success, status) match(*status) {
            of(SmolRTSP_ParseStatus_Complete, offset) {
                smolrtsp_dispatch(conn, controller, &req);
                if (*offset < client->read_pos) {
                    memmove(client->read_buffer, client->read_buffer + *offset, client->read_pos - *offset);
                }
                client->read_pos -= *offset;
            }
            otherwise return;
        }
        of(SmolRTSP_ParseResult_Failure, e) {
            fputs("Failed to parse the request: ", stderr);
            const int err_bytes =
                SmolRTSP_ParseError_print(*e, smolrtsp_file_writer(stderr));
            assert(err_bytes >= 0);
            fputs(".\n", stderr);

            if (buf.len > 0) {
                if (buf.len < client->read_pos) {
                    memmove(client->read_buffer, client->read_buffer + buf.len, client->read_pos - buf.len);
                }
                client->read_pos -= buf.len;
            }
            return;
        }
    }

    if (client->write_pos > 0) {
        uev_io_stop(w);
        uev_io_init(&ServerLoop, &client->watch, handle_client_write, client, client->sock_fd, UEV_WRITE);
    }
}

static void handle_client_write(uev_t *w, void *arg, int evt) {
    Client *client = (Client *)arg;

    if (UEV_ERROR == evt || UEV_HUP == evt) {
        close_client(client);
        return;
    }
    
    ssize_t bytes_written = write(client->sock_fd, client->write_buffer, client->write_pos);
    
    if (bytes_written < 0) {
        if (errno != EAGAIN && errno != EWOULDBLOCK) {
            perror("Writing to the client socket failed");
            close_client(client);
            return;
        }
        return;
    }
    
    if ((size_t)bytes_written < client->write_pos) {
        memmove(client->write_buffer, client->write_buffer + bytes_written,
                client->write_pos - bytes_written);
        client->write_pos -= bytes_written;
    } else {
        client->write_pos = 0;
        client->write_buffer[0] = '\0';
        uev_io_stop(w);
        uev_io_init(&ServerLoop, &client->watch, handle_client_read, client, client->sock_fd, UEV_READ);
    }
}

static void listen_client(uev_t *w, void *arg, int evt) {
    (void)arg;

    if (evt & UEV_ERROR) {
        HAL_DANGER("rtsp", "An error occured on the server socket!\n");
        uev_exit(&ServerLoop);
        return;
    }

    char hoststr[NI_MAXHOST], portstr[NI_MAXSERV];
    struct sockaddr_storage client_addr;
    socklen_t client_len = sizeof(client_addr);
    
    int client_fd = accept(ServerSockFd, (struct sockaddr*)&client_addr, &client_len);
    if (client_fd < 0) {
        if (errno != EAGAIN && errno != EWOULDBLOCK) {
            perror("Accepting the connection failed!");
            uev_exit(&ServerLoop);
        }
        return;
    }
    
    if (ClientCount >= RTSP_MAXIMUM_CONNECTIONS) {
        HAL_DANGER("rtsp", "Too many clients, connection rejected!");
        close(client_fd);
        return;
    }
    
    int flags = fcntl(client_fd, F_GETFL, 0);
    fcntl(client_fd, F_SETFL, flags | O_NONBLOCK);
    
    Client *client = calloc(1, sizeof(Client));
    if (!client) {
        perror("Failed to allocate client");
        close(client_fd);
        return;
    }
    
    client->sock_fd = client_fd;
    memcpy(&client->addr, &client_addr, client_len);
    client->addr_len = client_len;
    
    Clients[ClientCount++] = client;

    int ret = getnameinfo((struct sockaddr *)&client_addr, 
        client_len, hoststr, sizeof(hoststr), portstr, sizeof(portstr), 
        NI_NUMERICHOST | NI_NUMERICSERV);
    if (!ret)
        HAL_INFO("rtsp", "%s:%s has just connected!\n", hoststr, portstr);
    else
        HAL_INFO("rtsp", "A client has just connected!\n");

    uev_io_init(&ServerLoop, &client->watch, handle_client_read, client, client_fd, UEV_READ);
}

static void close_client(Client *client) {
    int client_idx = -1;
    for (int i = 0; i < ClientCount; i++) {
        if (Clients[i] != client) continue;
        client_idx = i;
    }
    
    if (client_idx == -1) return;
    
    uev_io_stop(&client->watch);
    
    close(client->sock_fd);
    
    for (size_t i = 0; i < RTSP_MAXIMUM_STREAMS; i++) {
        if (!client->streams[i].ctx.vptr) continue;
        VCALL(client->streams[i].ctx, drop);
    }
    
    Clients[client_idx] = Clients[--ClientCount];
    free(client);
    
    HAL_INFO("rtsp", "A RTSP client has just disconnected!\n");
}

static void Client_drop(VSelf) {
    VSELF(Client);

    for (size_t i = 0; i < RTSP_MAXIMUM_STREAMS; i++) {
        if (self->streams[i].ctx.vptr != NULL) {
            VCALL(self->streams[i].ctx, drop);
        }
    }

    free(self);
}

impl(SmolRTSP_Droppable, Client);

static void
Client_options(VSelf, SmolRTSP_Context *ctx, const SmolRTSP_Request *req) {
    VSELF(Client);

    (void)self;
    (void)req;

    if (!check_client_auth(self, ctx, req)) return;

    smolrtsp_header(
        ctx, SMOLRTSP_HEADER_PUBLIC, "DESCRIBE, SETUP, TEARDOWN, PLAY");
    smolrtsp_respond_ok(ctx);
}

static void
Client_describe(VSelf, SmolRTSP_Context *ctx, const SmolRTSP_Request *req) {
    VSELF(Client);

    (void)self;
    (void)req;

    if (!check_client_auth(self, ctx, req)) return;

    char sdp_buf[1024] = {0};
    SmolRTSP_Writer sdp = smolrtsp_string_writer(sdp_buf);
    ssize_t ret = 0;

    // clang-format off
    SMOLRTSP_SDP_DESCRIBE(
        ret, sdp,
        (SMOLRTSP_SDP_VERSION, "0"),
        (SMOLRTSP_SDP_ORIGIN, "SmolRTSP 3855320066 3855320129 IN IP4 0.0.0.0"),
        (SMOLRTSP_SDP_SESSION_NAME, "Divinus"),
        (SMOLRTSP_SDP_CONNECTION, "IN IP4 0.0.0.0"),
        (SMOLRTSP_SDP_TIME, "0 0"));

    SMOLRTSP_SDP_DESCRIBE(
        ret, sdp,
        (SMOLRTSP_SDP_MEDIA, "audio 0 RTP/AVP %d", AUDIO_PCMU_PAYLOAD_TYPE),
        (SMOLRTSP_SDP_ATTR, "control:audio"));

    SMOLRTSP_SDP_DESCRIBE(
        ret, sdp,
        (SMOLRTSP_SDP_MEDIA, "video 0 RTP/AVP %d", VIDEO_PAYLOAD_TYPE),
        (SMOLRTSP_SDP_ATTR, "control:video"),
        (SMOLRTSP_SDP_ATTR, "rtpmap:%d H264/%" PRIu32, VIDEO_PAYLOAD_TYPE, VIDEO_SAMPLE_RATE),
        (SMOLRTSP_SDP_ATTR, "fmtp:%d packetization-mode=1", VIDEO_PAYLOAD_TYPE),
        (SMOLRTSP_SDP_ATTR, "framerate:%d", VIDEO_FPS));
    // clang-format on

    assert(ret > 0);

    smolrtsp_header(ctx, SMOLRTSP_HEADER_CONTENT_TYPE, "application/sdp");
    smolrtsp_body(ctx, CharSlice99_from_str(sdp_buf));

    smolrtsp_respond_ok(ctx);
}

static void
Client_setup(VSelf, SmolRTSP_Context *ctx, const SmolRTSP_Request *req) {
    VSELF(Client);

    if (!check_client_auth(self, ctx, req)) return;

    SmolRTSP_Transport transport;
    if (setup_transport(self, ctx, req, &transport) == -1) {
        return;
    }

    const size_t stream_id =
        CharSlice99_primitive_ends_with(
            req->start_line.uri, CharSlice99_from_str("/audio"))
            ? AUDIO_STREAM_ID
            : VIDEO_STREAM_ID;
    Stream *stream = &self->streams[stream_id];

    const bool aggregate_control_requested = SmolRTSP_HeaderMap_contains_key(
        &req->header_map, SMOLRTSP_HEADER_SESSION);
    if (aggregate_control_requested) {
        uint64_t session_id;
        if (smolrtsp_scanf_header(
                &req->header_map, SMOLRTSP_HEADER_SESSION, "%" SCNu64,
                &session_id) != 1) {
            smolrtsp_respond(
                ctx, SMOLRTSP_STATUS_BAD_REQUEST, "Malformed `Session'");
            return;
        }

        stream->session_id = session_id;
    } else {
        stream->session_id = (uint64_t)rand();
    }

    if (AUDIO_STREAM_ID == stream_id) {
        stream->transport = SmolRTSP_RtpTransport_new(
            transport, AUDIO_PCMU_PAYLOAD_TYPE, AUDIO_SAMPLE_RATE);
    } else {
        stream->transport = SmolRTSP_RtpTransport_new(
            transport, VIDEO_PAYLOAD_TYPE, VIDEO_SAMPLE_RATE);
    }

    smolrtsp_header(
        ctx, SMOLRTSP_HEADER_SESSION, "%" PRIu64, stream->session_id);

    smolrtsp_respond_ok(ctx);
}

static void
Client_play(VSelf, SmolRTSP_Context *ctx, const SmolRTSP_Request *req) {
    VSELF(Client);

    if (!check_client_auth(self, ctx, req)) return;

    uint64_t session_id;
    if (smolrtsp_scanf_header(
            &req->header_map, SMOLRTSP_HEADER_SESSION, "%" SCNu64,
            &session_id) != 1) {
        smolrtsp_respond(
            ctx, SMOLRTSP_STATUS_BAD_REQUEST, "Malformed `Session'");
        return;
    }

    bool played = false;
    for (size_t i = 0; i < RTSP_MAXIMUM_STREAMS; i++) {
        if (self->streams[i].session_id == session_id) {
            if (AUDIO_STREAM_ID == i) {
                self->streams[i].ctx = play_audio(self, i);
            } else {
                self->streams[i].ctx = play_video(self, i);
            }

            played = true;
        }
    }

    if (!played) {
        smolrtsp_respond(
            ctx, SMOLRTSP_STATUS_SESSION_NOT_FOUND, "Invalid Session ID");
        return;
    }

    smolrtsp_header(ctx, SMOLRTSP_HEADER_RANGE, "npt=now-");
    smolrtsp_respond_ok(ctx);
}

static void
Client_teardown(VSelf, SmolRTSP_Context *ctx, const SmolRTSP_Request *req) {
    VSELF(Client);

    if (!check_client_auth(self, ctx, req)) return;

    uint64_t session_id;
    if (smolrtsp_scanf_header(
            &req->header_map, SMOLRTSP_HEADER_SESSION, "%" SCNu64,
            &session_id) != 1) {
        smolrtsp_respond(
            ctx, SMOLRTSP_STATUS_BAD_REQUEST, "Malformed `Session'");
        return;
    }

    bool teardowned = false;
    for (size_t i = 0; i < RTSP_MAXIMUM_STREAMS; i++) {
        if (self->streams[i].session_id == session_id) {
            uev_timer_stop(&self->streams[i].watch);
            teardowned = true;
        }
    }

    if (!teardowned) {
        smolrtsp_respond(
            ctx, SMOLRTSP_STATUS_SESSION_NOT_FOUND, "Invalid Session ID");
        return;
    }

    smolrtsp_respond_ok(ctx);
}

static void
Client_unknown(VSelf, SmolRTSP_Context *ctx, const SmolRTSP_Request *req) {
    VSELF(Client);

    (void)self;
    (void)req;

    smolrtsp_respond(ctx, SMOLRTSP_STATUS_METHOD_NOT_ALLOWED, "Unknown method");
}

static SmolRTSP_ControlFlow
Client_before(VSelf, SmolRTSP_Context *ctx, const SmolRTSP_Request *req) {
    VSELF(Client);

    (void)self;
    (void)ctx;

#ifdef DEBUG_RTSP
    HAL_INFO("rtsp",
        "%s %s CSeq=%" PRIu32 ".\n",
        CharSlice99_alloca_c_str(req->start_line.method),
        CharSlice99_alloca_c_str(req->start_line.uri), req->cseq);
#endif

    return SmolRTSP_ControlFlow_Continue;
}

static void Client_after(
    VSelf, ssize_t ret, SmolRTSP_Context *ctx, const SmolRTSP_Request *req) {
    VSELF(Client);

    (void)self;
    (void)ctx;
    (void)req;

    if (ret < 0) {
        perror("Failed to respond");
    } else self->write_pos = ret;
}

impl(SmolRTSP_Controller, Client);

static bool check_client_auth(Client *self, SmolRTSP_Context *ctx, const SmolRTSP_Request *req) {
    char cred[64], valid[256];
    CharSlice99 auth;

    if (!app_config.rtsp_enable_auth) return true;

    if (SmolRTSP_HeaderMap_find(&req->header_map, SMOLRTSP_HEADER_AUTHORIZATION, &auth)) {
        sprintf(cred, "%s:%s", app_config.rtsp_auth_user, app_config.rtsp_auth_pass);
        strcpy(valid, "Basic ");
        base64_encode(valid + 6, cred, strlen(cred));
        if (strncmp(CharSlice99_alloca_c_str(auth), valid, strlen(valid))) goto bad_auth;
    } else goto bad_auth;

    return true;

bad_auth:
    smolrtsp_header(ctx, SMOLRTSP_HEADER_WWW_AUTHENTICATE, "Basic realm=\"Divinus\"");
    smolrtsp_respond(ctx, SMOLRTSP_STATUS_UNAUTHORIZED, "Unauthorized");
    return false;
}

static int setup_transport(
    Client *self, SmolRTSP_Context *ctx, const SmolRTSP_Request *req,
    SmolRTSP_Transport *t) {
    CharSlice99 transport_val;
    const bool transport_found = SmolRTSP_HeaderMap_find(
        &req->header_map, SMOLRTSP_HEADER_TRANSPORT, &transport_val);
    if (!transport_found) {
        smolrtsp_respond(
            ctx, SMOLRTSP_STATUS_BAD_REQUEST, "`Transport' not present");
        return -1;
    }

    SmolRTSP_TransportConfig config;
    if (smolrtsp_parse_transport(&config, transport_val) == -1) {
        smolrtsp_respond(
            ctx, SMOLRTSP_STATUS_BAD_REQUEST, "Malformed `Transport'");
        return -1;
    }

    switch (config.lower) {
    case SmolRTSP_LowerTransport_TCP:
        if (setup_tcp(ctx, t, config) == -1) {
            smolrtsp_respond_internal_error(ctx);
            return -1;
        }
        break;
    case SmolRTSP_LowerTransport_UDP:
        if (setup_udp((const struct sockaddr *)&self->addr, ctx, t, config) ==
            -1) {
            smolrtsp_respond_internal_error(ctx);
            return -1;
        }
        break;
    }

    return 0;
}

static int setup_tcp(
    SmolRTSP_Context *ctx, SmolRTSP_Transport *t,
    SmolRTSP_TransportConfig config) {
    ifLet(config.interleaved, SmolRTSP_ChannelPair_Some, interleaved) {
        *t = smolrtsp_transport_tcp(
            SmolRTSP_Context_get_writer(ctx), interleaved->rtp_channel, 0);

        smolrtsp_header(
            ctx, SMOLRTSP_HEADER_TRANSPORT,
            "RTP/AVP/TCP;unicast;interleaved=%" PRIu8 "-%" PRIu8,
            interleaved->rtp_channel, interleaved->rtcp_channel);
        return 0;
    }

    smolrtsp_respond(
        ctx, SMOLRTSP_STATUS_BAD_REQUEST, "`interleaved' not found");
    return -1;
}

static int setup_udp(
    const struct sockaddr *addr, SmolRTSP_Context *ctx, SmolRTSP_Transport *t,
    SmolRTSP_TransportConfig config) {

    ifLet(config.client_port, SmolRTSP_PortPair_Some, client_port) {
        int fd;
        if ((fd = smolrtsp_dgram_socket(
                 addr->sa_family, smolrtsp_sockaddr_ip(addr),
                 client_port->rtp_port)) == -1) {
            return -1;
        }

        *t = smolrtsp_transport_udp(fd);

        smolrtsp_header(
            ctx, SMOLRTSP_HEADER_TRANSPORT,
            "RTP/AVP/UDP;unicast;client_port=%" PRIu16 "-%" PRIu16,
            client_port->rtp_port, client_port->rtcp_port);
        return 0;
    }

    smolrtsp_respond(
        ctx, SMOLRTSP_STATUS_BAD_REQUEST, "`client_port' not found");
    return -1;
}

static void VideoCtx_drop(VSelf) {
    VSELF(VideoCtx);

    uev_timer_stop(&self->client->streams[self->stream_id].watch);

    VTABLE(SmolRTSP_NalTransport, SmolRTSP_Droppable).drop(self->transport);
    free(self);
}

impl(SmolRTSP_Droppable, VideoCtx);

static bool send_nalu(VideoCtx *ctx) {
    const SmolRTSP_NalUnit nalu = {
        .header = SmolRTSP_NalHeader_H264(
            SmolRTSP_H264NalHeader_parse(ctx->nalu_start[0])),
        .payload = U8Slice99_from_ptrdiff(ctx->nalu_start + 1, ctx->video.ptr),
    };

    bool au_found = false;

    if (SmolRTSP_NalHeader_unit_type(nalu.header) ==
        SMOLRTSP_H264_NAL_UNIT_AUD) {
        ctx->timestamp += VIDEO_SAMPLE_RATE / VIDEO_FPS;
        au_found = true;
    }

    if (SmolRTSP_NalTransport_send_packet(
            ctx->transport, SmolRTSP_RtpTimestamp_Raw(ctx->timestamp), nalu) ==
        -1) {
        perror("Failed to send RTP/NAL");
    }

    return au_found;
}

static void handle_video(uev_t *w, void *arg, int evt) {
    VideoCtx *ctx = (VideoCtx *)arg;
    
    bool loop_continue = true;
    while (loop_continue) {
        if (U8Slice99_is_empty(ctx->video)) {
            send_nalu(ctx);
            uev_timer_stop(&ctx->client->streams[ctx->stream_id].watch);
            (*ctx->streams_playing)--;
            if (0 == *ctx->streams_playing) {

            }
            return;
        }

        const size_t start_code_len = ctx->start_code_tester(ctx->video);
        if (0 == start_code_len) {
            ctx->video = U8Slice99_advance(ctx->video, 1);
            continue;
        }

        bool au_found = false;
        if (NULL != ctx->nalu_start) {
            au_found = send_nalu(ctx);
        }

        ctx->video = U8Slice99_advance(ctx->video, start_code_len);
        ctx->nalu_start = ctx->video.ptr;

        if (au_found) {
            loop_continue = false;
        }
    }
}


static SmolRTSP_Droppable play_video(Client *client, int stream_id) {
    U8Slice99 video = Slice99_typed_from_array(video_h264);

    SmolRTSP_NalStartCodeTester start_code_tester;
    if ((start_code_tester = smolrtsp_determine_start_code(video)) == NULL) {
        HAL_DANGER("rtsp", "Invalid video file!\n");
        abort();
    }

    VideoCtx *ctx = malloc(sizeof *ctx);
    assert(ctx);
    *ctx = (VideoCtx){
        .transport = SmolRTSP_NalTransport_new(client->streams[stream_id].transport),
        .start_code_tester = start_code_tester,
        .timestamp = 0,
        .video = video,
        .nalu_start = NULL,
        .client = client,
        .stream_id = stream_id,
        .streams_playing = &client->streams_playing,
    };

    uev_timer_init(&ServerLoop, &client->streams[stream_id].watch,
        handle_video, ctx, 1000 / VIDEO_FPS, 1000 / VIDEO_FPS);
    
    (client->streams_playing)++;

    return DYN(VideoCtx, SmolRTSP_Droppable, ctx);
}

static void AudioCtx_drop(VSelf) {
    VSELF(AudioCtx);

    uev_timer_stop(&self->client->streams[self->stream_id].watch);

    VTABLE(SmolRTSP_RtpTransport, SmolRTSP_Droppable).drop(self->transport);
    free(self);
}

impl(SmolRTSP_Droppable, AudioCtx);

static void handle_audio(uev_t *w, void *arg, int evt) {
    AudioCtx *ctx = (AudioCtx *)arg;
    
    if (ctx->i * AUDIO_SAMPLES_PER_PACKET >= audio_g711a_len) {
        uev_timer_stop(&ctx->client->streams[ctx->stream_id].watch);
        (*ctx->streams_playing)--;
        if (0 == *ctx->streams_playing) {

        }
        return;
    }
    
    const SmolRTSP_RtpTimestamp ts =
        SmolRTSP_RtpTimestamp_Raw(ctx->i * AUDIO_SAMPLES_PER_PACKET);
    const bool marker = false;
    const size_t samples_count =
        audio_g711a_len <
                ctx->i * AUDIO_SAMPLES_PER_PACKET + AUDIO_SAMPLES_PER_PACKET
            ? audio_g711a_len % AUDIO_SAMPLES_PER_PACKET
            : AUDIO_SAMPLES_PER_PACKET;
    const U8Slice99 header = U8Slice99_empty(),
                    payload = U8Slice99_new(
                        audio_g711a +
                            ctx->i * AUDIO_SAMPLES_PER_PACKET,
                        samples_count);

    if (SmolRTSP_RtpTransport_send_packet(
            ctx->transport, ts, marker, header, payload) == -1) {
        perror("Failed to send RTP/PCMU");
    }

    ctx->i++;
}

static SmolRTSP_Droppable play_audio(Client *client, int stream_id) {
    AudioCtx *ctx = malloc(sizeof *ctx);
    assert(ctx);
    *ctx = (AudioCtx){
        .transport = client->streams[stream_id].transport,
        .i = 0,
        .client = client,
        .stream_id = stream_id,
        .streams_playing = &client->streams_playing,
    };

    uev_timer_init(&ServerLoop, &client->streams[stream_id].watch,
        handle_audio, ctx, AUDIO_PACKETIZATION_TIME_US / 1000,
        AUDIO_PACKETIZATION_TIME_US / 1000);
    
    (client->streams_playing)++;

    return DYN(AudioCtx, SmolRTSP_Droppable, ctx);
}

static void *rtsp_thread(void) {
    uev_init(&ServerLoop);

    if ((ServerSockFd = socket(AF_INET, SOCK_STREAM, 0)) == -1) {
        perror("Creating the socket failed");
        return NULL;
    }

    int options = 1;
    if (setsockopt(ServerSockFd, SOL_SOCKET, SO_REUSEADDR, &options, sizeof options) < 0) {
        perror("Setting the socket options failed");
        return NULL;
    }

    int flags = fcntl(ServerSockFd, F_GETFL, 0);
    fcntl(ServerSockFd, F_SETFL, flags | O_NONBLOCK);

    struct sockaddr_in addr = {
        .sin_family = AF_INET,
        .sin_port = htons(ServerPort),
        .sin_addr = {INADDR_ANY},
    };

    if (bind(ServerSockFd, (struct sockaddr *)&addr, sizeof addr) == -1) {
        perror("Binding to the socket failed");
        return NULL;
    }

    if (listen(ServerSockFd, RTSP_MAXIMUM_CONNECTIONS) == -1) {
        perror("Listening on the socket failed");
        return NULL;
    }

    uev_io_init(&ServerLoop, &ServerWatch, listen_client, NULL, ServerSockFd, UEV_READ);

    HAL_INFO("rtsp", "Server has started on port %d\n", ServerPort);

    uev_run(&ServerLoop, UEV_NONE);

    for (int i = 0; i < ClientCount; i++) {
        if (!Clients[i]) continue;
        close_client(Clients[i]);
        free(Clients[i]);
        Clients[i] = NULL;
    }

    close(ServerSockFd);
    ServerSockFd = -1;
}

int rtsp_init(int priority) {
    pthread_t thread;
    pthread_attr_t attr;

    pthread_attr_init(&attr);
    pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);

    if (priority > 0) {
        struct sched_param param;
        param.sched_priority = priority;
        pthread_attr_setschedparam(&attr, &param);
    }

    int ret = pthread_create(&thread, &attr, (void *(*)(void *))rtsp_thread, NULL);
    pthread_attr_destroy(&attr);

    if (ret) {
        perror("Failed to create RTSP thread");
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}

void rtsp_finish(void) {
    if (ServerSockFd == -1) return;

    uev_exit(&ServerLoop);
}