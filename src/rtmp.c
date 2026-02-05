#include "rtmp.h"

typedef struct RtmpPacket {
    int type;
    int stream_id;
    int timestamp;
    uint8_t *data;
    size_t len;
    struct RtmpPacket *next;
} RtmpPacket;

static const size_t MAX_QUEUE_BYTES = 512 * 1024;

static int socket_fd = -1;
static char rtmp_url[256];
static pthread_t recvPid, sndPid;
static pthread_mutex_t rtmp_mutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_mutex_t queue_mutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t queue_cond = PTHREAD_COND_INITIALIZER;
static struct FlvState flv_state;
static bool is_connected, metadata_sent, seq_header_sent;
static uint32_t start_timestamp;
static RtmpPacket *queue_head, *queue_tail;
static int queue_count;
static size_t queue_total_bytes;

static uint32_t get_rtmp_timestamp() {
    if (start_timestamp == 0) start_timestamp = millis();
    return millis() - start_timestamp;
}

static int send_data(const void *buf, size_t len) {
    if (socket_fd < 0) return -1;
    ssize_t sent = 0;
    const char *p = buf;
    while (len > 0) {
        sent = send(socket_fd, p, len, MSG_NOSIGNAL);
        if (sent < 0) {
            if (errno == EINTR) continue;
            return -1;
        }
        p += sent;
        len -= sent;
    }
    return 0;
}

static int rtmp_handshake(void) {
    char c0c1[RTMP_SIG_SIZE + 1];
    char s0s1[RTMP_SIG_SIZE + 1];
    char c2[RTMP_SIG_SIZE];
    char s2[RTMP_SIG_SIZE];

    memset(c0c1, 0, sizeof(c0c1));
    c0c1[0] = 0x03;

    if (send_data(c0c1, sizeof(c0c1)) < 0) return -1;

    int received = 0;
    while (received < sizeof(s0s1)) {
        int r = recv(socket_fd, s0s1 + received, sizeof(s0s1) - received, 0);
        if (r <= 0) return -1;
        received += r;
    }

    if (s0s1[0] != 0x03) return -1;

    memcpy(c2, s0s1 + 1, RTMP_SIG_SIZE);
    if (send_data(c2, sizeof(c2)) < 0) return -1;

    received = 0;
    while (received < sizeof(s2)) {
        int r = recv(socket_fd, s2 + received, sizeof(s2) - received, 0);
        if (r <= 0) return -1;
        received += r;
    }

    return 0;
}

static int send_chunk_header(int cs_id, int fmt_type, int timestamp, int msg_len, int msg_type_id, int msg_stream_id) {
    uint8_t header[16];
    int len = 0;

    header[len++] = (fmt_type << 6) | (cs_id & 0x3F);

    if (fmt_type < 3) {
        int ts = (timestamp >= 0xFFFFFF) ? 0xFFFFFF : timestamp;
        header[len++] = (ts >> 16) & 0xFF;
        header[len++] = (ts >> 8) & 0xFF;
        header[len++] = ts & 0xFF;
    }

    if (fmt_type < 2) {
        header[len++] = (msg_len >> 16) & 0xFF;
        header[len++] = (msg_len >> 8) & 0xFF;
        header[len++] = msg_len & 0xFF;
        header[len++] = msg_type_id;
    }

    if (fmt_type < 1) {
        header[len++] = msg_stream_id & 0xFF;
        header[len++] = (msg_stream_id >> 8) & 0xFF;
        header[len++] = (msg_stream_id >> 16) & 0xFF;
        header[len++] = (msg_stream_id >> 24) & 0xFF;
    }

    if (fmt_type < 3 && timestamp >= 0xFFFFFF) {
        header[len++] = (timestamp >> 24) & 0xFF;
        header[len++] = (timestamp >> 16) & 0xFF;
        header[len++] = (timestamp >> 8) & 0xFF;
        header[len++] = timestamp & 0xFF;
    }

    return send_data(header, len);
}

static int rtmp_send_packet(int message_type, int stream_id, const void *data, int len, int timestamp) {
    int cs_id = RTMP_CS_CMD;
    if (message_type == RTMP_MSG_AUDIO) cs_id = RTMP_CS_AUDIO;
    else if (message_type == RTMP_MSG_VIDEO) cs_id = RTMP_CS_VIDEO;

    int chunk_size = RTMP_DEFAULT_CHUNK_SIZE;
    const uint8_t *ptr = data;
    int remaining = len;

    if (send_chunk_header(cs_id, 0, timestamp, len, message_type, stream_id) < 0) return -1;

    while (remaining > 0) {
        int to_send = (remaining > chunk_size) ? chunk_size : remaining;
        if (send_data(ptr, to_send) < 0) return -1;
        ptr += to_send;
        remaining -= to_send;

        if (remaining > 0) {
            uint8_t h = (3 << 6) | (cs_id & 0x3F);
            if (send_data(&h, 1) < 0) return -1;
        }
    }
    return 0;
}

static void queue_push(int type, int stream_id, int timestamp, const void *data, size_t len, bool force) {
    pthread_mutex_lock(&queue_mutex);

    if (!force && queue_total_bytes + len > MAX_QUEUE_BYTES) {
        HAL_WARNING("rtmp", "Queue full (%zu/%zu bytes), dropping packet!\n", queue_total_bytes, MAX_QUEUE_BYTES);
        pthread_mutex_unlock(&queue_mutex);
        return;
    }

    RtmpPacket *pkt = malloc(sizeof(RtmpPacket));
    if (!pkt) {
        pthread_mutex_unlock(&queue_mutex);
        return;
    }
    pkt->type = type;
    pkt->stream_id = stream_id;
    pkt->timestamp = timestamp;
    pkt->len = len;
    pkt->data = malloc(len);
    if (!pkt->data) {
        free(pkt);
        pthread_mutex_unlock(&queue_mutex);
        return;
    }
    memcpy(pkt->data, data, len);
    pkt->next = NULL;

    if (queue_tail) {
        queue_tail->next = pkt;
        queue_tail = pkt;
    } else {
        queue_head = queue_tail = pkt;
    }
    queue_count++;
    queue_total_bytes += len;
    pthread_cond_signal(&queue_cond);
    pthread_mutex_unlock(&queue_mutex);
}

static void *send_thread(void *arg) {
    while (keepRunning && is_connected) {
        pthread_mutex_lock(&queue_mutex);
        while (queue_head == NULL && keepRunning && is_connected) {
            pthread_cond_wait(&queue_cond, &queue_mutex);
        }

        if (!keepRunning || !is_connected) {
            pthread_mutex_unlock(&queue_mutex);
            break;
        }

        RtmpPacket *pkt = queue_head;
        queue_head = pkt->next;
        if (queue_head == NULL) queue_tail = NULL;
        queue_count--;
        queue_total_bytes -= pkt->len;
        pthread_mutex_unlock(&queue_mutex);

        rtmp_send_packet(pkt->type, pkt->stream_id, pkt->data, pkt->len, pkt->timestamp);

        free(pkt->data);
        free(pkt);
    }
    return NULL;
}

static int rtmp_connect(char *app, char *tcurl) {
    struct BitBuf buf;
    uint8_t buffer[4096];
    buf.buf = buffer;
    buf.size = sizeof(buffer);
    buf.offset = 0;

    AMFWriteString(&buf, "connect", 7);
    AMFWriteDouble(&buf, 1.0);
    AMFWriteObject(&buf);
    AMFWriteNamedString(&buf, "app", 3, app, strlen(app));
    AMFWriteNamedString(&buf, "tcUrl", 5, tcurl, strlen(tcurl));
    AMFWriteNamedString(&buf, "flashVer", 8, "FMLE/3.0", 8);
    AMFWriteObjectEnd(&buf);

    return rtmp_send_packet(RTMP_MSG_AMF_CMD, 0, buffer, buf.offset, 0);
}

static int rtmp_create_stream(void) {
    struct BitBuf buf;
    uint8_t buffer[1024];
    buf.buf = buffer;
    buf.size = sizeof(buffer);
    buf.offset = 0;

    AMFWriteString(&buf, "createStream", 12);
    AMFWriteDouble(&buf, 2.0);
    AMFWriteNull(&buf);

    return rtmp_send_packet(RTMP_MSG_AMF_CMD, 0, buffer, buf.offset, 0);
}

static int rtmp_publish(char *stream_key) {
    struct BitBuf buf;
    uint8_t buffer[1024];
    buf.buf = buffer;
    buf.size = sizeof(buffer);
    buf.offset = 0;

    AMFWriteString(&buf, "publish", 7);
    AMFWriteDouble(&buf, 3.0);
    AMFWriteNull(&buf);
    AMFWriteString(&buf, stream_key, strlen(stream_key));
    AMFWriteString(&buf, "live", 4);

    return rtmp_send_packet(RTMP_MSG_AMF_CMD, 1, buffer, buf.offset, 0);
}

static int rtmp_start_sequence(const char *url) {
    char protocol[8], host[256], app[256], stream[256];
    int port = RTMP_PORT;

    char *p = (char *)url;
    if (strncmp(p, "rtmp://", 7) != 0) return -1;
    p += 7;

    char *slash = strchr(p, '/');
    if (!slash) return -1;

    int host_len = slash - p;
    strncpy(host, p, host_len);
    host[host_len] = '\0';

    char *curr_host = host;
    char *colon = strchr(host, ':');
    if (colon) {
        *colon = '\0';
        port = atoi(colon + 1);
    }

    p = slash + 1;
    slash = strchr(p, '/');
    if (!slash) return -1;

    int app_len = slash - p;
    strncpy(app, p, app_len);
    app[app_len] = '\0';

    strcpy(stream, slash + 1);

    struct hostent *he = gethostbyname(curr_host);
    if (!he) return -1;

    socket_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (socket_fd < 0) return -1;

    struct sockaddr_in serv_addr;
    memset(&serv_addr, 0, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    bcopy((char *)he->h_addr_list[0], (char *)&serv_addr.sin_addr.s_addr, he->h_length);
    serv_addr.sin_port = htons(port);

    if (connect(socket_fd, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
        close(socket_fd);
        socket_fd = -1;
        return -1;
    }

    if (rtmp_handshake() < 0) {
        close(socket_fd);
        socket_fd = -1;
        return -1;
    }

    char tcurl[512];
    snprintf(tcurl, sizeof(tcurl), "rtmp://%s:%d/%s", curr_host, port, app);

    if (rtmp_connect(app, tcurl) < 0) return -1;
    if (rtmp_create_stream() < 0) return -1;
    if (rtmp_publish(stream) < 0) return -1;

    return 0;
}

static void *recv_thread(void *arg) {
    uint8_t buffer[4096];
    while (keepRunning && is_connected) {
        ssize_t r = recv(socket_fd, buffer, sizeof(buffer), 0);
        if (r <= 0) {
            if (r < 0 && errno == EINTR) continue;
            HAL_DANGER("rtmp", "Lost connection (errno=%d)\n", errno);
            break;
        }
    }
    return NULL;
}

/**
 * Initializes the RTMP connection
 * @param url The RTMP URL (e.g., "rtmp://host/app/stream")
 * @return EXIT_SUCCESS or EXIT_FAILURE
 */
int rtmp_init(const char *url) {
    if (is_connected) rtmp_close();

    pthread_mutex_lock(&rtmp_mutex);
    memset(&flv_state, 0, sizeof(flv_state));
    seq_header_sent = false;
    metadata_sent = false;
    int ret = rtmp_start_sequence(url);
    if (ret == 0) {
        start_timestamp = millis();
        is_connected = true;

        if (pthread_create(&recvPid, NULL, recv_thread, NULL)) {
            HAL_ERROR("rtmp", "Failed to create receiver thread\n");
            is_connected = false;
            close(socket_fd);
            socket_fd = -1;
            ret = EXIT_FAILURE;
        } else if (pthread_create(&sndPid, NULL, send_thread, NULL)) {
            HAL_ERROR("rtmp", "Failed to create sender thread\n");
            is_connected = false;
            pthread_join(recvPid, NULL);
            close(socket_fd);
            socket_fd = -1;
            ret = EXIT_FAILURE;
        } else {
            strcpy(rtmp_url, url);
            HAL_INFO("rtmp", "Connected to %s\n", url);
            ret = EXIT_SUCCESS;
        }
    } else {
        HAL_ERROR("rtmp", "Failed to connect to %s\n", url);
        ret = EXIT_FAILURE;
    }
    pthread_mutex_unlock(&rtmp_mutex);
    return ret;
}

/**
 * Closes the RTMP connection and cleans up resources
 */
void rtmp_close(void) {
    pthread_mutex_lock(&rtmp_mutex);

    pthread_mutex_lock(&queue_mutex);
    pthread_cond_broadcast(&queue_cond);
    pthread_mutex_unlock(&queue_mutex);

    if (socket_fd >= 0) {
        shutdown(socket_fd, SHUT_RDWR);
        close(socket_fd);
        socket_fd = -1;
    }
    if (is_connected) {
        is_connected = false;
        pthread_join(recvPid, NULL);
        pthread_join(sndPid, NULL);
    }

    pthread_mutex_lock(&queue_mutex);
    while (queue_head) {
        RtmpPacket *pkt = queue_head;
        queue_head = pkt->next;
        free(pkt->data);
        free(pkt);
    }
    queue_tail = NULL;
    queue_count = 0;
    queue_total_bytes = 0;
    pthread_mutex_unlock(&queue_mutex);

    pthread_mutex_unlock(&rtmp_mutex);
    HAL_INFO("rtmp", "RTMP has closed!\n");
}

/**
 * Ingests a video packet into the RTMP stream
 * @param packet Pointer to the hal_vidpack structure containing NALU info
 * @param is_h265 1 if H.265 (HEVC), 0 for H.264 (AVC)
 * @return EXIT_SUCCESS or EXIT_FAILURE
 */
int rtmp_ingest_video(hal_vidpack *packet, int is_h265) {
    if (!is_connected) return EXIT_FAILURE;
    if (!packet || !packet->data) return EXIT_FAILURE;

    pthread_mutex_lock(&rtmp_mutex);

    uint32_t now = get_rtmp_timestamp();
    flv_state.timestamp_ms = now;
    flv_state.audio_timestamp_ms = now;

    int count = packet->naluCnt;
    if (count > 8) count = 8;

    for (int i = 0; i < count; i++) {
        hal_vidnalu *nalu = &packet->nalu[i];
        if (nalu->length == 0) continue;

        if (nalu->offset + nalu->length > packet->length) {
            HAL_WARNING("rtmp", "NAL offset is out of bounds (off=%u len=%u total=%u)\n",
                nalu->offset, nalu->length, packet->length);
            continue;
        }

        uint8_t *nal_start = packet->data + nalu->offset;
        uint32_t nal_len = nalu->length;
        int type = nalu->type;

        bool is_slice = false;
        bool is_idr = false;

        if (is_h265) {
            if (type == NalUnitType_VPS_HEVC) flv_set_vps((char *)nal_start, nal_len);
            if (type == NalUnitType_SPS_HEVC) flv_set_sps((char *)nal_start, nal_len);
            if (type == NalUnitType_PPS_HEVC) flv_set_pps((char *)nal_start, nal_len);
            if (type == NalUnitType_CodedSliceIdr || type == NalUnitType_CodedSliceNonIdr) {
                is_slice = true;
                is_idr = (type == NalUnitType_CodedSliceIdr);
            }
        } else {
            if (type == NalUnitType_SPS) flv_set_sps((char *)nal_start, nal_len);
            if (type == NalUnitType_PPS) flv_set_pps((char *)nal_start, nal_len);
            if (type == NalUnitType_CodedSliceIdr || type == NalUnitType_CodedSliceNonIdr) {
                is_slice = true;
                is_idr = (type == NalUnitType_CodedSliceIdr);
            }
        }

        if (is_slice) {
            if (!metadata_sent) {
                 struct BitBuf meta;
                 uint8_t meta_buf[1024];
                 meta.buf = meta_buf;
                 meta.size = sizeof(meta_buf);
                 meta.offset = 0;

                if (flv_get_metadata(&meta) == BUF_OK && meta.offset > 0) {
                     queue_push(RTMP_MSG_AMF_META, 1, 0, meta.buf, meta.offset, true);
                     metadata_sent = true;
                     HAL_INFO("rtmp", "Sent metadata (size=%d)\n", meta.offset);
                 }
            }

            if (!seq_header_sent) {
                if (!is_idr) continue;

                struct BitBuf header;
                if (flv_get_header(&header) == BUF_OK && header.offset > 11) {
                    queue_push(RTMP_MSG_VIDEO, 1, 0, header.buf + 11, header.offset - 15, true);
                    seq_header_sent = true;
                    HAL_INFO("rtmp", "Sent video sequence header (size=%d)\n", header.offset - 15);
                } else {
                     HAL_WARNING("rtmp", "Waiting for header generation...\n");
                     continue;
                }
            }

            if (seq_header_sent) {
                 if (flv_set_slice((char *)nal_start, nal_len, is_idr) == BUF_OK &&
                     flv_set_state(&flv_state) == BUF_OK) {
                     struct BitBuf tags;
                     if (flv_get_tags(&tags) == BUF_OK && tags.offset > 15) {
                         queue_push(RTMP_MSG_VIDEO, 1, flv_state.timestamp_ms, tags.buf + 11, tags.offset - 15, false);
                     }

                     if (flv_get_audio_tags(&tags) == BUF_OK && tags.offset > 15) {
                         flv_set_state(&flv_state);

                         queue_push(RTMP_MSG_AUDIO, 1, flv_state.audio_timestamp_ms, tags.buf + 11, tags.offset - 15, false);
                     }
                 }
            }
        }
    }

    pthread_mutex_unlock(&rtmp_mutex);
    return EXIT_SUCCESS;
}

/**
 * Ingests an audio packet into the RTMP stream
 * @param data Pointer to the audio data (MP3 as of now)
 * @param len Length of the data
 * @return EXIT_SUCCESS or EXIT_FAILURE
 */
int rtmp_ingest_audio(void *data, int len) {
    if (!is_connected) return EXIT_FAILURE;

    if (!seq_header_sent) return EXIT_SUCCESS;

    pthread_mutex_lock(&rtmp_mutex);

    if (flv_ingest_audio((char*)data, len) != BUF_OK) {
        pthread_mutex_unlock(&rtmp_mutex);
        return EXIT_FAILURE;
    }

    pthread_mutex_unlock(&rtmp_mutex);
    return EXIT_SUCCESS;
}
