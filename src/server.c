#include "server.h"

#define HTTP_MAX_CLIENTS 50
#define HTTP_MIN_BUF_SIZE 4096
#define HTTP_MAX_BUF_SIZE (4 * 1024 * 1024)

IMPORT_STR(.rodata, "../res/index.html", indexhtml);
extern const char indexhtml[];
IMPORT_STR(.rodata, "../res/onvif/badauth.xml", badauthxml);
extern const char badauthxml[];

enum StreamType {
    STREAM_H26X  = (1 << 0),
    STREAM_JPEG  = (1 << 1),
    STREAM_MJPEG = (1 << 2),
    STREAM_MP3   = (1 << 3),
    STREAM_MP4   = (1 << 4),
    STREAM_PCM   = (1 << 5)
};

typedef struct {
    int clntFd;
    char *input, *method, *payload, *prot, *query, *uri;
    int paysize, total;
    size_t buf_size;
    int header_len;
} http_request_t;

struct {
    int sockFd;
    enum StreamType type;
    struct Mp4State mp4;
    unsigned int nalCnt;
} client_fds[HTTP_MAX_CLIENTS];

typedef struct {
    char *name, *value;
} http_header_t;

typedef struct {
    int code;
    const char *msg, *desc;
} http_error_t;

const http_error_t http_errors[] = {
    {400, "Bad Request", "The server has no handler to the request."},
    {401, "Unauthorized", "You are not authorized to access this resource."},
    {403, "Forbidden", "You have been denied access to this resource."},
    {404, "Not Found", "The requested resource was not found."},
    {405, "Method Not Allowed", "This method is not handled on this endpoint."},
    {500, "Internal Server Error", "An invalid operation was caught on this request."},
    {501, "Not Implemented", "The server does not support the functionality."}
};
http_header_t http_headers[17] = {{"\0", "\0"}};

int server_fd = -1;
pthread_t server_thread_id;
pthread_mutex_t client_fds_mutex;

#include "sysauth.h"
static bool is_local_address(const char *client_ip) {
    if (!client_ip) return false;

    if (!strcmp(client_ip, "127.0.0.1") ||
        !strncmp(client_ip, "127.", 4))
        return true;

    if (!strcmp(client_ip, "::1"))
        return true;

    if (!strncmp(client_ip, "::ffff:127.", 11))
        return true;

    return false;
}

static void close_socket_fd(int sockFd) {
    shutdown(sockFd, SHUT_RDWR);
    close(sockFd);
}

void free_client(int i) {
    if (client_fds[i].sockFd < 0) return;

    close_socket_fd(client_fds[i].sockFd);
    client_fds[i].sockFd = -1;
    client_fds[i].type = -1;
}

int send_to_fd(int fd, char *buf, ssize_t size) {
    if (fd < 0) return -1;

    ssize_t total = 0;
    for (int attempts = 0; attempts < 4; attempts++) {
        ssize_t n = send(fd, buf + total, size - total, MSG_DONTWAIT | MSG_NOSIGNAL);
        if (n > 0) {
            total += n;
            attempts = -1;
        } else if (n == 0) return -1;
        else if (errno != EAGAIN && errno != EWOULDBLOCK) return -1;

        if (total == size) return EXIT_SUCCESS;

        struct pollfd pfd = { .fd = fd, .events = POLLOUT };
        if (poll(&pfd, 1, 25) < 0) return -1;
    }

    return -1;
}

int sendv_to_client(int i, struct iovec *iov, int iovcnt) {
    int fd = client_fds[i].sockFd;
    if (fd < 0) return -1;

    for (int attempts = 0; attempts < 4; attempts++) {
        ssize_t n = writev(fd, iov, iovcnt);
        if (n > 0) {
            while (n > 0 && iovcnt > 0) {
                if (n >= iov[0].iov_len) {
                    n -= iov[0].iov_len;
                    iov++;
                    iovcnt--;
                } else {
                    iov[0].iov_base = (char *)iov[0].iov_base + n;
                    iov[0].iov_len -= n;
                    n = 0;
                }
            }
            if (iovcnt == 0) return 0;
            attempts = -1;
        } else if (n == 0) goto error;
        else if (errno != EAGAIN && errno != EWOULDBLOCK && errno != EINTR) goto error;

        struct pollfd pfd = { .fd = fd, .events = POLLOUT };
        if (poll(&pfd, 1, 25) < 0) goto error;
    }

error:
    free_client(i);
    return -1;
}

int send_to_client(int i, char *buf, ssize_t size) {
    if (send_to_fd(client_fds[i].sockFd, buf, size) < 0) {
        free_client(i);
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}

static void send_and_close(int client_fd, char *buf, ssize_t size) {
    send_to_fd(client_fd, buf, size);
    close_socket_fd(client_fd);
}

void send_http_error(int fd, int code) {
    const char *desc = "\0", *msg = "Unspecified";
    char buffer[256];
    int len;

    for (int i = 0; i < sizeof(http_errors) / sizeof(*http_errors); i++) {
        if (http_errors[i].code == code) {
            desc = http_errors[i].desc;
            msg = http_errors[i].msg;
            break;
        }
    }

    len = snprintf(buffer, sizeof(buffer),
        "HTTP/1.1 %d %s\r\n"
        "Content-Type: text/plain\r\n"
        "Connection: close\r\n"
        "\r\n%s\r\n",
        code, msg, desc);

    send_and_close(fd, buffer, len);
}

void send_h26x_to_client(char index, hal_vidstream *stream) {
    for (unsigned int i = 0; i < stream->count; ++i) {
        hal_vidpack *pack = &stream->pack[i];
        unsigned char *pack_data = pack->data + pack->offset;

        pthread_mutex_lock(&client_fds_mutex);
        for (unsigned int c = 0; c < HTTP_MAX_CLIENTS; ++c) {
            if (client_fds[c].sockFd < 0) continue;
            if (client_fds[c].type != STREAM_H26X) continue;

            for (char j = 0; j < pack->naluCnt; j++) {
                if (client_fds[c].nalCnt == 0 &&
                    pack->nalu[j].type != NalUnitType_SPS &&
                    pack->nalu[j].type != NalUnitType_SPS_HEVC)
                    continue;

                char len_buf[16];
                int len_size = sprintf(len_buf, "%zX\r\n", pack->nalu[j].length);

                struct iovec iov[3];
                iov[0].iov_base = len_buf;
                iov[0].iov_len = len_size;
                iov[1].iov_base = pack_data + pack->nalu[j].offset;
                iov[1].iov_len = pack->nalu[j].length;
                iov[2].iov_base = (void*)"\r\n";
                iov[2].iov_len = 2;

                if (sendv_to_client(c, iov, 3) < 0)
                    break;

                client_fds[c].nalCnt++;
                if (client_fds[c].nalCnt == 300) {
                    char end[] = "0\r\n\r\n";
                    send_to_client(c, end, sizeof(end) - 1);
                    free_client(c);
                    break;
                }
            }
        }
        pthread_mutex_unlock(&client_fds_mutex);
    }
}

char any_http_audio(void) {
    char any = 0;
    pthread_mutex_lock(&client_fds_mutex);
    for (unsigned int i = 0; i < HTTP_MAX_CLIENTS; ++i)
        if (client_fds[i].sockFd >= 0 &&
            (client_fds[i].type & (STREAM_MP3 | STREAM_MP4 | STREAM_PCM))) { any = 1; break; }
    pthread_mutex_unlock(&client_fds_mutex);
    return any;
}

void send_mp4_to_client(char index, hal_vidstream *stream, char isH265) {
    char anyClient = 0;
    pthread_mutex_lock(&client_fds_mutex);
    for (unsigned int i = 0; i < HTTP_MAX_CLIENTS; ++i)
        if (client_fds[i].sockFd >= 0 && client_fds[i].type == STREAM_MP4) { anyClient = 1; break; }
    pthread_mutex_unlock(&client_fds_mutex);
    char recording = record_active();

    for (unsigned int i = 0; i < stream->count; ++i) {
        hal_vidpack *pack = &stream->pack[i];
        unsigned char *pack_data = pack->data + pack->offset;
        char gotSlice = 0, isKey = 0;

        if (plat == HAL_PLATFORM_AK)
            mp4_set_timestamp(pack->timestamp);

        for (char j = 0; j < pack->naluCnt; j++) {
            /* nalu[].offset points at the start code, which is 3 or 4
             * bytes depending on the encoder; the muxer wants the payload. */
            unsigned int scLen = (pack_data[pack->nalu[j].offset + 2] == 1) ? 3 : 4;
            if ((pack->nalu[j].type == NalUnitType_SPS || pack->nalu[j].type == NalUnitType_SPS_HEVC)
                && pack->nalu[j].length > scLen && pack->nalu[j].length <= UINT16_MAX)
                mp4_set_sps(pack_data + pack->nalu[j].offset + scLen, pack->nalu[j].length - scLen, isH265);
            else if ((pack->nalu[j].type == NalUnitType_PPS || pack->nalu[j].type == NalUnitType_PPS_HEVC)
                && pack->nalu[j].length <= UINT16_MAX)
                mp4_set_pps(pack_data + pack->nalu[j].offset + scLen, pack->nalu[j].length - scLen, isH265);
            else if (pack->nalu[j].type == NalUnitType_VPS_HEVC && pack->nalu[j].length <= UINT16_MAX)
                mp4_set_vps(pack_data + pack->nalu[j].offset + scLen, pack->nalu[j].length - scLen);
            else if (!anyClient && !recording)
                continue;
            else if (pack->nalu[j].type == NalUnitType_CodedSliceIdr || pack->nalu[j].type == NalUnitType_CodedSliceAux) {
                mp4_set_slice(pack_data + pack->nalu[j].offset + scLen, pack->nalu[j].length - scLen, 1);
                gotSlice = isKey = 1;
            } else if (pack->nalu[j].type == NalUnitType_CodedSliceNonIdr) {
                mp4_set_slice(pack_data + pack->nalu[j].offset + scLen, pack->nalu[j].length - scLen, 0);
                gotSlice = 1;
            }
        }
        /* A pack without a slice leaves no new fragment to hand out */
        if (!gotSlice) continue;
        if (recording) record_fragment(isKey);
        if (!anyClient) continue;

        static enum BufError err;
        char len_buf[50];
        pthread_mutex_lock(&client_fds_mutex);
        for (unsigned int i = 0; i < HTTP_MAX_CLIENTS; ++i) {
            if (client_fds[i].sockFd < 0) continue;
            if (client_fds[i].type != STREAM_MP4) continue;

            if (!client_fds[i].mp4.header_sent) {
                struct BitBuf header_buf;
                err = mp4_get_header(&header_buf);
                chk_err_continue ssize_t len_size =
                    sprintf(len_buf, "%zX\r\n", header_buf.offset);
                if (send_to_client(i, len_buf, len_size) < 0)
                    continue;
                if (send_to_client(i, header_buf.buf, header_buf.offset) < 0)
                    continue;
                if (send_to_client(i, (void*)"\r\n", 2) < 0)
                    continue;

                client_fds[i].mp4.sequence_number = 0;
                client_fds[i].mp4.base_data_offset = header_buf.offset;
                client_fds[i].mp4.base_media_decode_time = 0;
                client_fds[i].mp4.audio_media_decode_time = 0;
                client_fds[i].mp4.header_sent = true;
                client_fds[i].mp4.nals_count = 0;
                client_fds[i].mp4.default_sample_duration =
                    default_sample_size;
            }

            /* buf_moof and the pos_* cursors are shared with the recorder,
               so the whole muxing sequence has to be exclusive. */
            pthread_mutex_lock(&mp4_mtx);
            err = mp4_set_state(&client_fds[i].mp4);
            if (err != BUF_OK) {
                pthread_mutex_unlock(&mp4_mtx);
                continue;
            }
            {
                struct BitBuf moof_buf;
                err = mp4_get_moof(&moof_buf);
                if (err != BUF_OK) {
                    pthread_mutex_unlock(&mp4_mtx);
                    continue;
                }
                ssize_t len_size =
                    sprintf(len_buf, "%zX\r\n", (ssize_t)moof_buf.offset);

                struct iovec iov[3];
                iov[0].iov_base = len_buf;
                iov[0].iov_len = len_size;
                iov[1].iov_base = moof_buf.buf;
                iov[1].iov_len = moof_buf.offset;
                iov[2].iov_base = (void*)"\r\n";
                iov[2].iov_len = 2;

                if (sendv_to_client(i, iov, 3) < 0) {
                    pthread_mutex_unlock(&mp4_mtx);
                    continue;
                }
            }
            {
                struct BitBuf mdat_buf;
                err = mp4_get_mdat(&mdat_buf);
                if (err != BUF_OK) {
                    pthread_mutex_unlock(&mp4_mtx);
                    continue;
                }
                ssize_t len_size =
                    sprintf(len_buf, "%zX\r\n", (ssize_t)mdat_buf.offset);

                struct iovec iov[3];
                iov[0].iov_base = len_buf;
                iov[0].iov_len = len_size;
                iov[1].iov_base = mdat_buf.buf;
                iov[1].iov_len = mdat_buf.offset;
                iov[2].iov_base = (void*)"\r\n";
                iov[2].iov_len = 2;

                if (sendv_to_client(i, iov, 3) < 0) {
                    pthread_mutex_unlock(&mp4_mtx);
                    continue;
                }
            }
            pthread_mutex_unlock(&mp4_mtx);
        }
        pthread_mutex_unlock(&client_fds_mutex);
    }
}

void send_mp3_to_client(char *buf, ssize_t size) {
    pthread_mutex_lock(&client_fds_mutex);
    for (unsigned int i = 0; i < HTTP_MAX_CLIENTS; ++i) {
        if (client_fds[i].sockFd < 0) continue;
        if (client_fds[i].type != STREAM_MP3) continue;

        char len_buf[50];
        ssize_t len_size = sprintf(len_buf, "%zX\r\n", size);
        if (send_to_client(i, len_buf, len_size) < 0)
            continue; // send <SIZE>\r\n
        if (send_to_client(i, buf, size) < 0)
            continue; // send <DATA>
        if (send_to_client(i, "\r\n", 2) < 0)
            continue; // send \r\n
    }
    pthread_mutex_unlock(&client_fds_mutex);
}

void send_pcm_to_client(hal_audframe *frame) {
    pthread_mutex_lock(&client_fds_mutex);
    for (unsigned int i = 0; i < HTTP_MAX_CLIENTS; ++i) {
        if (client_fds[i].sockFd < 0) continue;
        if (client_fds[i].type != STREAM_PCM) continue;

        char len_buf[50];
        ssize_t len_size = sprintf(len_buf, "%zX\r\n", frame->length[0]);
        if (send_to_client(i, len_buf, len_size) < 0)
            continue; // send <SIZE>\r\n
        if (send_to_client(i, frame->data[0], frame->length[0]) < 0)
            continue; // send <DATA>
        if (send_to_client(i, "\r\n", 2) < 0)
            continue; // send \r\n
    }
    pthread_mutex_unlock(&client_fds_mutex);
}

void send_mjpeg_to_client(char index, char *buf, ssize_t size) {
    static char prefix_buf[128];
    ssize_t prefix_size = sprintf(prefix_buf,
        "--boundarydonotcross\r\n"
        "Content-Type:image/jpeg\r\n"
        "Content-Length: %lu\r\n\r\n", size);
    buf[size++] = '\r';
    buf[size++] = '\n';

    pthread_mutex_lock(&client_fds_mutex);
    for (unsigned int i = 0; i < HTTP_MAX_CLIENTS; ++i) {
        if (client_fds[i].sockFd < 0) continue;
        if (client_fds[i].type != STREAM_MJPEG) continue;

        if (send_to_client(i, prefix_buf, prefix_size) < 0)
            continue; // send <SIZE>\r\n
        if (send_to_client(i, buf, size) < 0)
            continue; // send <DATA>\r\n
    }
    pthread_mutex_unlock(&client_fds_mutex);
}

void send_jpeg_to_client(char index, char *buf, ssize_t size) {
    static char prefix_buf[128];
    ssize_t prefix_size = sprintf(
        prefix_buf,
        "HTTP/1.1 200 OK\r\n"
        "Content-Type: image/jpeg\r\n"
        "Content-Length: %lu\r\n"
        "Connection: close\r\n\r\n", size);

    pthread_mutex_lock(&client_fds_mutex);
    for (unsigned int i = 0; i < HTTP_MAX_CLIENTS; ++i) {
        if (client_fds[i].sockFd < 0) continue;
        if (client_fds[i].type != STREAM_JPEG) continue;

        if (send_to_client(i, prefix_buf, prefix_size) < 0)
            continue; // send <SIZE>\r\n
        if (send_to_client(i, buf, size) < 0)
            continue; // send <DATA>\r\n
        free_client(i);
    }
    pthread_mutex_unlock(&client_fds_mutex);
}

struct jpegtask {
    int client_fd;
    uint16_t width;
    uint16_t height;
    uint8_t qfactor;
    uint8_t color2Gray;
};

int send_file(const int client_fd, const char *path) {
    if (!access(path, F_OK)) {
        const char *mime = (path);
        FILE *file = fopen(path, "r");
        if (file == NULL) {
            close_socket_fd(client_fd);
            return EXIT_SUCCESS;
        }
        char header[1024];
        int header_len = sprintf(header,
            "HTTP/1.1 200 OK\r\n"
            "Content-Type: %s\r\n"
            "Transfer-Encoding: chunked\r\n"
            "Connection: keep-alive\r\n\r\n", mime);
        send_to_fd(client_fd, header, header_len); // zero ending string!
        const int buf_size = 1024;
        char buf[buf_size + 2];
        char len_buf[50];
        ssize_t len_size;
        while (1) {
            ssize_t size = fread(buf, sizeof(char), buf_size, file);
            if (size <= 0) break;
            len_size = sprintf(len_buf, "%zX\r\n", size);
            buf[size++] = '\r';
            buf[size++] = '\n';
            if (send_to_fd(client_fd, len_buf, len_size) < 0) break;
            if (send_to_fd(client_fd, buf, size) < 0) break;
        }
        char end[] = "0\r\n\r\n";
        send_to_fd(client_fd, end, sizeof(end) - 1);
        fclose(file);
        close_socket_fd(client_fd);
        return EXIT_FAILURE;
    }

    send_http_error(client_fd, 404);
    return EXIT_FAILURE;
}

void send_binary(const int fd, const char *data, const long size) {
    char *buf;
    int buf_len = asprintf(&buf,
        "HTTP/1.1 200 OK\r\n" \
        "Content-Type: application/octet-stream\r\n" \
        "Content-Length: %zu\r\n" \
        "Connection: close\r\n\r\n", size);
    send_to_fd(fd, buf, buf_len);
    send_to_fd(fd, (char*)data, size);
    send_to_fd(fd, "\r\n", 2);
    close_socket_fd(fd);
    free(buf);
}

void send_html(const int fd, const char *data) {
    char *buf;
    int buf_len = asprintf(&buf,
        "HTTP/1.1 200 OK\r\n" \
        "Content-Type: text/html\r\n" \
        "Content-Length: %zu\r\n" \
        "Connection: close\r\n" \
        "\r\n%s", strlen(data), data);
    buf[buf_len++] = 0;
    send_and_close(fd, buf, buf_len);
    free(buf);
}

char *request_header(const char *name) {
    http_header_t *h = http_headers;
    for (; h->name; h++)
        if (!strcasecmp(h->name, name))
            return h->value;
    return NULL;
}

http_header_t *request_headers(void) { return http_headers; }

/* Recorded clips are sent from their own thread: the server loop handles one
 * request at a time and a download would freeze everything else. */
#define CLIP_MAX_SENDERS 3
typedef struct {
    int fd;
    char path[RECORD_PATH_MAX];
    char name[128];
    long long first, last;
    bool hasRange, download;
} clip_job_t;

static int clip_senders;
static pthread_mutex_t clip_mutex = PTHREAD_MUTEX_INITIALIZER;

static void *send_clip_thread(void *arg) {
    clip_job_t *job = arg;
    char *buf = malloc(32768), header[512];
    /* Only regular files in the recording directory, not what a link points at */
    int clipFd = open(job->path, O_RDONLY | O_NOFOLLOW);
    FILE *file = clipFd >= 0 ? fdopen(clipFd, "rb") : NULL;
    struct stat st;

    if (clipFd >= 0 && !file) close(clipFd);

    if (!buf || !file || fstat(fileno(file), &st) || !S_ISREG(st.st_mode)) {
        send_http_error(job->fd, 404);
        goto done;
    }

    long long size = st.st_size, first = 0, last = size - 1;
    if (job->hasRange) {
        if (job->first < 0) {
            /* "bytes=-N": the last N bytes */
            first = size + job->first < 0 ? 0 : size + job->first;
        } else {
            first = job->first;
            if (job->last >= 0 && job->last < last) last = job->last;
        }
        if (first > last || first >= size) {
            int len = sprintf(header, "HTTP/1.1 416 Range Not Satisfiable\r\n"
                "Content-Range: bytes */%lld\r\nContent-Length: 0\r\nConnection: close\r\n\r\n", size);
            send_to_fd(job->fd, header, len);
            close_socket_fd(job->fd);
            goto done;
        }
    }

    int len = sprintf(header, "HTTP/1.1 %s\r\n"
        "Content-Type: video/mp4\r\n"
        "Accept-Ranges: bytes\r\n"
        "Content-Length: %lld\r\n", job->hasRange ? "206 Partial Content" : "200 OK", last - first + 1);
    if (job->hasRange)
        len += sprintf(header + len, "Content-Range: bytes %lld-%lld/%lld\r\n", first, last, size);
    if (job->download)
        len += sprintf(header + len, "Content-Disposition: attachment; filename=\"%s\"\r\n", job->name);
    len += sprintf(header + len, "Connection: close\r\n\r\n");

    if (send(job->fd, header, len, MSG_NOSIGNAL) == len && !fseeko(file, first, SEEK_SET)) {
        long long left = last - first + 1;
        while (left > 0 && keepRunning) {
            size_t got = fread(buf, 1, left > 32768 ? 32768 : (size_t)left, file);
            if (!got) break;
            size_t sent = 0;
            while (sent < got) {
                ssize_t n = send(job->fd, buf + sent, got - sent, MSG_NOSIGNAL);
                if (n <= 0) goto closing;
                sent += n;
            }
            left -= got;
        }
    }
closing:
    close_socket_fd(job->fd);
done:
    if (file) fclose(file);
    free(buf);
    free(job);
    pthread_mutex_lock(&clip_mutex);
    clip_senders--;
    pthread_mutex_unlock(&clip_mutex);
    return NULL;
}

static int clip_name_cmp(const void *a, const void *b) {
    /* Names carry the start time: descending order puts the newest first */
    return strcmp(*(char *const *)b, *(char *const *)a);
}

/* Lists the recorded clips as JSON, newest first. */
static void send_clip_list(int fd) {
    char **names = NULL, *out;
    size_t count = 0, cap = 0, len;
    struct dirent *ent;
    DIR *dir = opendir(app_config.record_path);

    while (dir && (ent = readdir(dir))) {
        size_t n = strlen(ent->d_name);
        if (n < 5 || n > 120 || strcmp(ent->d_name + n - 4, ".mp4")) continue;
        /* Names go into JSON as they are: skip any that would need escaping */
        bool plain = true;
        for (const char *c = ent->d_name; *c && plain; c++)
            plain = (unsigned char)*c >= 0x20 && *c != '"' && *c != '\\';
        if (!plain) continue;
        if (count == cap) {
            char **grown = realloc(names, (cap = cap ? cap * 2 : 64) * sizeof(*names));
            if (!grown) break;
            names = grown;
        }
        if (!(names[count] = strdup(ent->d_name))) break;
        count++;
    }
    if (dir) closedir(dir);
    if (count) qsort(names, count, sizeof(*names), clip_name_cmp);

    if (!(out = malloc(256 + count * 176))) {
        send_http_error(fd, 500);
        goto cleanup;
    }
    len = sprintf(out, "HTTP/1.1 200 OK\r\n"
        "Content-Type: application/json;charset=UTF-8\r\n"
        "Connection: close\r\n\r\n{\"recording\":%s,\"files\":[", record_active() ? "true" : "false");
    for (size_t i = 0; i < count; i++) {
        char path[RECORD_PATH_MAX];
        struct stat st;
        snprintf(path, sizeof(path), "%s/%s", app_config.record_path, names[i]);
        if (lstat(path, &st) || !S_ISREG(st.st_mode)) continue;
        len += sprintf(out + len, "%s{\"name\":\"%s\",\"size\":%lld}",
            out[len - 1] == '[' ? "" : ",", names[i], (long long)st.st_size);
    }
    len += sprintf(out + len, "]}");
    send_and_close(fd, out, len);
    free(out);
cleanup:
    for (size_t i = 0; i < count; i++) free(names[i]);
    free(names);
}

/* Hands a clip over to a sender thread; the socket then belongs to it. */
static void send_clip(http_request_t *req, const char *name) {
    clip_job_t *job;
    pthread_attr_t attr;
    pthread_t tid;
    char *range = request_header("Range");
    size_t n = strlen(name);

    if (n < 5 || n >= sizeof(job->name) || strchr(name, '/') || strcmp(name + n - 4, ".mp4")) {
        send_http_error(req->clntFd, 404);
        return;
    }

    pthread_mutex_lock(&clip_mutex);
    bool busy = clip_senders >= CLIP_MAX_SENDERS;
    if (!busy) clip_senders++;
    pthread_mutex_unlock(&clip_mutex);
    if (busy || !(job = calloc(1, sizeof(*job)))) {
        if (!busy) {
            pthread_mutex_lock(&clip_mutex);
            clip_senders--;
            pthread_mutex_unlock(&clip_mutex);
        }
        send_http_error(req->clntFd, 500);
        return;
    }

    job->fd = req->clntFd;
    strcpy(job->name, name);
    snprintf(job->path, sizeof(job->path), "%s/%s", app_config.record_path, name);
    job->download = req->query && strstr(req->query, "download");
    job->last = -1;
    if (range && !strncmp(range, "bytes=", 6)) {
        char *end;
        range += 6;
        if (*range == '-') {
            job->first = -strtoll(range + 1, &end, 10);
            job->hasRange = end != range + 1 && job->first < 0;
        } else {
            job->first = strtoll(range, &end, 10);
            job->hasRange = end != range && *end == '-';
            if (job->hasRange && end[1] >= '0' && end[1] <= '9') job->last = strtoll(end + 1, NULL, 10);
        }
    }

    /* Long transfers over WiFi: a stalled client is dropped after 20 s */
    struct timeval tv = { .tv_sec = 20, .tv_usec = 0 };
    setsockopt(job->fd, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof(tv));

    pthread_attr_init(&attr);
    pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);
    pthread_attr_setstacksize(&attr, 64 * 1024);
    if (pthread_create(&tid, &attr, send_clip_thread, job)) {
        pthread_mutex_lock(&clip_mutex);
        clip_senders--;
        pthread_mutex_unlock(&clip_mutex);
        free(job);
        send_http_error(req->clntFd, 500);
    }
    pthread_attr_destroy(&attr);
}


void parse_request(http_request_t *req) {
    struct sockaddr_in client_sock;
    socklen_t client_sock_len = sizeof(client_sock);
    memset(&client_sock, 0, client_sock_len);

    getpeername(req->clntFd,
        (struct sockaddr *)&client_sock, &client_sock_len);
    char *client_ip = inet_ntoa(client_sock.sin_addr);

    if (!EMPTY(*app_config.web_whitelist)) {
        for (int i = 0; app_config.web_whitelist[i] && *app_config.web_whitelist[i]; i++)
            if (ip_in_cidr(client_ip, app_config.web_whitelist[i])) goto grant_access;
        close_socket_fd(req->clntFd);
        req->clntFd = -1;
        return;
    }

grant_access:
    if (req->total <= 0) return;

    req->input[req->total] = '\0';
    char *state = NULL;
    req->method = strtok_r(req->input, " \t\r\n", &state);
    req->uri = strtok_r(NULL, " \t", &state);
    req->prot = strtok_r(NULL, " \t\r\n", &state);

    if (!req->method || !req->uri || !req->prot) {
        close_socket_fd(req->clntFd);
        req->clntFd = -1;
        return;
    }

    HAL_INFO("server", "\x1b[32mNew request: (%s) %s\n"
        "         Received from: %s\x1b[0m\n",
        req->method, req->uri, client_ip);

    if (req->query = strchr(req->uri, '?'))
        *req->query++ = '\0';
    else
        req->query = NULL;

    if (req->uri && strstr(req->uri, "..")) {
        close_socket_fd(req->clntFd);
        req->clntFd = -1;
        return;
    }

    http_header_t *h = http_headers;
    while (h < http_headers + 16) {
        char *k, *v, *e;
        if (!(k = strtok_r(NULL, "\r\n: \t", &state)))
            break;
        v = strtok_r(NULL, "\r\n", &state);
        if (!v) break;
        while (*v && *v == ' ' && v++);
        h->name = k;
        h++->value = v;
#ifdef DEBUG_HTTP
        fprintf(stderr, "         (H) %s: %s\n", k, v);
#endif
        if (state >= req->input + req->header_len) break;
        e = v + 1 + strlen(v);
        if (e[1] == '\r' && e[2] == '\n')
            break;
    }

    req->payload = req->input + req->header_len;
}

void respond_request(http_request_t *req) {
    char response[8192] = {0};
    int respLen = 0;

    if (req->clntFd < 0) return;

    if (!EQUALS(req->method, "GET") && !EQUALS(req->method, "POST")) {
        send_http_error(req->clntFd, 405);
        return;
    }

    if (app_config.onvif_enable && STARTS_WITH(req->uri, "/onvif")) {
        char *path = req->uri + 6;
        if (*path == '/') path++;

        if (!EQUALS(req->method, "POST")) {
            send_http_error(req->clntFd, 405);
            return;
        }

        char *action = onvif_extract_soap_action(req->payload);
        HAL_INFO("onvif", "\x1b[32mAction: %s\x1b[0m\n", action);
        respLen = sizeof(response);

        if (app_config.onvif_enable_auth && !onvif_is_preauth_action(action) &&
            !onvif_validate_soap_auth(req->payload)) {
            respLen = sprintf(response,
                "HTTP/1.1 401 Unauthorized\r\n"
                "Content-Type: text/plain\r\n"
                "WWW-Authenticate: Digest realm=\"Access the camera services\"\r\n"
                "Connection: close\r\n\r\n%s",
                badauthxml);
            send_and_close(req->clntFd, response, respLen);
            return;
        }

        if (EQUALS(path, "device_service")) {
            if (EQUALS(action, "GetCapabilities")) {
                onvif_respond_capabilities((char*)response, &respLen);
                send_and_close(req->clntFd, response, respLen);
                return;
            } else if (EQUALS(action, "GetDeviceInformation")) {
                onvif_respond_deviceinfo((char*)response, &respLen);
                send_and_close(req->clntFd, response, respLen);
                return;
            } else if (EQUALS(action, "GetSystemDateAndTime")) {
                onvif_respond_systemtime((char*)response, &respLen);
                send_and_close(req->clntFd, response, respLen);
                return;
            }
        } else if (EQUALS(path, "media_service")) {
            if (EQUALS(action, "GetProfiles")) {
                onvif_respond_mediaprofiles((char*)response, &respLen);
                send_and_close(req->clntFd, response, respLen);
                return;
            } else if (EQUALS(action, "GetSnapshotUri")) {
                onvif_respond_snapshot((char*)response, &respLen);
                send_and_close(req->clntFd, response, respLen);
                return;
            } else if (EQUALS(action, "GetStreamUri")) {
                onvif_respond_stream((char*)response, &respLen);
                send_and_close(req->clntFd, response, respLen);
                return;
            } else if (EQUALS(action, "GetVideoSources")) {
                onvif_respond_videosources((char*)response, &respLen);
                send_and_close(req->clntFd, response, respLen);
                return;
            }
        }

        if (!EMPTY(action))
            HAL_WARNING("server", "Unknown ONVIF request: %s->%s\n", path, action);
        send_http_error(req->clntFd, 501);
        return;
    }

    if (app_config.web_enable_auth) {
        bool should_skip_auth = false;

        if (app_config.web_auth_skiplocal) {
            struct sockaddr_in client_sock;
            socklen_t client_sock_len = sizeof(client_sock);
            memset(&client_sock, 0, client_sock_len);

            if (getpeername(req->clntFd, (struct sockaddr *)&client_sock, &client_sock_len) == 0) {
                char *client_ip = inet_ntoa(client_sock.sin_addr);
                should_skip_auth = is_local_address(client_ip);
            }
        }

        if (!should_skip_auth) {
            char *auth = request_header("Authorization");
            char cred[66], valid[256];

            strcpy(cred, app_config.web_auth_user);
            strcpy(cred + strlen(app_config.web_auth_user), ":");
            strcpy(cred + strlen(app_config.web_auth_user) + 1, app_config.web_auth_pass);
            strcpy(valid, "Basic ");
            base64_encode(valid + 6, cred, strlen(cred));

            bool granted = EQUALS(app_config.web_auth_pass, "@system") ?
                (auth && STARTS_WITH(auth, "Basic ") && sysauth_check_basic(auth + 6)) :
                (auth && EQUALS(auth, valid));
            if (!granted) {
                respLen = sprintf(response,
                    "HTTP/1.1 401 Unauthorized\r\n"
                    "Content-Type: text/plain\r\n"
                    "WWW-Authenticate: Basic realm=\"Access the camera services\"\r\n"
                    "Connection: close\r\n\r\n");
                send_and_close(req->clntFd, response, respLen);
                return;
            }
        }
    }

    if (EQUALS(req->uri, "/exit")) {
        respLen = sprintf(response,
            "HTTP/1.1 200 OK\r\n"
            "Connection: close\r\n\r\n"
            "Closing...");
        send_and_close(req->clntFd, response, respLen);
        keepRunning = 0;
        graceful = 1;
        return;
    }

    if (EQUALS(req->uri, "/") || EQUALS(req->uri, "/index.htm") || EQUALS(req->uri, "/index.html")) {
        send_html(req->clntFd, indexhtml);
        return;
    }

    if (app_config.audio_enable && EQUALS(req->uri, "/audio.mp3")) {
        respLen = sprintf(response,
            "HTTP/1.1 200 OK\r\n"
            "Content-Type: audio/mpeg\r\n"
            "Transfer-Encoding: chunked\r\n"
            "Connection: keep-alive\r\n\r\n");
        send_to_fd(req->clntFd, response, respLen);
        pthread_mutex_lock(&client_fds_mutex);
        for (uint32_t i = 0; i < HTTP_MAX_CLIENTS; ++i)
            if (client_fds[i].sockFd < 0) {
                client_fds[i].sockFd = req->clntFd;
                client_fds[i].type = STREAM_MP3;
                break;
            }
        pthread_mutex_unlock(&client_fds_mutex);
        return;
    }

    if (app_config.audio_enable && EQUALS(req->uri, "/audio.pcm")) {
        respLen = sprintf(response,
            "HTTP/1.1 200 OK\r\n"
            "Content-Type: audio/pcm\r\n"
            "Transfer-Encoding: chunked\r\n"
            "Connection: keep-alive\r\n\r\n");
        send_to_fd(req->clntFd, response, respLen);
        pthread_mutex_lock(&client_fds_mutex);
        for (uint32_t i = 0; i < HTTP_MAX_CLIENTS; ++i)
            if (client_fds[i].sockFd < 0) {
                client_fds[i].sockFd = req->clntFd;
                client_fds[i].type = STREAM_PCM;
                break;
            }
        pthread_mutex_unlock(&client_fds_mutex);
        return;
    }

    if ((!app_config.mp4_codecH265 && EQUALS(req->uri, "/video.264")) ||
        (app_config.mp4_codecH265 && EQUALS(req->uri, "/video.265"))) {
        request_idr();
        respLen = sprintf(response,
            "HTTP/1.1 200 OK\r\n"
            "Content-Type: application/octet-stream\r\n"
            "Transfer-Encoding: chunked\r\n"
            "Connection: keep-alive\r\n\r\n");
        send_to_fd(req->clntFd, response, respLen);
        pthread_mutex_lock(&client_fds_mutex);
        for (uint32_t i = 0; i < HTTP_MAX_CLIENTS; ++i)
            if (client_fds[i].sockFd < 0) {
                client_fds[i].sockFd = req->clntFd;
                client_fds[i].type = STREAM_H26X;
                client_fds[i].nalCnt = 0;
                break;
            }
        pthread_mutex_unlock(&client_fds_mutex);
        return;
    }

    if (app_config.mp4_enable && EQUALS(req->uri, "/video.mp4")) {
        request_idr();
        respLen = sprintf(response,
            "HTTP/1.1 200 OK\r\n"
            "Content-Type: video/mp4\r\n"
            "Transfer-Encoding: chunked\r\n"
            "Connection: keep-alive\r\n\r\n");
        send_to_fd(req->clntFd, response, respLen);
        pthread_mutex_lock(&client_fds_mutex);
        for (uint32_t i = 0; i < HTTP_MAX_CLIENTS; ++i)
            if (client_fds[i].sockFd < 0) {
                client_fds[i].sockFd = req->clntFd;
                client_fds[i].type = STREAM_MP4;
                client_fds[i].mp4.header_sent = false;
                break;
            }
        pthread_mutex_unlock(&client_fds_mutex);
        return;
    }

    if (app_config.mjpeg_enable && EQUALS(req->uri, "/mjpeg")) {
        respLen = sprintf(response,
            "HTTP/1.0 200 OK\r\n"
            "Cache-Control: no-cache\r\n"
            "Pragma: no-cache\r\n"
            "Connection: close\r\n"
            "Content-Type: multipart/x-mixed-replace; boundary=boundarydonotcross\r\n\r\n");
        send_to_fd(req->clntFd, response, respLen);
        pthread_mutex_lock(&client_fds_mutex);
        for (uint32_t i = 0; i < HTTP_MAX_CLIENTS; ++i)
            if (client_fds[i].sockFd < 0) {
                client_fds[i].sockFd = req->clntFd;
                client_fds[i].type = STREAM_MJPEG;
                break;
            }
        pthread_mutex_unlock(&client_fds_mutex);
        return;
    }

    if (app_config.jpeg_enable && STARTS_WITH(req->uri, "/image.jpg")) {
        {
            struct jpegtask *task = malloc(sizeof(struct jpegtask));
            task->client_fd = req->clntFd;
            task->width = app_config.jpeg_width;
            task->height = app_config.jpeg_height;
            task->qfactor = app_config.jpeg_qfactor;
            task->color2Gray = 0;

            if (req->query) {
                char *remain;
                while (req->query) {
                    char *value = split(&req->query, "&");
                    if (!value || !*value) continue;
                    char *key = split(&value, "=");
                    if (!key || !*key || !value || !*value) continue;
                    if (EQUALS(key, "width")) {
                        short result = strtol(value, &remain, 10);
                        if (remain != value)
                            task->width = result;
                    }
                    else if (EQUALS(key, "height")) {
                        short result = strtol(value, &remain, 10);
                        if (remain != value)
                            task->height = result;
                    }
                    else if (EQUALS(key, "qfactor")) {
                        short result = strtol(value, &remain, 10);
                        if (remain != value)
                            task->qfactor = result;
                    }
                    else if (EQUALS(key, "color2gray") || EQUALS(key, "gray")) {
                        if (EQUALS_CASE(value, "true") || EQUALS(value, "1"))
                            task->color2Gray = 1;
                        else if (EQUALS_CASE(value, "false") || EQUALS(value, "0"))
                            task->color2Gray = 0;
                    }
                }
            }

            hal_jpegdata jpeg = {0};
            if (jpeg_get(task->width, task->height, task->qfactor,
                    task->color2Gray, &jpeg)) {
                send_http_error(req->clntFd, 503);
                free(task);
                return;
            }

            char header[256];
            int header_len = sprintf(header,
                "HTTP/1.1 200 OK\r\n"
                "Content-Type: image/jpeg\r\n"
                "Content-Length: %u\r\n"
                "Connection: close\r\n\r\n",
                (unsigned)jpeg.jpegSize);
            send_to_fd(req->clntFd, header, header_len);
            send_to_fd(req->clntFd, jpeg.data, jpeg.jpegSize);
            free(jpeg.data);
            free(task);
            close_socket_fd(req->clntFd);
            req->clntFd = -1;
            return;
        }
        return;
    }

    if (EQUALS(req->uri, "/api/audio")) {
        if (req->query) {
            while (req->query) {
                char *value = split(&req->query, "&");
                if (!value || !*value) continue;
                unescape_uri(value);
                char *key = split(&value, "=");
                if (!key || !*key || !value || !*value) continue;
                if (EQUALS(key, "bitrate")) {
                    long result;
                    if (parse_ranged(value, 32, 320, &result))
                        app_config.audio_bitrate = (unsigned int)result;
                } else if (EQUALS(key, "enable")) {
                    if (EQUALS_CASE(value, "true") || EQUALS(value, "1"))
                        app_config.audio_enable = 1;
                    else if (EQUALS_CASE(value, "false") || EQUALS(value, "0"))
                        app_config.audio_enable = 0;
                } else if (EQUALS(key, "gain")) {
                    long result;
                    if (parse_ranged(value, -60, 30, &result))
                        app_config.audio_gain = (int)result;
                } else if (EQUALS(key, "srate")) {
                    long result;
                    if (parse_ranged(value, 8000, 96000, &result))
                        app_config.audio_srate = (unsigned int)result;
                }
            }

            media_audio_disable();
            if (app_config.audio_enable) media_audio_enable();
        }

        respLen = sprintf(response,
            "HTTP/1.1 200 OK\r\n"
            "Content-Type: application/json;charset=UTF-8\r\n"
            "Connection: close\r\n"
            "\r\n"
            "{\"enable\":%s,\"bitrate\":%d,\"gain\":%d,\"srate\":%d}",
            app_config.audio_enable ? "true" : "false",
            app_config.audio_bitrate, app_config.audio_gain, app_config.audio_srate);
        send_and_close(req->clntFd, response, respLen);
        return;
    }

    if (EQUALS(req->uri, "/api/cmd")) {
        int result = -1;
        if (req->query) {
            char *remain;
            while (req->query) {
                char *value = split(&req->query, "&");
                if (!value || !*value) continue;
                unescape_uri(value);
                char *key = split(&value, "=");
                if (!key || !*key) continue;
                if (EQUALS(key, "save")) {
                    result = app_config_save();
                    if (!result)
                        HAL_INFO("server", "Configuration saved!\n");
                    else
                        HAL_WARNING("server", "Failed to save configuration!\n");
                    break;
                } else if (EQUALS(key, "restart")) {
                    kill(getpid(), SIGHUP);
                    result = 0;
                    break;
                }
            }
        }

        respLen = sprintf(response,
            "HTTP/1.1 200 OK\r\n"
            "Content-Type: application/json;charset=UTF-8\r\n"
            "Connection: close\r\n"
            "\r\n"
            "{\"code\":%d}", result);
        send_and_close(req->clntFd, response, respLen);
        return;
    }

    if (EQUALS(req->uri, "/api/isp")) {
        if (req->query) {
            char *remain;
            while (req->query) {
                char *value = split(&req->query, "&");
                if (!value || !*value) continue;
                unescape_uri(value);
                char *key = split(&value, "=");
                if (!key || !*key || !value || !*value) continue;
                if (EQUALS(key, "mirror")) {
                    if (EQUALS_CASE(value, "true") || EQUALS(value, "1"))
                        app_config.mirror = 1;
                    else if (EQUALS_CASE(value, "false") || EQUALS(value, "0"))
                        app_config.mirror = 0;
                } else if (EQUALS(key, "flip")) {
                    if (EQUALS_CASE(value, "true") || EQUALS(value, "1"))
                        app_config.flip = 1;
                    else if (EQUALS_CASE(value, "false") || EQUALS(value, "0"))
                        app_config.flip = 0;
                } else if (EQUALS(key, "antiflicker")) {
                    app_config.antiflicker = strtol(value, &remain, 10);
                }
            }
        }
        respLen = sprintf(response,
            "HTTP/1.1 200 OK\r\n"
            "Content-Type: application/json;charset=UTF-8\r\n"
            "Connection: close\r\n"
            "\r\n"
            "{\"mirror\":%s, \"flip\":%s, \"antiflicker\":%d}",
            app_config.mirror ? "true" : "false", app_config.flip ? "true" : "false",
            app_config.antiflicker);
        send_and_close(req->clntFd, response, respLen);
        return;
    }

    if (EQUALS(req->uri, "/api/jpeg")) {
        if (req->query) {
            char *remain;
            while (req->query) {
                char *value = split(&req->query, "&");
                if (!value || !*value) continue;
                unescape_uri(value);
                char *key = split(&value, "=");
                if (!key || !*key || !value || !*value) continue;
                if (EQUALS(key, "width")) {
                    short result = strtol(value, &remain, 10);
                    if (remain != value)
                        app_config.jpeg_width = result;
                } else if (EQUALS(key, "height")) {
                    short result = strtol(value, &remain, 10);
                    if (remain != value)
                        app_config.jpeg_height = result;
                } else if (EQUALS(key, "qfactor")) {
                    short result = strtol(value, &remain, 10);
                    if (remain != value)
                        app_config.jpeg_qfactor = result;
                }
            }

            jpeg_deinit();
            if (app_config.jpeg_enable) jpeg_init();
        }

        respLen = sprintf(response,
            "HTTP/1.1 200 OK\r\n"
            "Content-Type: application/json;charset=UTF-8\r\n"
            "Connection: close\r\n"
            "\r\n"
            "{\"enable\":%s,\"width\":%d,\"height\":%d,\"qfactor\":%d}",
            app_config.jpeg_enable ? "true" : "false",
            app_config.jpeg_width, app_config.jpeg_height, app_config.jpeg_qfactor);
        send_and_close(req->clntFd, response, respLen);
        return;
    }

    if (EQUALS(req->uri, "/api/mjpeg")) {
        if (req->query) {
            char *remain;
            while (req->query) {
                char *value = split(&req->query, "&");
                if (!value || !*value) continue;
                unescape_uri(value);
                char *key = split(&value, "=");
                if (!key || !*key || !value || !*value) continue;
                if (EQUALS(key, "enable")) {
                    if (EQUALS_CASE(value, "true") || EQUALS(value, "1"))
                        app_config.mjpeg_enable = 1;
                    else if (EQUALS_CASE(value, "false") || EQUALS(value, "0"))
                        app_config.mjpeg_enable = 0;
                } else if (EQUALS(key, "width")) {
                    short result = strtol(value, &remain, 10);
                    if (remain != value)
                        app_config.mjpeg_width = result;
                } else if (EQUALS(key, "height")) {
                    short result = strtol(value, &remain, 10);
                    if (remain != value)
                        app_config.mjpeg_height = result;
                } else if (EQUALS(key, "fps")) {
                    short result = strtol(value, &remain, 10);
                    if (remain != value)
                        app_config.mjpeg_fps = result;
                } else if (EQUALS(key, "bitrate")) {
                    short result = strtol(value, &remain, 10);
                    if (remain != value)
                        app_config.mjpeg_bitrate = result;
                } else if (EQUALS(key, "qfactor")) {
                    short result = strtol(value, &remain, 10);
                    if (remain != value)
                        app_config.mjpeg_qfactor = result;
                } else if (EQUALS(key, "mode")) {
                    if (EQUALS_CASE(value, "CBR"))
                        app_config.mjpeg_mode = HAL_VIDMODE_CBR;
                    else if (EQUALS_CASE(value, "VBR"))
                        app_config.mjpeg_mode = HAL_VIDMODE_VBR;
                    else if (EQUALS_CASE(value, "QP"))
                        app_config.mjpeg_mode = HAL_VIDMODE_QP;
                }
            }

            media_mjpeg_disable();
            if (app_config.mjpeg_enable) media_mjpeg_enable();
        }

        char mode[5] = "\0";
        switch (app_config.mjpeg_mode) {
            case HAL_VIDMODE_CBR: strcpy(mode, "CBR"); break;
            case HAL_VIDMODE_VBR: strcpy(mode, "VBR"); break;
            case HAL_VIDMODE_QP: strcpy(mode, "QP"); break;
        }
        respLen = sprintf(response,
            "HTTP/1.1 200 OK\r\n"
            "Content-Type: application/json;charset=UTF-8\r\n"
            "Connection: close\r\n"
            "\r\n"
            "{\"enable\":%s,\"width\":%d,\"height\":%d,\"fps\":%d,\"mode\":\"%s\","
            "\"bitrate\":%d,\"qfactor\":%d}",
            app_config.mjpeg_enable ? "true" : "false",
            app_config.mjpeg_width, app_config.mjpeg_height, app_config.mjpeg_fps, mode,
            app_config.mjpeg_bitrate, app_config.mjpeg_qfactor);
        send_and_close(req->clntFd, response, respLen);
        return;
    }

    if (EQUALS(req->uri, "/api/mp4")) {
        if (req->query) {
            char *remain;
            while (req->query) {
                char *value = split(&req->query, "&");
                if (!value || !*value) continue;
                unescape_uri(value);
                char *key = split(&value, "=");
                if (!key || !*key || !value || !*value) continue;
                if (EQUALS(key, "enable")) {
                    if (EQUALS_CASE(value, "true") || EQUALS(value, "1"))
                        app_config.mp4_enable = 1;
                    else if (EQUALS_CASE(value, "false") || EQUALS(value, "0"))
                        app_config.mp4_enable = 0;
                } else if (EQUALS(key, "width")) {
                    short result = strtol(value, &remain, 10);
                    if (remain != value)
                        app_config.mp4_width = result;
                } else if (EQUALS(key, "height")) {
                    short result = strtol(value, &remain, 10);
                    if (remain != value)
                        app_config.mp4_height = result;
                } else if (EQUALS(key, "fps")) {
                    short result = strtol(value, &remain, 10);
                    if (remain != value)
                        app_config.mp4_fps = result;
                } else if (EQUALS(key, "bitrate")) {
                    short result = strtol(value, &remain, 10);
                    if (remain != value)
                        app_config.mp4_bitrate = result;
                } else if (EQUALS(key, "h265")) {
                    if (EQUALS_CASE(value, "true") || EQUALS(value, "1"))
                        app_config.mp4_codecH265 = 1;
                    else if (EQUALS_CASE(value, "false") || EQUALS(value, "0"))
                        app_config.mp4_codecH265 = 0;
                } else if (EQUALS(key, "mode")) {
                    if (EQUALS_CASE(value, "CBR"))
                        app_config.mp4_mode = HAL_VIDMODE_CBR;
                    else if (EQUALS_CASE(value, "VBR"))
                        app_config.mp4_mode = HAL_VIDMODE_VBR;
                    else if (EQUALS_CASE(value, "QP"))
                        app_config.mp4_mode = HAL_VIDMODE_QP;
                    else if (EQUALS_CASE(value, "ABR"))
                        app_config.mp4_mode = HAL_VIDMODE_ABR;
                    else if (EQUALS_CASE(value, "AVBR"))
                        app_config.mp4_mode = HAL_VIDMODE_AVBR;
                } else if (EQUALS(key, "profile")) {
                    if (EQUALS_CASE(value, "BP") || EQUALS_CASE(value, "BASELINE"))
                        app_config.mp4_profile = HAL_VIDPROFILE_BASELINE;
                    else if (EQUALS_CASE(value, "MP") || EQUALS_CASE(value, "MAIN"))
                        app_config.mp4_profile = HAL_VIDPROFILE_MAIN;
                    else if (EQUALS_CASE(value, "HP") || EQUALS_CASE(value, "HIGH"))
                        app_config.mp4_profile = HAL_VIDPROFILE_HIGH;
                }
            }

            media_mp4_disable();
            if (app_config.mp4_enable) media_mp4_enable();
        }

        char h265[6] = "false";
        char mode[5] = "\0";
        char profile[3] = "\0";
        if (app_config.mp4_codecH265)
            strcpy(h265, "true");
        switch (app_config.mp4_mode) {
            case HAL_VIDMODE_CBR: strcpy(mode, "CBR"); break;
            case HAL_VIDMODE_VBR: strcpy(mode, "VBR"); break;
            case HAL_VIDMODE_QP: strcpy(mode, "QP"); break;
            case HAL_VIDMODE_ABR: strcpy(mode, "ABR"); break;
            case HAL_VIDMODE_AVBR: strcpy(mode, "AVBR"); break;
        }
        switch (app_config.mp4_profile) {
            case HAL_VIDPROFILE_BASELINE: strcpy(profile, "BP"); break;
            case HAL_VIDPROFILE_MAIN: strcpy(profile, "MP"); break;
            case HAL_VIDPROFILE_HIGH: strcpy(profile, "HP"); break;
        }
        respLen = sprintf(response,
            "HTTP/1.1 200 OK\r\n"
            "Content-Type: application/json;charset=UTF-8\r\n"
            "Connection: close\r\n"
            "\r\n"
            "{\"enable\":%s,\"width\":%d,\"height\":%d,\"fps\":%d,\"gop\":%d,"
            "\"h265\":%s,\"mode\":\"%s\",\"profile\":\"%s\",\"bitrate\":%d}",
            app_config.mp4_enable ? "true" : "false", app_config.mp4_width, app_config.mp4_height,
            app_config.mp4_fps, app_config.mp4_gop, h265, mode, profile, app_config.mp4_bitrate);
        send_and_close(req->clntFd, response, respLen);
        return;
    }

    if (EQUALS(req->uri, "/api/rtsp")) {
        if (req->query) {
            char *remain;
            while (req->query) {
                char *value = split(&req->query, "&");
                if (!value || !*value) continue;
                unescape_uri(value);
                char *key = split(&value, "=");
                if (!key || !*key || !value || !*value) continue;
                if (EQUALS(key, "enable"))
                    app_config.rtsp_enable = EQUALS_CASE(value, "true") || EQUALS(value, "1");
                else if (EQUALS(key, "enable_auth"))
                    app_config.rtsp_enable_auth = EQUALS_CASE(value, "true") || EQUALS(value, "1");
                else if (EQUALS(key, "port")) {
                    long result;
                    if (parse_ranged(value, 1, 65535, &result))
                        app_config.rtsp_port = result;
                } else if (EQUALS(key, "auth_user"))
                    strncpy(app_config.rtsp_auth_user, value, sizeof(app_config.rtsp_auth_user) - 1);
                else if (EQUALS(key, "auth_pass"))
                    strncpy(app_config.rtsp_auth_pass, value, sizeof(app_config.rtsp_auth_pass) - 1);
                else if (EQUALS(key, "audio_codec")) {
                    if (EQUALS(value, "pcma") || EQUALS(value, "pcmu") || EQUALS(value, "mp3")) {
                        strncpy(app_config.rtsp_audio_codec, value,
                            sizeof(app_config.rtsp_audio_codec) - 1);
                        app_config.rtsp_audio_codec[sizeof(app_config.rtsp_audio_codec) - 1] = 0;
                    }
                }
            }
        }
        char esc_user[sizeof(app_config.rtsp_auth_user) * 6 + 1];
        char esc_codec[sizeof(app_config.rtsp_audio_codec) * 6 + 1];
        escape_json(esc_user, app_config.rtsp_auth_user, sizeof(esc_user));
        escape_json(esc_codec, app_config.rtsp_audio_codec, sizeof(esc_codec));
        respLen = sprintf(response,
            "HTTP/1.1 200 OK\r\n"
            "Content-Type: application/json;charset=UTF-8\r\n"
            "Connection: close\r\n"
            "\r\n"
            "{\"enable\":%s,\"enable_auth\":%s,\"port\":%d,\"auth_user\":\"%s\",\"audio_codec\":\"%s\","
            "\"note\":\"port and codec changes apply after restart\"}",
            app_config.rtsp_enable ? "true" : "false", app_config.rtsp_enable_auth ? "true" : "false",
            app_config.rtsp_port, esc_user, esc_codec);
        send_and_close(req->clntFd, response, respLen);
        return;
    }
    if (EQUALS(req->uri, "/api/onvif")) {
        if (req->query) {
            while (req->query) {
                char *value = split(&req->query, "&");
                if (!value || !*value) continue;
                unescape_uri(value);
                char *key = split(&value, "=");
                if (!key || !*key || !value || !*value) continue;
                if (EQUALS(key, "enable"))
                    app_config.onvif_enable = EQUALS_CASE(value, "true") || EQUALS(value, "1");
                else if (EQUALS(key, "enable_auth"))
                    app_config.onvif_enable_auth = EQUALS_CASE(value, "true") || EQUALS(value, "1");
                else if (EQUALS(key, "auth_user"))
                    strncpy(app_config.onvif_auth_user, value, sizeof(app_config.onvif_auth_user) - 1);
                else if (EQUALS(key, "auth_pass"))
                    strncpy(app_config.onvif_auth_pass, value, sizeof(app_config.onvif_auth_pass) - 1);
            }
        }
        char esc_onvif_user[sizeof(app_config.onvif_auth_user) * 6 + 1];
        escape_json(esc_onvif_user, app_config.onvif_auth_user, sizeof(esc_onvif_user));
        respLen = sprintf(response,
            "HTTP/1.1 200 OK\r\n"
            "Content-Type: application/json;charset=UTF-8\r\n"
            "Connection: close\r\n"
            "\r\n"
            "{\"enable\":%s,\"enable_auth\":%s,\"auth_user\":\"%s\",\"note\":\"applies after restart\"}",
            app_config.onvif_enable ? "true" : "false", app_config.onvif_enable_auth ? "true" : "false",
            esc_onvif_user);
        send_and_close(req->clntFd, response, respLen);
        return;
    }
    if (EQUALS(req->uri, "/api/speak")) {
        /* Talk-back: the body is raw 16 kHz mono signed 16-bit PCM */
        int ok = EXIT_FAILURE, volume = -1;
#if defined(__ARM_PCS_VFP)
        if (plat == HAL_PLATFORM_I3 && app_config.audio_enable) {
            char *vol = req->query ? strstr(req->query, "volume=") : NULL;
            if (vol) ok = (volume = i3_speaker_volume(atoi(vol + 7))) < 0;
            if (EQUALS(req->method, "POST") &&
                req->paysize >= 2 && req->total >= req->header_len + req->paysize)
                ok = i3_speaker_write((short*)req->payload, req->paysize / 2);
            volume = i3_speaker_volume(-1);
        }
#endif
        respLen = sprintf(response,
            "HTTP/1.1 %s\r\n"
            "Content-Type: application/json;charset=UTF-8\r\n"
            "Connection: close\r\n"
            "\r\n"
            "{\"ok\":%s,\"volume\":%d}", ok ? "503 Service Unavailable" : "200 OK",
            ok ? "false" : "true", volume);
        send_and_close(req->clntFd, response, respLen);
        return;
    }
    if (EQUALS(req->uri, "/api/lamp")) {
        /* White lamp only; unlike /api/night this leaves the day/night logic alone */
        if (req->query) {
            if (strstr(req->query, "on=1") || strstr(req->query, "on=true")) night_whiteled(1);
            else if (strstr(req->query, "on=0") || strstr(req->query, "on=false")) night_whiteled(0);
            else if (strstr(req->query, "toggle")) night_whiteled(!night_whiteled_on());
        }
        respLen = sprintf(response,
            "HTTP/1.1 200 OK\r\n"
            "Content-Type: application/json;charset=UTF-8\r\n"
            "Connection: close\r\n"
            "\r\n"
            "{\"on\":%s}", night_whiteled_on() ? "true" : "false");
        send_and_close(req->clntFd, response, respLen);
        return;
    }
    if (EQUALS(req->uri, "/api/night")) {
        if (req->query) {
            char *remain;
            while (req->query) {
                char *value = split(&req->query, "&");
                if (!value || !*value) continue;
                unescape_uri(value);
                char *key = split(&value, "=");
                if (!key || !*key || !value || !*value) continue;
                if (EQUALS(key, "enable")) {
                    if (EQUALS_CASE(value, "true") || EQUALS(value, "1"))
                        app_config.night_mode_enable = 1;
                    else if (EQUALS_CASE(value, "false") || EQUALS(value, "0"))
                        app_config.night_mode_enable = 0;
                } else if (EQUALS(key, "adc_device")) {
                    strncpy(app_config.adc_device, value, sizeof(app_config.adc_device));
                } else if (EQUALS(key, "adc_threshold")) {
                    short result = strtol(value, &remain, 10);
                    if (remain != value)
                        app_config.adc_threshold = result;
                } else if (EQUALS(key, "grayscale")) {
                    if (EQUALS_CASE(value, "true") || EQUALS(value, "1"))
                        night_grayscale(1);
                    else if (EQUALS_CASE(value, "false") || EQUALS(value, "0"))
                        night_grayscale(0);
                } else if (EQUALS(key, "ircut")) {
                    if (EQUALS_CASE(value, "true") || EQUALS(value, "1"))
                        night_ircut(1);
                    else if (EQUALS_CASE(value, "false") || EQUALS(value, "0"))
                        night_ircut(0);
                } else if (EQUALS(key, "ircut_pin1")) {
                    short result = strtol(value, &remain, 10);
                    if (remain != value)
                        app_config.ir_cut_pin1 = result;
                } else if (EQUALS(key, "ircut_pin2")) {
                    short result = strtol(value, &remain, 10);
                    if (remain != value)
                        app_config.ir_cut_pin2 = result;
                } else if (EQUALS(key, "irled")) {
                    if (EQUALS_CASE(value, "true") || EQUALS(value, "1"))
                        night_irled(1);
                    else if (EQUALS_CASE(value, "false") || EQUALS(value, "0"))
                        night_irled(0);
                } else if (EQUALS(key, "whiteled")) {
                    if (EQUALS_CASE(value, "true") || EQUALS(value, "1"))
                        night_whiteled(1);
                    else if (EQUALS_CASE(value, "false") || EQUALS(value, "0"))
                        night_whiteled(0);
                } else if (EQUALS(key, "irled_pin")) {
                    short result = strtol(value, &remain, 10);
                    if (remain != value)
                        app_config.ir_led_pin = result;
                } else if (EQUALS(key, "irsense_pin")) {
                    short result = strtol(value, &remain, 10);
                    if (remain != value)
                        app_config.ir_sensor_pin = result;
                } else if (EQUALS(key, "manual")) {
                    if (EQUALS_CASE(value, "true") || EQUALS(value, "1"))
                        night_manual(1);
                    else if (EQUALS_CASE(value, "false") || EQUALS(value, "0"))
                        night_manual(0);
                }
            }

            night_disable();
            if (app_config.night_mode_enable) night_enable();
        }
        unsigned int expo[4] = {0};
        int expoBv = 0;
        night_exposure(&expo[0], &expo[1], &expo[2], &expo[3], &expoBv);
        respLen = sprintf(response,
            "HTTP/1.1 200 OK\r\n"
            "Content-Type: application/json;charset=UTF-8\r\n"
            "Connection: close\r\n"
            "\r\n"
            "{\"active\":%s,\"manual\":%s,\"grayscale\":%s,\"ircut\":%s,\"ircut_pin1\":%d,\"ircut_pin2\":%d,"
            "\"irled\":%s,\"whiteled\":%s,\"irled_pin\":%d,\"irsense_pin\":%d,\"adc_device\":\"%s\",\"adc_threshold\":%d,"
            "\"sensor_gain\":%u,\"isp_gain\":%u,\"shutter_us\":%u,\"avg_y\":%u,\"bv\":%d,"
            "\"exposure\":%u,\"exposure_night\":%u,\"exposure_day\":%u}",
            app_config.night_mode_enable ? "true" : "false", night_manual_on() ? "true" : "false",
            night_grayscale_on() ? "true" : "false",
            night_ircut_on() ? "true" : "false", app_config.ir_cut_pin1, app_config.ir_cut_pin2,
            night_irled_on() ? "true" : "false", night_whiteled_on() ? "true" : "false",
            app_config.ir_led_pin, app_config.ir_sensor_pin,
            app_config.adc_device, app_config.adc_threshold,
            expo[0], expo[1], expo[2], expo[3], expoBv,
            (unsigned int)((unsigned long long)expo[0] * expo[1] / 1024 * expo[2] / 40000 / 1024),
            app_config.exposure_night, app_config.exposure_day);
        send_and_close(req->clntFd, response, respLen);
        return;
    }

    if (app_config.osd_enable && STARTS_WITH(req->uri, "/api/osd/")) {
        char *remain;
        int respLen;
        short id = strtol(req->uri + 9, &remain, 10);
        if (remain == req->uri + 9 || id < 0 || id >= MAX_OSD) {
            send_http_error(req->clntFd, 404);
            return;
        }
        if (EMPTY(osds[id].text) && EMPTY(osds[id].img))
            region_defaults(&osds[id], id);
        if (EQUALS(req->method, "POST")) {
            char *type = request_header("Content-Type");
            if (type && STARTS_WITH(type, "multipart/form-data")) {
                char *bound = strstr(type, "boundary=") + strlen("boundary=");

                char *payloadb = strstr(req->payload, bound);
                payloadb = memstr(payloadb, "\r\n\r\n", req->total - (payloadb - req->input), 4);
                if (payloadb) payloadb += 4;

                char *payloade = memstr(payloadb, bound,
                    req->total - (payloadb - req->input), strlen(bound));
                if (payloade) payloade -= 4;

                char path[32];

                if (!memcmp(payloadb, "\x89\x50\x4E\x47\xD\xA\x1A\xA", 8))
                    sprintf(path, "/tmp/osd%d.png", id);
                else
                    sprintf(path, "/tmp/osd%d.bmp", id);

                FILE *img = fopen(path, "wb");
                fwrite(payloadb, sizeof(char), payloade - payloadb, img);
                fclose(img);

                strcpy(osds[id].text, "");
                osds[id].updt = 1;
            } else {
                respLen = sprintf(response,
                    "HTTP/1.1 415 Unsupported Media Type\r\n"
                    "Content-Type: text/plain\r\n"
                    "Connection: close\r\n"
                    "\r\n"
                    "The payload must be presented as multipart/form-data.\r\n"
                );
                send_and_close(req->clntFd, response, respLen);
                return;
            }
        }
        if (req->query) {
            char *remain;
            while (req->query) {
                char *value = split(&req->query, "&");
                if (!value || !*value) continue;
                unescape_uri(value);
                char *key = split(&value, "=");
                if (!key || !*key || !value || !*value) continue;
                if (EQUALS(key, "img"))
                    strncpy(osds[id].img, value,
                        sizeof(osds[id].img) - 1);
                else if (EQUALS(key, "font"))
                    strncpy(osds[id].font, !EMPTY(value) ? value : DEF_FONT,
                        sizeof(osds[id].font) - 1);
                else if (EQUALS(key, "text"))
                    strncpy(osds[id].text, value,
                        sizeof(osds[id].text) - 1);
                else if (EQUALS(key, "size")) {
                    double result = strtod(value, &remain);
                    if (remain == value) continue;
                    osds[id].size = (result != 0 ? result : DEF_SIZE);
                }
                else if (EQUALS(key, "color")) {
                    int result = color_parse(value);
                    osds[id].color = result;
                }
                else if (EQUALS(key, "opal")) {
                    short result = strtol(value, &remain, 10);
                    if (remain != value)
                        osds[id].opal = result & 0xFF;
                }
                else if (EQUALS(key, "anchor"))
                    osds[id].anchor = region_anchor_parse(value);
                else if (EQUALS(key, "posx")) {
                    short result = strtol(value, &remain, 10);
                    if (remain != value)
                        osds[id].posx = result;
                }
                else if (EQUALS(key, "posy")) {
                    short result = strtol(value, &remain, 10);
                    if (remain != value)
                        osds[id].posy = result;
                }
                else if (EQUALS(key, "pos")) {
                    int x, y;
                    if (sscanf(value, "%d,%d", &x, &y) == 2) {
                        osds[id].posx = x;
                        osds[id].posy = y;
                    }
                }
                else if (EQUALS(key, "outl")) {
                    int result = color_parse(value);
                    osds[id].outl = result;
                }
                else if (EQUALS(key, "thick")) {
                    double result = strtod(value, &remain);
                    if (remain == value) continue;
                        osds[id].thick = result;
                }
            }
            osds[id].updt = 1;
        }
        int color = (((osds[id].color >> 10) & 0x1F) * 255 / 31) << 16 |
                    (((osds[id].color >> 5) & 0x1F) * 255 / 31) << 8 |
                    ((osds[id].color & 0x1F) * 255 / 31);
        respLen = sprintf(response,
            "HTTP/1.1 200 OK\r\n"
            "Content-Type: application/json;charset=UTF-8\r\n"
            "Connection: close\r\n"
            "\r\n"
            "{\"id\":%d,\"color\":\"#%x\",\"opal\":%d,\"anchor\":\"%s\",\"pos\":[%d,%d],"
            "\"font\":\"%s\",\"size\":%.1f,\"text\":\"%s\",\"img\":\"%s\","
            "\"outl\":\"#%x\",\"thick\":%.1f}",
            id, color, osds[id].opal, osd_anchors[(unsigned char)osds[id].anchor % 9],
            osds[id].posx, osds[id].posy,
            osds[id].font, osds[id].size, osds[id].text, osds[id].img,
            osds[id].outl, osds[id].thick);
        send_and_close(req->clntFd, response, respLen);
        return;
    }

    if (EQUALS(req->uri, "/api/recordings")) {
        send_clip_list(req->clntFd);
        return;
    }

    if (STARTS_WITH(req->uri, "/recordings/")) {
        unescape_uri(req->uri);
        if (strstr(req->uri, "..")) send_http_error(req->clntFd, 404);
        else send_clip(req, req->uri + 12);
        return;
    }

    if (EQUALS(req->uri, "/api/record")) {
        if (req->query) {
            char *remain;
            char wantStart = 0, wantStop = 0;
            long duration = -1;
            while (req->query) {
                char *value = split(&req->query, "&");
                if (!value || !*value) continue;
                unescape_uri(value);
                char *key = split(&value, "=");
                if (!key || !*key) continue;
                /* "start" and "stop" come without a value */
                if (EQUALS(key, "start")) { wantStart = 1; continue; }
                if (EQUALS(key, "stop")) { wantStop = 1; continue; }
                if (!value || !*value) continue;
                if (EQUALS(key, "enable")) {
                    if (EQUALS_CASE(value, "true") || EQUALS(value, "1"))
                        app_config.record_enable = 1;
                    else if (EQUALS_CASE(value, "false") || EQUALS(value, "0"))
                        app_config.record_enable = 0;
                }
                else if (EQUALS(key, "continuous")) {
                    if (EQUALS_CASE(value, "true") || EQUALS(value, "1"))
                        app_config.record_continuous = 1;
                    else if (EQUALS_CASE(value, "false") || EQUALS(value, "0"))
                        app_config.record_continuous = 0;
                }
                else if (EQUALS(key, "path"))
                    strncpy(app_config.record_path, value, sizeof(app_config.record_path) - 1);
                else if (EQUALS(key, "filename"))
                    strncpy(app_config.record_filename, value, sizeof(app_config.record_filename) - 1);
                else if (EQUALS(key, "segment_duration")) {
                    long result = strtol(value, &remain, 10);
                    if (remain != value && result >= 0)
                        app_config.record_segment_duration = result;
                }
                else if (EQUALS(key, "segment_size")) {
                    long result = strtol(value, &remain, 10);
                    if (remain != value && result >= 0)
                        app_config.record_segment_size = result;
                }
                else if (EQUALS(key, "duration")) {
                    long result = strtol(value, &remain, 10);
                    if (remain != value && result >= 0 && result <= 86400) duration = result;
                }
                else if (EQUALS(key, "reserve")) {
                    long result = strtol(value, &remain, 10);
                    if (remain != value && result >= 0)
                        app_config.record_reserve = result;
                }

            }

            /* Bring the recorder in line with the settings: continuous mode
             * records whenever enabled, otherwise start/stop decide. An
             * explicit stop also ends continuous mode, or it would restart. */
            if (!app_config.record_enable || wantStop) {
                if (wantStop) app_config.record_continuous = 0;
                if (record_active()) record_stop();
            } else if (app_config.record_continuous || wantStart) {
                /* "start&duration=N" records for N seconds, repeating it
                 * moves the end so an event source can keep it going;
                 * a plain "start" records until "stop" */
                if (record_start() && wantStart)
                    record_stop_after(duration > 0 ? duration : 0);
            }
        }
        char start_time[32] = "", storage[64];
        record_storage(storage, sizeof(storage));
        time_t startTime = record_start_time();
        if (startTime) {
            struct tm tm_buf, *tm_info = localtime_r(&startTime, &tm_buf);
            strftime(start_time, sizeof(start_time), "%Y-%m-%d %H:%M:%S", tm_info);
        }

        respLen = sprintf(response,
            "HTTP/1.1 200 OK\r\n"
            "Content-Type: application/json;charset=UTF-8\r\n"
            "Connection: close\r\n"
            "\r\n"
            "{\"recording\":%s,\"start_time\":\"%s\",\"continuous\":%s,\"path\":\"%s\","
            "\"filename\":\"%s\",\"segment_duration\":%d,\"segment_size\":%d,"
            "\"enable\":%s,\"reserve\":%d,\"storage\":\"%s\",\"stop_in\":%u}",
                record_active() ? "true" : "false", start_time, app_config.record_continuous ? "true" : "false",
                app_config.record_path, app_config.record_filename,
                app_config.record_segment_duration, app_config.record_segment_size,
                app_config.record_enable ? "true" : "false", app_config.record_reserve, storage,
                record_stop_in());
        send_and_close(req->clntFd, response, respLen);
        return;
    }

    if (EQUALS(req->uri, "/api/sub")) {
        /* The scaler size is read once when the vendor library loads and the
         * second RTSP server is set up at start, so a change is saved and the
         * streamer restarts itself. */
        char changed = 0;
        if (req->query) {
            char *remain;
            while (req->query) {
                char *value = split(&req->query, "&");
                if (!value || !*value) continue;
                unescape_uri(value);
                char *key = split(&value, "=");
                if (!key || !*key || !value || !*value) continue;
                if (EQUALS(key, "enable")) {
                    bool on = EQUALS_CASE(value, "true") || EQUALS(value, "1");
                    if (on != app_config.sub_enable) changed = 1;
                    app_config.sub_enable = on;
                    continue;
                }
                long result = strtol(value, &remain, 10);
                if (remain == value) continue;
                unsigned int *field = NULL;
                long min = 1, max = INT_MAX;
                if (EQUALS(key, "size")) {
                    /* "640x360" */
                    long h = *remain == 'x' ? strtol(remain + 1, NULL, 10) : 0;
                    /* Sizes the Infinity3 scaler was seen to deliver; others give
                     * no frames or stop the pipeline. */
                    if (!((result == 640 && (h == 360 || h == 480)) || (result == 704 && h == 576)))
                        continue;
                    if (result != app_config.sub_width || h != app_config.sub_height) changed = 1;
                    app_config.sub_width = result;
                    app_config.sub_height = h;
                    continue;
                }
                else if (EQUALS(key, "gop")) { field = &app_config.sub_gop; max = 300; }
                else if (EQUALS(key, "bitrate")) { field = &app_config.sub_bitrate; min = 32; max = 8192; }
                else if (EQUALS(key, "rtsp_port")) { field = &app_config.sub_rtsp_port; max = USHRT_MAX; }
                if (!field || result < min || result > max) continue;
                if (*field != result) changed = 1;
                *field = result;
            }
        }

        respLen = sprintf(response,
            "HTTP/1.1 200 OK\r\n"
            "Content-Type: application/json;charset=UTF-8\r\n"
            "Connection: close\r\n"
            "\r\n"
            "{\"enable\":%s,\"size\":\"%dx%d\",\"gop\":%d,\"bitrate\":%d,"
            "\"rtsp_port\":%d,\"measured\":\"%d fps\",\"restarting\":%s}",
            app_config.sub_enable ? "true" : "false", app_config.sub_width, app_config.sub_height,
            app_config.sub_gop, app_config.sub_bitrate,
            app_config.sub_rtsp_port, media_sub_rate(), changed ? "true" : "false");
        send_and_close(req->clntFd, response, respLen);
        if (changed && !app_config_save())
            kill(getpid(), SIGHUP);
        return;
    }

    if (EQUALS(req->uri, "/api/status")) {
        struct sysinfo si;
        sysinfo(&si);
        char memory[16], uptime[48];
        short free = (si.freeram + si.bufferram) / 1024 / 1024;
        short total = si.totalram / 1024 / 1024;
        sprintf(memory, "%d/%dMB", total - free, total);
        if (si.uptime > 86400)
            sprintf(uptime, "%ld days, %ld:%02ld:%02ld", si.uptime / 86400, (si.uptime % 86400) / 3600, (si.uptime % 3600) / 60, si.uptime % 60);
        else if (si.uptime > 3600)
            sprintf(uptime, "%ld:%02ld:%02ld", si.uptime / 3600, (si.uptime % 3600) / 60, si.uptime % 60);
        else
            sprintf(uptime, "%ld:%02ld", si.uptime / 60, si.uptime % 60);
        respLen = sprintf(response,
            "HTTP/1.1 200 OK\r\n"
            "Content-Type: application/json;charset=UTF-8\r\n"
            "Connection: close\r\n"
            "\r\n"
            "{\"chip\":\"%s\",\"loadavg\":[%.2f,%.2f,%.2f],\"memory\":\"%s\","
            "\"sensor\":\"%s\",\"temp\":\"%.1f\u00B0C\",\"uptime\":\"%s\"}",
            chip, si.loads[0] / 65536.0, si.loads[1] / 65536.0, si.loads[2] / 65536.0,
            memory, sensor, hal_temperature_read(), uptime);
        send_and_close(req->clntFd, response, respLen);
        return;
    }

    if (EQUALS(req->uri, "/api/time")) {
        struct timespec t;
        if (req->query) {
            char *remain;
            while (req->query) {
                char *value = split(&req->query, "&");
                if (!value || !*value) continue;
                unescape_uri(value);
                char *key = split(&value, "=");
                if (!key || !*key || !value || !*value) continue;
                if (EQUALS(key, "fmt")) {
                    strncpy(timefmt, value, 32);
                } else if (EQUALS(key, "tz")) {
                    /* POSIX TZ rule, e.g. CST6CDT,M3.2.0,M11.1.0. Kept where the
                     * rest of the system reads it, and applied right away. */
                    FILE *tzf;
                    if (strlen(value) < 64 && !strpbrk(value, "\n\r\"'`$;|&") &&
                        (tzf = fopen("/etc/TZ", "w"))) {
                        fprintf(tzf, "%s\n", value);
                        fclose(tzf);
                        setenv("TZ", value, 1);
                        tzset();
                    }
                } else if (EQUALS(key, "ts")) {
                    long long result = strtoll(value, &remain, 10);
                    if (remain == value) continue;
                    t.tv_sec = result;
                    clock_settime(CLOCK_REALTIME, &t);
                }
            }
        }
        clock_gettime(CLOCK_REALTIME, &t);
        respLen = sprintf(response,
            "HTTP/1.1 200 OK\r\n"
            "Content-Type: application/json;charset=UTF-8\r\n"
            "Connection: close\r\n"
            "\r\n"
            "{\"fmt\":\"%s\",\"ts\":%zu,\"tz\":\"%s\"}", timefmt, t.tv_sec,
            getenv("TZ") ? getenv("TZ") : "");
        send_and_close(req->clntFd, response, respLen);
        return;
    }

    if (app_config.web_enable_static && send_file(req->clntFd, req->uri))
        return;

    send_http_error(req->clntFd, 400);
}

void *server_thread(void *vargp) {
    int ret, server_fd = *((int *)vargp);
    int enable = 1;
    if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &enable, sizeof(int)) < 0) {
        HAL_WARNING("server", "setsockopt(SO_REUSEADDR) failed");
        fflush(stdout);
    }

    fcntl(server_fd, F_SETFL, fcntl(server_fd, F_GETFL, 0) | O_NONBLOCK);

    struct sockaddr_in client, server = {
        .sin_family = AF_INET,
        .sin_port = htons(app_config.web_port),
        .sin_addr.s_addr = htonl(INADDR_ANY)
    };
    /* A previous instance that is still shutting down keeps the port for a
     * few seconds; giving up at once would leave the camera without its page. */
    for (int attempt = 0; attempt < 30 && keepRunning; attempt++) {
        if (!(ret = bind(server_fd, (struct sockaddr *)&server, sizeof(server)))) break;
        if (errno != EADDRINUSE) break;
        sleep(1);
    }
    if (ret) {
        HAL_DANGER("server", "%s (%d)\n", strerror(errno), errno);
        keepRunning = 0;
        close_socket_fd(server_fd);
        return NULL;
    }
    listen(server_fd, 128);

    struct pollfd fds[HTTP_MAX_CLIENTS + 1];
    http_request_t reqs[HTTP_MAX_CLIENTS];

    for (int i = 0; i < HTTP_MAX_CLIENTS; i++) {
        fds[i + 1].fd = -1;
        reqs[i].clntFd = -1;
        reqs[i].input = NULL;
    }

    fds[0].fd = server_fd;
    fds[0].events = POLLIN;

    while (keepRunning) {
        int poll_count = poll(fds, HTTP_MAX_CLIENTS + 1, 1000);
        if (poll_count < 0) {
            if (errno == EINTR) continue;
            break;
        }

        if (fds[0].revents & POLLIN) {
            struct sockaddr_in client_sock;
            socklen_t client_sock_len = sizeof(client_sock);
            int clntFd = accept(server_fd, (struct sockaddr *)&client_sock, &client_sock_len);
            if (clntFd >= 0) {
                fcntl(clntFd, F_SETFL, fcntl(clntFd, F_GETFL, 0) | O_NONBLOCK);
                int i = 0;
                for (i = 0; i < HTTP_MAX_CLIENTS; i++) {
                    if (fds[i+1].fd == -1) {
                        fds[i+1].fd = clntFd;
                        fds[i+1].events = POLLIN;
                        reqs[i].clntFd = clntFd;
                        reqs[i].total = 0;
                        reqs[i].buf_size = HTTP_MIN_BUF_SIZE;
                        reqs[i].input = malloc(reqs[i].buf_size + 1);
                        reqs[i].header_len = 0;
                        reqs[i].paysize = 0;
                        break;
                    }
                }
                if (i == HTTP_MAX_CLIENTS) {
                    close_socket_fd(clntFd);
                }
            }
        }

        for (int i = 0; i < HTTP_MAX_CLIENTS; i++) {
            if (fds[i + 1].fd != -1) {
                if (fds[i + 1].revents & (POLLERR | POLLHUP | POLLNVAL)) {
                    close_socket_fd(fds[i + 1].fd);
                    fds[i + 1].fd = -1;
                    free(reqs[i].input);
                    reqs[i].input = NULL;
                    continue;
                }
                if (fds[i + 1].revents & POLLIN) {
                    http_request_t *req = &reqs[i];

                    if (req->total == req->buf_size) {
                        req->buf_size *= 2;
                        if (req->buf_size > HTTP_MAX_BUF_SIZE) {
                            close_socket_fd(req->clntFd);
                            fds[i + 1].fd = -1;
                            free(req->input);
                            req->input = NULL;
                            continue;
                        }
                        req->input = realloc(req->input, req->buf_size + 1);
                    }

                    int received = recv(req->clntFd, req->input + req->total, req->buf_size - req->total, 0);
                    if (received < 0) {
                        if (errno == EAGAIN || errno == EWOULDBLOCK || errno == EINTR)
                            continue;
                        close_socket_fd(req->clntFd);
                        fds[i + 1].fd = -1;
                        free(req->input);
                        req->input = NULL;
                        continue;
                    } else if (received == 0) {
                        close_socket_fd(req->clntFd);
                        fds[i + 1].fd = -1;
                        free(req->input);
                        req->input = NULL;
                        continue;
                    }
                    req->total += received;

                    if (req->header_len == 0) {
                        char *hdr_end = memstr(req->input, "\r\n\r\n", req->total, 4);
                        if (hdr_end) {
                            req->header_len = (hdr_end + 4) - req->input;

                            char *cl = req->input;
                            req->paysize = 0;
                            while (cl < hdr_end) {
                                if (!strncasecmp(cl, "Content-Length:", 15)) {
                                    req->paysize = atoi(cl + 15);
                                    break;
                                }
                                cl = strchr(cl, '\n');
                                if (!cl) break;
                                cl++;
                            }
                        }
                    }

                    if (req->header_len > 0 && req->total >= req->header_len + req->paysize) {
                        parse_request(req);
                        if (req->clntFd != -1) {
                            fcntl(req->clntFd, F_SETFL, fcntl(req->clntFd, F_GETFL, 0) & ~O_NONBLOCK);
                            {
                                /* A client that stops reading a stream (e.g. a browser that gave
                                 * up on the MP4) must not block the whole server on send() */
                                struct timeval tv = { .tv_sec = 5, .tv_usec = 0 };
                                setsockopt(req->clntFd, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof(tv));
                            }
                            respond_request(req);
                        }
                        fds[i + 1].fd = -1;
                        free(req->input);
                        req->input = NULL;
                    }
                }
            }
        }
    }

    for (int i = 0; i < HTTP_MAX_CLIENTS; i++) {
        if (fds[i + 1].fd != -1) {
            close_socket_fd(fds[i + 1].fd);
            free(reqs[i].input);
        }
    }

    close_socket_fd(server_fd);
    HAL_INFO("server", "Thread has exited\n");
    return NULL;
}

int server_start() {
    for (int i = 0; i < HTTP_MAX_CLIENTS; i++) {
        client_fds[i].sockFd = -1;
        client_fds[i].type = -1;
    }
    pthread_mutex_init(&client_fds_mutex, NULL);

    server_fd = socket(AF_INET, SOCK_STREAM, 0);

    {
        pthread_attr_t thread_attr;
        pthread_attr_init(&thread_attr);
        size_t stacksize;
        pthread_attr_getstacksize(&thread_attr, &stacksize);
        size_t new_stacksize = app_config.web_server_thread_stack_size;
        if (pthread_attr_setstacksize(&thread_attr, new_stacksize))
            HAL_WARNING("server", "Can't set stack size %zu\n", new_stacksize);
        if (pthread_create(
            &server_thread_id, &thread_attr, server_thread, (void *)&server_fd))
            HAL_ERROR("server", "Starting the server thread failed!\n");
        if (pthread_attr_setstacksize(&thread_attr, stacksize))
            HAL_DANGER("server", "Can't set stack size %zu\n", stacksize);
        pthread_attr_destroy(&thread_attr);
    }

    return EXIT_SUCCESS;
}

int server_stop() {
    keepRunning = 0;

    close_socket_fd(server_fd);
    pthread_join(server_thread_id, NULL);

    pthread_mutex_destroy(&client_fds_mutex);
    HAL_INFO("server", "Shutting down server...\n");

    return EXIT_SUCCESS;
}
