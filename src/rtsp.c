#define RTSP_IMPLEMENTATION
#include "rtsp.h"

extern void request_idr(void);

#define RTSP_READ_BUFFER_SIZE 4096
#define RTSP_TCP_MAX_BUFFER (512 * 1024)

#define VIDEO_TRACK 0
#define AUDIO_TRACK 1

#define VIDEO_PAYLOAD_TYPE 96
#define VIDEO_CLOCK 90000
#define PCMU_PAYLOAD_TYPE 0
#define PCMA_PAYLOAD_TYPE 8
#define MP3_PAYLOAD_TYPE 14
#define G711_CLOCK 8000
#define MP3_CLOCK 90000

#define VIDEO_QUEUE 8
#define AUDIO_QUEUE 64

#define RTCP_SR_INTERVAL_MS 2000
#define NTP_EPOCH_OFFSET 2208988800u

static unsigned long long gen_session_id(void) {
    return ((unsigned long long)rand() << 32) | (unsigned)rand();
}

/* A growable output buffer: a real keyframe dwarfs the old fixed 4 KiB
 * response buffer, so the writer must be able to expand. */
typedef struct {
    int fd;
    unsigned char *data;
    size_t len, cap;
} ConnWriter;

static int connwriter_reserve(ConnWriter *w, size_t need) {
    if (w->cap >= need) return 0;
    size_t cap = w->cap ? w->cap : 4096;
    while (cap < need) cap <<= 1;
    unsigned char *p = realloc(w->data, cap);
    if (!p) return -1;
    w->data = p;
    w->cap = cap;
    return 0;
}

static ssize_t ConnWriter_write(VSelf, CharSlice99 data) {
    VSELF(ConnWriter);

    if (!data.len) return 0;
    if (connwriter_reserve(self, self->len + data.len) < 0) return -1;
    memcpy(self->data + self->len, data.ptr, data.len);
    self->len += data.len;
    return (ssize_t)data.len;
}

static void ConnWriter_lock(VSelf) {
    VSELF(ConnWriter);
    (void)self;
}

static void ConnWriter_unlock(VSelf) {
    VSELF(ConnWriter);
    (void)self;
}

static size_t ConnWriter_filled(VSelf) {
    VSELF(ConnWriter);
    return self->len;
}

static int ConnWriter_vwritef(VSelf, const char *restrict fmt, va_list ap) {
    VSELF(ConnWriter);

    va_list copy;
    va_copy(copy, ap);
    int n = vsnprintf(NULL, 0, fmt, copy);
    va_end(copy);
    if (n < 0) return -1;
    if (connwriter_reserve(self, self->len + n + 1) < 0) return -1;
    vsnprintf((char *)self->data + self->len, n + 1, fmt, ap);
    self->len += n;
    return n;
}

static int ConnWriter_writef(VSelf, const char *restrict fmt, ...) {
    VSELF(ConnWriter);

    va_list ap;
    va_start(ap, fmt);
    int ret = ConnWriter_vwritef(self, fmt, ap);
    va_end(ap);
    return ret;
}

impl(SmolRTSP_Writer, ConnWriter);

typedef struct {
    SmolRTSP_RtpTransport *rtp;
    SmolRTSP_NalTransport *nal;
    uint32_t ssrc;
    uint32_t last_ts;
    uint32_t packets;
    uint32_t octets;
    char active;
} Track;

struct __rtsp_obj_t;

typedef struct {
    struct __rtsp_obj_t *server;
    int fd;
    struct sockaddr_storage addr;
    socklen_t addr_len;
    char read_buf[RTSP_READ_BUFFER_SIZE];
    size_t read_len;
    ConnWriter out;
    uev_t io;
    int events;
    char eof;
    uint64_t session;
    Track tracks[RTSP_MAXIMUM_STREAMS];
    int rtcp_fd;
    uint8_t rtp_channel, rtcp_channel;
    char tcp;
    char playing;
} Client;

typedef struct {
    unsigned char *data;
    size_t len;
    uint64_t ts_us;
} VideoFrame;

typedef struct {
    unsigned char *data;
    size_t len;
    uint8_t pt;
    uint32_t ts;
} AudioFrame;

struct __rtsp_obj_t {
    uev_ctx_t loop;
    int sock;
    uev_t io_listen;
    uev_t mailbox;
    uev_t rtcp;
    pthread_t thread;
    pthread_mutex_t mtx;
    Client *clients[RTSP_MAXIMUM_CONNECTIONS];
    int count;
    unsigned short port;
    int priority;
    volatile char running;
    char auth;
    char user[32], pass[32];
    char isH265;
    unsigned char audioPt;
    char sps64[512], pps64[512], vps64[512], sps16[7];
    VideoFrame vq[VIDEO_QUEUE];
    int vhead, vcount;
    AudioFrame aq[AUDIO_QUEUE];
    int ahead, acount;
    uint32_t g711_ts;
    uint32_t mp3_ts;
};

declImpl(SmolRTSP_Controller, Client);

static void on_listen(uev_t *w, void *arg, int events);
static void on_client_io(uev_t *w, void *arg, int events);
static void on_mailbox(uev_t *w, void *arg, int events);
static void on_rtcp(uev_t *w, void *arg, int events);
static void close_client(struct __rtsp_obj_t *h, Client *c);
static void client_set_write(Client *c, int on);
static bool client_read(Client *c);
static bool client_write(Client *c);
static void track_drop(Track *t);
static int push_video_frame(struct __rtsp_obj_t *h, unsigned char *data, size_t len, uint64_t ts_us);
static int push_audio_frame(struct __rtsp_obj_t *h, unsigned char *data, size_t len, uint8_t pt, uint32_t ts);

static void writer_append(ConnWriter *w, const void *data, size_t len) {
    if (connwriter_reserve(w, w->len + len) < 0) return;
    memcpy(w->data + w->len, data, len);
    w->len += len;
}

static void buf_consume(Client *c, size_t n) {
    if (n >= c->read_len) {
        c->read_len = 0;
        return;
    }
    memmove(c->read_buf, c->read_buf + n, c->read_len - n);
    c->read_len -= n;
}

/* libuEv registers one epoll entry per descriptor, so both directions share a
 * single watcher whose event mask is switched as the output buffer fills up. */
static void client_set_write(Client *c, int on) {
    int want = on ? (c->events | UEV_WRITE) : (c->events & ~UEV_WRITE);
    if (want == c->events) return;
    c->events = want;
    uev_io_set(&c->io, c->fd, want);
}

static void track_drop(Track *t) {
    if (t->nal) {
        VTABLE(SmolRTSP_NalTransport, SmolRTSP_Droppable).drop(t->nal);
        t->nal = NULL;
        t->rtp = NULL;
    }
    if (t->rtp) {
        VTABLE(SmolRTSP_RtpTransport, SmolRTSP_Droppable).drop(t->rtp);
        t->rtp = NULL;
    }
    t->active = 0;
}

static void close_client(struct __rtsp_obj_t *h, Client *c) {
    for (int i = 0; i < h->count; i++) {
        if (h->clients[i] != c) continue;
        h->clients[i] = h->clients[--h->count];
        break;
    }

    uev_io_stop(&c->io);
    if (c->fd >= 0) close(c->fd);
    if (c->rtcp_fd >= 0) close(c->rtcp_fd);
    track_drop(&c->tracks[VIDEO_TRACK]);
    track_drop(&c->tracks[AUDIO_TRACK]);
    free(c->out.data);
    free(c);
}

/* SmolRTSP picks its RTP SSRC with rand() and never exposes it, so seed the
 * PRNG around construction and mirror the first draw to stamp matching RTCP
 * sender reports. */
static SmolRTSP_RtpTransport *make_rtp_transport(
    SmolRTSP_Transport t, uint8_t pt, uint32_t clock, uint32_t *ssrc) {
    unsigned seed = (unsigned)rand();
    srand(seed);
    *ssrc = (uint32_t)rand();
    srand(seed);
    return SmolRTSP_RtpTransport_new(t, pt, clock);
}

static void Client_drop(VSelf) {
    VSELF(Client);
    (void)self;
}

impl(SmolRTSP_Droppable, Client);

static bool check_auth(Client *c, SmolRTSP_Context *ctx, const SmolRTSP_Request *req) {
    (void)c;

    if (!app_config.rtsp_enable_auth) return true;

    char cred[70], valid[128];
    CharSlice99 auth;

    if (!SmolRTSP_HeaderMap_find(&req->header_map, SMOLRTSP_HEADER_AUTHORIZATION, &auth))
        goto deny;

    snprintf(cred, sizeof cred, "%s:%s", app_config.rtsp_auth_user, app_config.rtsp_auth_pass);
    strcpy(valid, "Basic ");
    base64_encode(valid + 6, cred, strlen(cred));

    if (auth.len != strlen(valid) || memcmp(auth.ptr, valid, auth.len))
        goto deny;

    return true;

deny:
    smolrtsp_header(ctx, SMOLRTSP_HEADER_WWW_AUTHENTICATE, "Basic realm=\"Divinus\"");
    smolrtsp_respond(ctx, SMOLRTSP_STATUS_UNAUTHORIZED, "Unauthorized");
    return false;
}

static void Client_options(VSelf, SmolRTSP_Context *ctx, const SmolRTSP_Request *req) {
    VSELF(Client);

    if (!check_auth(self, ctx, req)) return;

    smolrtsp_header(ctx, SMOLRTSP_HEADER_PUBLIC, "OPTIONS, DESCRIBE, SETUP, TEARDOWN, PLAY");
    smolrtsp_respond_ok(ctx);
}

static void Client_describe(VSelf, SmolRTSP_Context *ctx, const SmolRTSP_Request *req) {
    VSELF(Client);
    struct __rtsp_obj_t *h = self->server;

    if (!check_auth(self, ctx, req)) return;

    char sdp[1536] = {0};
    SmolRTSP_Writer w = smolrtsp_string_writer(sdp);
    ssize_t ret = 0;

    char sps[512] = {0}, pps[512] = {0}, vps[512] = {0}, plid[7] = {0};
    char isH265;
    unsigned char audioPt;

    pthread_mutex_lock(&h->mtx);
    strncpy(sps, h->sps64, sizeof sps - 1);
    strncpy(pps, h->pps64, sizeof pps - 1);
    strncpy(vps, h->vps64, sizeof vps - 1);
    strncpy(plid, h->sps16, sizeof plid - 1);
    isH265 = h->isH265;
    audioPt = h->audioPt;
    pthread_mutex_unlock(&h->mtx);

    // clang-format off
    SMOLRTSP_SDP_DESCRIBE(
        ret, w,
        (SMOLRTSP_SDP_VERSION, "0"),
        (SMOLRTSP_SDP_ORIGIN, "Divinus 0 0 IN IP4 0.0.0.0"),
        (SMOLRTSP_SDP_SESSION_NAME, "Divinus"),
        (SMOLRTSP_SDP_CONNECTION, "IN IP4 0.0.0.0"),
        (SMOLRTSP_SDP_TIME, "0 0"));

    SMOLRTSP_SDP_DESCRIBE(
        ret, w,
        (SMOLRTSP_SDP_MEDIA, "video 0 RTP/AVP %d", VIDEO_PAYLOAD_TYPE),
        (SMOLRTSP_SDP_ATTR, "control:video"),
        (SMOLRTSP_SDP_ATTR, "rtpmap:%d %s/%d", VIDEO_PAYLOAD_TYPE,
            isH265 ? "H265" : "H264", VIDEO_CLOCK));

    if (!isH265 && *sps && *pps) {
        if (*plid)
            SMOLRTSP_SDP_DESCRIBE(ret, w, (SMOLRTSP_SDP_ATTR,
                "fmtp:%d packetization-mode=1;profile-level-id=%s;sprop-parameter-sets=%s,%s",
                VIDEO_PAYLOAD_TYPE, plid, sps, pps));
        else
            SMOLRTSP_SDP_DESCRIBE(ret, w, (SMOLRTSP_SDP_ATTR,
                "fmtp:%d packetization-mode=1;sprop-parameter-sets=%s,%s",
                VIDEO_PAYLOAD_TYPE, sps, pps));
    } else if (isH265 && *vps && *sps && *pps) {
        SMOLRTSP_SDP_DESCRIBE(ret, w, (SMOLRTSP_SDP_ATTR,
            "fmtp:%d sprop-vps=%s;sprop-sps=%s;sprop-pps=%s",
            VIDEO_PAYLOAD_TYPE, vps, sps, pps));
    } else if (!isH265) {
        SMOLRTSP_SDP_DESCRIBE(ret, w,
            (SMOLRTSP_SDP_ATTR, "fmtp:%d packetization-mode=1", VIDEO_PAYLOAD_TYPE));
    }

    if (audioPt != 255) {
        const char *name = audioPt == PCMU_PAYLOAD_TYPE ? "PCMU" :
            audioPt == PCMA_PAYLOAD_TYPE ? "PCMA" : "MPA";
        uint32_t clock = (audioPt == PCMU_PAYLOAD_TYPE || audioPt == PCMA_PAYLOAD_TYPE)
            ? G711_CLOCK : MP3_CLOCK;
        SMOLRTSP_SDP_DESCRIBE(ret, w,
            (SMOLRTSP_SDP_MEDIA, "audio 0 RTP/AVP %d", audioPt),
            (SMOLRTSP_SDP_ATTR, "control:audio"),
            (SMOLRTSP_SDP_ATTR, "rtpmap:%d %s/%" PRIu32, audioPt, name, clock));
    }
    // clang-format on

    smolrtsp_header(ctx, SMOLRTSP_HEADER_CONTENT_TYPE, "application/sdp");
    smolrtsp_body(ctx, CharSlice99_from_str(sdp));
    smolrtsp_respond_ok(ctx);
}

static void Client_setup(VSelf, SmolRTSP_Context *ctx, const SmolRTSP_Request *req) {
    VSELF(Client);
    struct __rtsp_obj_t *h = self->server;

    if (!check_auth(self, ctx, req)) return;

    CharSlice99 transport_val;
    if (!SmolRTSP_HeaderMap_find(&req->header_map, SMOLRTSP_HEADER_TRANSPORT, &transport_val)) {
        smolrtsp_respond(ctx, SMOLRTSP_STATUS_BAD_REQUEST, "`Transport' not present");
        return;
    }

    SmolRTSP_TransportConfig config;
    if (smolrtsp_parse_transport(&config, transport_val) == -1) {
        smolrtsp_respond(ctx, SMOLRTSP_STATUS_BAD_REQUEST, "Malformed `Transport'");
        return;
    }

    const bool is_audio =
        CharSlice99_primitive_ends_with(req->start_line.uri, CharSlice99_from_str("/audio"));
    const int track = is_audio ? AUDIO_TRACK : VIDEO_TRACK;

    pthread_mutex_lock(&h->mtx);
    uint8_t payload = is_audio
        ? (h->audioPt == 255 ? PCMU_PAYLOAD_TYPE : h->audioPt)
        : VIDEO_PAYLOAD_TYPE;
    uint32_t clock = is_audio
        ? ((payload == PCMU_PAYLOAD_TYPE || payload == PCMA_PAYLOAD_TYPE) ? G711_CLOCK : MP3_CLOCK)
        : VIDEO_CLOCK;
    pthread_mutex_unlock(&h->mtx);

    track_drop(&self->tracks[track]);

    if (!self->session)
        self->session = gen_session_id();

    if (config.lower == SmolRTSP_LowerTransport_TCP) {
        if (!MATCHES(config.interleaved, SmolRTSP_ChannelPair_Some)) {
            smolrtsp_respond(ctx, SMOLRTSP_STATUS_BAD_REQUEST, "`interleaved' not found");
            return;
        }

        SmolRTSP_ChannelPair pair = {0};
        ifLet(config.interleaved, SmolRTSP_ChannelPair_Some, v) {
            pair = *v;
        }

        SmolRTSP_Writer writer = SmolRTSP_Context_get_writer(ctx);
        SmolRTSP_Transport t = smolrtsp_transport_tcp(writer, pair.rtp_channel, RTSP_TCP_MAX_BUFFER);

        self->tcp = 1;
        self->rtp_channel = pair.rtp_channel;
        self->rtcp_channel = pair.rtcp_channel;

        self->tracks[track].rtp =
            make_rtp_transport(t, payload, clock, &self->tracks[track].ssrc);
        if (track == VIDEO_TRACK)
            self->tracks[track].nal = SmolRTSP_NalTransport_new(self->tracks[track].rtp);
        self->tracks[track].active = 1;

        smolrtsp_header(ctx, SMOLRTSP_HEADER_TRANSPORT,
            "RTP/AVP/TCP;unicast;interleaved=%" PRIu8 "-%" PRIu8,
            pair.rtp_channel, pair.rtcp_channel);
    } else {
        if (!MATCHES(config.client_port, SmolRTSP_PortPair_Some)) {
            smolrtsp_respond(ctx, SMOLRTSP_STATUS_BAD_REQUEST, "`client_port' not found");
            return;
        }

        ifLet(config.client_port, SmolRTSP_PortPair_Some, ports) {
            const struct sockaddr *addr = (const struct sockaddr *)&self->addr;
            int af = addr->sa_family;
            void *ip = smolrtsp_sockaddr_ip(addr);

            int fd = smolrtsp_dgram_socket(af, ip, ports->rtp_port);
            if (fd == -1) {
                smolrtsp_respond_internal_error(ctx);
                return;
            }
            self->rtcp_fd = smolrtsp_dgram_socket(af, ip, ports->rtcp_port);

            SmolRTSP_Transport t = smolrtsp_transport_udp(fd);
            self->tracks[track].rtp =
                make_rtp_transport(t, payload, clock, &self->tracks[track].ssrc);
            if (track == VIDEO_TRACK)
                self->tracks[track].nal = SmolRTSP_NalTransport_new(self->tracks[track].rtp);
            self->tracks[track].active = 1;

            smolrtsp_header(ctx, SMOLRTSP_HEADER_TRANSPORT,
                "RTP/AVP/UDP;unicast;client_port=%" PRIu16 "-%" PRIu16,
                ports->rtp_port, ports->rtcp_port);
        }
    }

    smolrtsp_header(ctx, SMOLRTSP_HEADER_SESSION, "%" PRIu64, self->session);
    smolrtsp_respond_ok(ctx);
}

static void Client_play(VSelf, SmolRTSP_Context *ctx, const SmolRTSP_Request *req) {
    VSELF(Client);

    if (!check_auth(self, ctx, req)) return;

    self->playing = 1;

    smolrtsp_header(ctx, SMOLRTSP_HEADER_SESSION, "%" PRIu64, self->session);
    smolrtsp_header(ctx, SMOLRTSP_HEADER_RANGE, "npt=now-");
    smolrtsp_respond_ok(ctx);

    request_idr();
}

static void Client_teardown(VSelf, SmolRTSP_Context *ctx, const SmolRTSP_Request *req) {
    VSELF(Client);

    if (!check_auth(self, ctx, req)) return;

    self->playing = 0;
    track_drop(&self->tracks[VIDEO_TRACK]);
    track_drop(&self->tracks[AUDIO_TRACK]);

    smolrtsp_respond_ok(ctx);
}

static void Client_unknown(VSelf, SmolRTSP_Context *ctx, const SmolRTSP_Request *req) {
    VSELF(Client);
    (void)self;
    (void)req;

    smolrtsp_respond(ctx, SMOLRTSP_STATUS_METHOD_NOT_ALLOWED, "Method Not Allowed");
}

static SmolRTSP_ControlFlow Client_before(
    VSelf, SmolRTSP_Context *ctx, const SmolRTSP_Request *req) {
    VSELF(Client);
    (void)self;
    (void)ctx;
    (void)req;
    return SmolRTSP_ControlFlow_Continue;
}

static void Client_after(
    VSelf, ssize_t ret, SmolRTSP_Context *ctx, const SmolRTSP_Request *req) {
    VSELF(Client);
    (void)self;
    (void)ctx;
    (void)req;

    if (ret < 0)
        perror("Failed to respond");
}

impl(SmolRTSP_Controller, Client);

static bool client_read(Client *c) {
    ssize_t n = read(c->fd, c->read_buf + c->read_len, sizeof c->read_buf - c->read_len);
    if (n < 0) {
        if (errno == EAGAIN || errno == EWOULDBLOCK) return true;
        return false;
    }
    if (n == 0) {
        c->eof = 1;
        return true;
    }
    c->read_len += n;

    while (c->read_len >= 4 && c->read_buf[0] == '$') {
        size_t frame = 4 + ((size_t)c->read_buf[2] << 8 | c->read_buf[3]);
        if (c->read_len < frame) break;
        buf_consume(c, frame);
    }

    SmolRTSP_Request req = SmolRTSP_Request_uninit();
    SmolRTSP_ParseResult res = SmolRTSP_Request_parse(&req, CharSlice99_new(c->read_buf, c->read_len));

    match(res) {
        of(SmolRTSP_ParseResult_Success, status) {
            match(*status) {
                of(SmolRTSP_ParseStatus_Complete, offset) {
                    SmolRTSP_Controller ctrl = DYN(Client, SmolRTSP_Controller, c);
                    SmolRTSP_Writer conn = DYN(ConnWriter, SmolRTSP_Writer, &c->out);
                    smolrtsp_dispatch(conn, ctrl, &req);
                    buf_consume(c, *offset);
                }
                of(SmolRTSP_ParseStatus_Partial) {}
            }
        }
        of(SmolRTSP_ParseResult_Failure, e) {
            fputs("Failed to parse the request: ", stderr);
            if (SmolRTSP_ParseError_print(*e, smolrtsp_file_writer(stderr)) < 0)
                fputs("(unprintable)", stderr);
            fputs(".\n", stderr);
            return false;
        }
    }

    return true;
}

static bool client_write(Client *c) {
    while (c->out.len) {
        ssize_t n = write(c->fd, c->out.data, c->out.len);
        if (n < 0) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) return true;
            return false;
        }
        if (n == 0) return true;
        memmove(c->out.data, c->out.data + n, c->out.len - n);
        c->out.len -= n;
    }
    return true;
}

static void on_client_io(uev_t *w, void *arg, int events) {
    Client *c = arg;
    (void)w;

    if (events & (UEV_ERROR | UEV_HUP)) {
        close_client(c->server, c);
        return;
    }

    if ((events & UEV_READ) && !client_read(c)) {
        close_client(c->server, c);
        return;
    }

    if ((events & UEV_WRITE) && !client_write(c)) {
        close_client(c->server, c);
        return;
    }

    if (c->eof && !c->out.len) {
        close_client(c->server, c);
        return;
    }

    client_set_write(c, c->out.len > 0);
}

static int make_listen_socket(unsigned short port) {
    int fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0) return -1;

    int on = 1;
    setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &on, sizeof on);

    struct sockaddr_in addr = {
        .sin_family = AF_INET,
        .sin_port = htons(port),
        .sin_addr = {INADDR_ANY},
    };
    if (bind(fd, (struct sockaddr *)&addr, sizeof addr) < 0 ||
        listen(fd, RTSP_MAXIMUM_CONNECTIONS) < 0) {
        close(fd);
        return -1;
    }

    fcntl(fd, F_SETFL, fcntl(fd, F_GETFL, 0) | O_NONBLOCK);
    return fd;
}

static void on_listen(uev_t *w, void *arg, int events) {
    struct __rtsp_obj_t *h = arg;
    (void)w;

    if (events & UEV_ERROR) {
        HAL_DANGER("rtsp", "An error occured on the server socket!\n");
        uev_exit(&h->loop);
        return;
    }

    for (;;) {
        struct sockaddr_storage addr;
        socklen_t addr_len = sizeof addr;
        int fd = accept(h->sock, (struct sockaddr *)&addr, &addr_len);
        if (fd < 0) {
            if (errno != EAGAIN && errno != EWOULDBLOCK)
                HAL_DANGER("rtsp", "Accepting a connection failed!\n");
            return;
        }

        if (h->count >= RTSP_MAXIMUM_CONNECTIONS) {
            HAL_INFO("rtsp", "Too many clients, connection rejected!\n");
            close(fd);
            continue;
        }

        fcntl(fd, F_SETFL, fcntl(fd, F_GETFL, 0) | O_NONBLOCK);

        Client *c = calloc(1, sizeof *c);
        if (!c) {
            close(fd);
            continue;
        }

        c->server = h;
        c->fd = fd;
        c->rtcp_fd = -1;
        memcpy(&c->addr, &addr, addr_len);
        c->addr_len = addr_len;

        h->clients[h->count++] = c;

        c->events = UEV_READ;
        uev_io_init(&h->loop, &c->io, on_client_io, c, fd, c->events);

        HAL_INFO("rtsp", "A client has just connected!\n");
    }
}

static void send_sr(Client *c, int track) {
    Track *t = &c->tracks[track];
    if (!t->active || !t->rtp) return;

    struct timeval tv;
    gettimeofday(&tv, NULL);

    uint8_t pkt[36] = {0};
    pkt[0] = 0x80;                 /* V=2 */
    pkt[1] = 200;                  /* SR */
    pkt[2] = 0;
    pkt[3] = 8;                    /* length in 32-bit words, minus one */

    uint32_t v;
    v = htonl(t->ssrc);      memcpy(pkt + 4, &v, 4);
    v = htonl((uint32_t)(tv.tv_sec + NTP_EPOCH_OFFSET)); memcpy(pkt + 8, &v, 4);
    v = htonl((uint32_t)((double)tv.tv_usec / 1e6 * 4294967296.0)); memcpy(pkt + 12, &v, 4);
    v = htonl(t->last_ts);   memcpy(pkt + 16, &v, 4);
    v = htonl(t->packets);   memcpy(pkt + 20, &v, 4);
    v = htonl(t->octets);    memcpy(pkt + 24, &v, 4);

    if (c->tcp) {
        uint8_t head[4] = {'$', c->rtcp_channel, 0, 36};
        writer_append(&c->out, head, sizeof head);
        writer_append(&c->out, pkt, sizeof pkt);
        client_set_write(c, 1);
    } else if (c->rtcp_fd >= 0) {
        send(c->rtcp_fd, pkt, sizeof pkt, 0);
    }

    t->packets = 0;
    t->octets = 0;
}

static void on_rtcp(uev_t *w, void *arg, int events) {
    struct __rtsp_obj_t *h = arg;
    (void)w;
    (void)events;

    pthread_mutex_lock(&h->mtx);
    for (int i = 0; i < h->count; i++) {
        if (!h->clients[i]->playing) continue;
        send_sr(h->clients[i], VIDEO_TRACK);
        send_sr(h->clients[i], AUDIO_TRACK);
    }
    pthread_mutex_unlock(&h->mtx);
}

static void send_video(Client *c, const unsigned char *data, size_t len, uint32_t ts) {
    Track *t = &c->tracks[VIDEO_TRACK];
    if (!t->active || !t->nal) return;

    unsigned char *buf = (unsigned char *)data;
    unsigned char *nalptr = buf;
    size_t nalu_len = 0;

    while (nal_split(buf, &nalptr, &nalu_len, len) == 0) {
        if (nalu_len < 1) continue;

        SmolRTSP_NalUnit nalu;
        if (c->server->isH265) {
            if (nalu_len < 2) continue;
            nalu.header = SmolRTSP_NalHeader_H265(SmolRTSP_H265NalHeader_parse(nalptr));
            nalu.payload = U8Slice99_new(nalptr + 2, nalu_len - 2);
        } else {
            nalu.header = SmolRTSP_NalHeader_H264(SmolRTSP_H264NalHeader_parse(nalptr[0]));
            nalu.payload = U8Slice99_new(nalptr + 1, nalu_len - 1);
        }

        t->octets += (uint32_t)nalu_len;
        if (SmolRTSP_NalTransport_send_packet(t->nal, SmolRTSP_RtpTimestamp_Raw(ts), nalu) == -1)
            return;
        t->packets++;
    }

    t->last_ts = ts;
}

static void send_audio(Client *c, unsigned char *data, size_t len, uint8_t pt, uint32_t ts) {
    Track *t = &c->tracks[AUDIO_TRACK];
    if (!t->active || !t->rtp || pt != c->server->audioPt) return;

    /* RFC 2250 puts a four-byte payload header (two zero fields) in front of
     * MPEG audio; receivers strip it and would choke on the frame sync. */
    uint8_t mpa_hdr[4] = {0};
    U8Slice99 hdr = pt == MP3_PAYLOAD_TYPE
        ? U8Slice99_new(mpa_hdr, sizeof mpa_hdr) : U8Slice99_empty();

    t->octets += (uint32_t)len;
    if (SmolRTSP_RtpTransport_send_packet(
            t->rtp, SmolRTSP_RtpTimestamp_Raw(ts), true, hdr,
            U8Slice99_new(data, len)) == -1)
        return;

    t->packets++;
    t->last_ts = ts;
}

static void on_mailbox(uev_t *w, void *arg, int events) {
    struct __rtsp_obj_t *h = arg;
    (void)w;
    (void)events;

    if (!h->running) {
        uev_exit(&h->loop);
        return;
    }

    pthread_mutex_lock(&h->mtx);

    while (h->vcount) {
        VideoFrame f = h->vq[h->vhead];
        h->vhead = (h->vhead + 1) % VIDEO_QUEUE;
        h->vcount--;

        uint32_t ts = (uint32_t)((f.ts_us * 9) / 100);
        for (int i = 0; i < h->count; i++) {
            Client *c = h->clients[i];
            if (c->playing) send_video(c, f.data, f.len, ts);
        }
        free(f.data);
    }

    while (h->acount) {
        AudioFrame f = h->aq[h->ahead];
        h->ahead = (h->ahead + 1) % AUDIO_QUEUE;
        h->acount--;

        for (int i = 0; i < h->count; i++) {
            Client *c = h->clients[i];
            if (c->playing) send_audio(c, f.data, f.len, f.pt, f.ts);
        }
        free(f.data);
    }

    for (int i = 0; i < h->count; i++) {
        Client *c = h->clients[i];
        if (c->playing) client_set_write(c, c->out.len > 0);
    }

    pthread_mutex_unlock(&h->mtx);
}

static void collect_sprop(struct __rtsp_obj_t *h, unsigned char *data, size_t len, char isH265) {
    unsigned char *nalptr = data;
    size_t nalu_len = 0;

    while (nal_split(data, &nalptr, &nalu_len, len) == 0) {
        if (!isH265) {
            if (nalu_len < 1) continue;
            unsigned type = nalptr[0] & 0x1F;
            if (type == 7 && nalu_len <= 384) {
                base64_encode(h->sps64, (const char *)nalptr, (int)nalu_len);
                snprintf(h->sps16, sizeof h->sps16, "%02X%02X%02X",
                    nalu_len > 2 ? nalptr[1] : 0, nalu_len > 3 ? nalptr[2] : 0,
                    nalu_len > 4 ? nalptr[3] : 0);
            } else if (type == 8 && nalu_len <= 384) {
                base64_encode(h->pps64, (const char *)nalptr, (int)nalu_len);
            }
        } else {
            if (nalu_len < 2) continue;
            unsigned type = (nalptr[0] >> 1) & 0x3F;
            if (type == 32 && nalu_len <= 384)
                base64_encode(h->vps64, (const char *)nalptr, (int)nalu_len);
            else if (type == 33 && nalu_len <= 384)
                base64_encode(h->sps64, (const char *)nalptr, (int)nalu_len);
            else if (type == 34 && nalu_len <= 384)
                base64_encode(h->pps64, (const char *)nalptr, (int)nalu_len);
        }
    }
}

static int push_video_frame(struct __rtsp_obj_t *h, unsigned char *data, size_t len, uint64_t ts_us) {
    if (h->vcount == VIDEO_QUEUE) {
        free(h->vq[h->vhead].data);
        h->vhead = (h->vhead + 1) % VIDEO_QUEUE;
        h->vcount--;
    }

    unsigned char *copy = malloc(len);
    if (!copy) return EXIT_FAILURE;
    memcpy(copy, data, len);

    int tail = (h->vhead + h->vcount) % VIDEO_QUEUE;
    h->vq[tail].data = copy;
    h->vq[tail].len = len;
    h->vq[tail].ts_us = ts_us;
    h->vcount++;
    return EXIT_SUCCESS;
}

static int push_audio_frame(
    struct __rtsp_obj_t *h, unsigned char *data, size_t len, uint8_t pt, uint32_t ts) {
    if (h->acount == AUDIO_QUEUE) {
        free(h->aq[h->ahead].data);
        h->ahead = (h->ahead + 1) % AUDIO_QUEUE;
        h->acount--;
    }

    unsigned char *copy = malloc(len);
    if (!copy) return EXIT_FAILURE;
    memcpy(copy, data, len);

    int tail = (h->ahead + h->acount) % AUDIO_QUEUE;
    h->aq[tail].data = copy;
    h->aq[tail].len = len;
    h->aq[tail].pt = pt;
    h->aq[tail].ts = ts;
    h->acount++;
    return EXIT_SUCCESS;
}

int rtp_send_h26x(rtsp_handle h, hal_vidstream *stream, char isH265) {
    if (!h || !stream || !stream->count) return EXIT_FAILURE;

    uint64_t ts_us = stream->pack[0].timestamp
        ? (uint64_t)stream->pack[0].timestamp
        : (uint64_t)millis() * 1000;

    pthread_mutex_lock(&h->mtx);
    h->isH265 = isH265;
    for (unsigned int i = 0; i < stream->count; i++) {
        unsigned char *data = stream->pack[i].data + stream->pack[i].offset;
        size_t len = stream->pack[i].length - stream->pack[i].offset;
        collect_sprop(h, data, len, isH265);
        if (push_video_frame(h, data, len, ts_us) != EXIT_SUCCESS) break;
    }
    pthread_mutex_unlock(&h->mtx);

    if (h->running) uev_event_post(&h->mailbox);
    return EXIT_SUCCESS;
}

static int queue_audio(rtsp_handle h, unsigned char *buf, size_t len, uint8_t pt) {
    if (!h || !buf || !len) return EXIT_FAILURE;

    pthread_mutex_lock(&h->mtx);
    h->audioPt = pt;

    /* Both tracks must share one clock: the video timestamps come from the
     * encoder and count from boot, so anchor audio to the same monotonic
     * source instead of counting from this process' start. */
    uint32_t ts;
    if (pt == MP3_PAYLOAD_TYPE) {
        /* One MPEG-1 Layer III frame is 1152 samples, so the 90 kHz clock
         * advances by a fixed step rather than by the send time. */
        uint32_t srate = app_config.audio_srate ? app_config.audio_srate : 48000;
        if (!h->mp3_ts) h->mp3_ts = (uint32_t)millis() * 90;
        h->mp3_ts += (uint32_t)((uint64_t)1152 * MP3_CLOCK / srate);
        ts = h->mp3_ts;
    } else {
        if (!h->g711_ts) h->g711_ts = (uint32_t)millis() * 8;
        h->g711_ts += (uint32_t)len;
        ts = h->g711_ts;
    }

    int ret = push_audio_frame(h, buf, len, pt, ts);
    pthread_mutex_unlock(&h->mtx);

    if (h->running) uev_event_post(&h->mailbox);
    return ret;
}

int rtp_send_pcmu(rtsp_handle h, unsigned char *buf, size_t len) {
    return queue_audio(h, buf, len, PCMU_PAYLOAD_TYPE);
}

int rtp_send_pcma(rtsp_handle h, unsigned char *buf, size_t len) {
    return queue_audio(h, buf, len, PCMA_PAYLOAD_TYPE);
}

int rtp_send_mp3(rtsp_handle h, unsigned char *buf, size_t len) {
    return queue_audio(h, buf, len, MP3_PAYLOAD_TYPE);
}

void rtp_disable_audio(rtsp_handle h) {
    if (h) h->audioPt = 255;
}

void rtsp_configure_auth(rtsp_handle h, const char *user, const char *pass) {
    if (!h) return;

    if (user && pass) {
        h->auth = 1;
        strncpy(h->user, user, sizeof h->user - 1);
        strncpy(h->pass, pass, sizeof h->pass - 1);
    } else {
        h->auth = 0;
    }
}

static void *rtsp_thread(void *arg) {
    struct __rtsp_obj_t *h = arg;

    uev_init(&h->loop);

    h->sock = make_listen_socket(h->port);
    if (h->sock < 0) {
        perror("Starting the RTSP server failed");
        return NULL;
    }

    uev_io_init(&h->loop, &h->io_listen, on_listen, h, h->sock, UEV_READ);
    uev_event_init(&h->loop, &h->mailbox, on_mailbox, h);
    uev_timer_init(&h->loop, &h->rtcp, on_rtcp, h, RTCP_SR_INTERVAL_MS, RTCP_SR_INTERVAL_MS);

    h->running = 1;
    uev_run(&h->loop, UEV_NONE);

    while (h->count)
        close_client(h, h->clients[0]);

    close(h->sock);
    return NULL;
}

rtsp_handle rtsp_create(unsigned char max_con, unsigned int port, int priority) {
    if (max_con > RTSP_MAXIMUM_CONNECTIONS) {
        HAL_DANGER("rtsp", "At most %d connections are supported!\n", RTSP_MAXIMUM_CONNECTIONS);
        return NULL;
    }

    struct __rtsp_obj_t *h = calloc(1, sizeof *h);
    if (!h) return NULL;

    h->port = port;
    h->priority = priority;
    h->audioPt = 255;
    pthread_mutex_init(&h->mtx, NULL);

    srand((unsigned)time(NULL));

    pthread_attr_t attr;
    pthread_attr_init(&attr);
    if (priority > 0) {
        struct sched_param param = {.sched_priority = priority};
        pthread_attr_setschedparam(&attr, &param);
    }

    int ret = pthread_create(&h->thread, &attr, rtsp_thread, h);
    pthread_attr_destroy(&attr);
    if (ret) {
        perror("Starting the RTSP server thread failed");
        pthread_mutex_destroy(&h->mtx);
        free(h);
        return NULL;
    }

    return h;
}

void rtsp_finish(rtsp_handle h) {
    if (!h) return;

    h->running = 0;
    if (h->mailbox.fd > 0) uev_event_post(&h->mailbox);

    pthread_join(h->thread, NULL);

    for (int i = 0; i < VIDEO_QUEUE; i++) free(h->vq[i].data);
    for (int i = 0; i < AUDIO_QUEUE; i++) free(h->aq[i].data);

    pthread_mutex_destroy(&h->mtx);
    free(h);
}
