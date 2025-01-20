#include "server.h"

#define MAX_CLIENTS 50
#define REQSIZE 512 * 1024

IMPORT_STR(.rodata, "../res/index.html", indexhtml);
extern const char indexhtml[];

enum StreamType {
    STREAM_H26X,
    STREAM_JPEG,
    STREAM_MJPEG,
    STREAM_MP3,
    STREAM_MP4,
    STREAM_PCM
};

struct Request {
    int clntFd;
    char *input, *method, *payload, *prot, *query, *uri;
    int paysize, total;
};

struct Client {
    int socket_fd;
    enum StreamType type;
    struct Mp4State mp4;
    unsigned int nalCnt;
} client_fds[MAX_CLIENTS];

struct Header {
    char *name, *value;
} reqhdr[17] = {{"\0", "\0"}};

int server_fd = -1;
pthread_t server_thread_id;
pthread_mutex_t client_fds_mutex;

const char error400[] = "HTTP/1.1 400 Bad Request\r\n" \
                        "Content-Type: text/plain\r\n" \
                        "Connection: close\r\n" \
                        "\r\n" \
                        "The server has no handler to the request.\r\n";
const char error404[] = "HTTP/1.1 404 Not Found\r\n" \
                        "Content-Type: text/plain\r\n" \
                        "Connection: close\r\n" \
                        "\r\n" \
                        "The requested resource was not found.\r\n";
const char error405[] = "HTTP/1.1 405 Method Not Allowed\r\n" \
                        "Content-Type: text/plain\r\n" \
                        "Connection: close\r\n" \
                        "\r\n" \
                        "This method is not handled by this server.\r\n";
const char error500[] = "HTTP/1.1 500 Internal Server Error\r\n" \
                        "Content-Type: text/plain\r\n" \
                        "Connection: close\r\n" \
                        "\r\n" \
                        "An invalid operation was caught on this request.\r\n";

void close_socket_fd(int socket_fd) {
    shutdown(socket_fd, SHUT_RDWR);
    close(socket_fd);
}

void free_client(int i) {
    if (client_fds[i].socket_fd < 0) return;

    close_socket_fd(client_fds[i].socket_fd);
    client_fds[i].socket_fd = -1;
}

int send_to_fd(int client_fd, char *buf, ssize_t size) {
    ssize_t sent = 0, len = 0;
    if (client_fd < 0) return -1;

    while (sent < size) {
        len = send(client_fd, buf + sent, size - sent, MSG_NOSIGNAL);
        if (len < 0) return -1;
        sent += len;
    }

    return 0;
}

int send_to_fd_nonblock(int client_fd, char *buf, ssize_t size) {
    if (client_fd < 0) return -1;

    send(client_fd, buf, size, MSG_DONTWAIT | MSG_NOSIGNAL);

    return 0;
}

int send_to_client(int i, char *buf, ssize_t size) {
    if (send_to_fd(client_fds[i].socket_fd, buf, size) < 0) {
        free_client(i);
        return -1;
    }
    
    return 0;
}

static void send_and_close(int client_fd, char *buf, ssize_t size) {
    send_to_fd(client_fd, buf, size);
    close_socket_fd(client_fd);
}

void send_h26x_to_client(char index, hal_vidstream *stream) {
    for (unsigned int i = 0; i < stream->count; ++i) {
        hal_vidpack *pack = &stream->pack[i];
        unsigned int pack_len = pack->length - pack->offset;
        unsigned char *pack_data = pack->data + pack->offset;

        pthread_mutex_lock(&client_fds_mutex);
        for (unsigned int i = 0; i < MAX_CLIENTS; ++i) {
            if (client_fds[i].socket_fd < 0) continue;
            if (client_fds[i].type != STREAM_H26X) continue;

            for (char j = 0; j < pack->naluCnt; j++) {
                if (client_fds[i].nalCnt == 0 &&
                    pack->nalu[j].type != NalUnitType_SPS &&
                    pack->nalu[j].type != NalUnitType_SPS_HEVC)
                    continue;

#ifdef DEBUG_VIDEO
                printf("NAL: %s send to %d\n", nal_type_to_str(pack->nalu[j].type), i);
#endif

                static char len_buf[50];
                ssize_t len_size = sprintf(len_buf, "%zX\r\n", pack->nalu[j].length);
                if (send_to_client(i, len_buf, len_size) < 0)
                    continue; // send <SIZE>\r\n
                if (send_to_client(i, pack_data + pack->nalu[j].offset, pack->nalu[j].length) < 0)
                    continue; // send <DATA>
                if (send_to_client(i, "\r\n", 2) < 0)
                    continue; // send \r\n

                client_fds[i].nalCnt++;
                if (client_fds[i].nalCnt == 300) {
                    char end[] = "0\r\n\r\n";
                    if (send_to_client(i, end, sizeof(end)) < 0)
                        continue;
                    free_client(i);
                }
            }
        }
        pthread_mutex_unlock(&client_fds_mutex);
    }
}

void send_mp4_to_client(char index, hal_vidstream *stream, char isH265) {

    for (unsigned int i = 0; i < stream->count; ++i) {
        hal_vidpack *pack = &stream->pack[i];
        unsigned int pack_len = pack->length - pack->offset;
        unsigned char *pack_data = pack->data + pack->offset;

        for (char j = 0; j < pack->naluCnt; j++) {
#ifdef DEBUG_VIDEO
            printf("NAL: %s received in packet %d\n", nal_type_to_str(pack->nalu[j].type), i);
            printf("     starts at %p, ends at %p\n", pack_data + pack->nalu[j].offset, pack_data + pack->nalu[j].offset + pack->nalu[j].length);
#endif
            if ((pack->nalu[j].type == NalUnitType_SPS || pack->nalu[j].type == NalUnitType_SPS_HEVC) 
                && pack->nalu[j].length >= 4 && pack->nalu[j].length <= UINT16_MAX)
                mp4_set_sps(pack_data + pack->nalu[j].offset + 4, pack->nalu[j].length - 4, isH265);
            else if ((pack->nalu[j].type == NalUnitType_PPS || pack->nalu[j].type == NalUnitType_PPS_HEVC)
                && pack->nalu[j].length <= UINT16_MAX)
                mp4_set_pps(pack_data + pack->nalu[j].offset + 4, pack->nalu[j].length - 4, isH265);
            else if (pack->nalu[j].type == NalUnitType_VPS_HEVC && pack->nalu[j].length <= UINT16_MAX)
                mp4_set_vps(pack_data + pack->nalu[j].offset + 4, pack->nalu[j].length - 4);
            else if (pack->nalu[j].type == NalUnitType_CodedSliceIdr || pack->nalu[j].type == NalUnitType_CodedSliceAux)
                mp4_set_slice(pack_data + pack->nalu[j].offset + 4, pack->nalu[j].length - 4, 1);
            else if (pack->nalu[j].type == NalUnitType_CodedSliceNonIdr)
                mp4_set_slice(pack_data + pack->nalu[j].offset + 4, pack->nalu[j].length - 4, 0);
        }

        static enum BufError err;
        static char len_buf[50];
        pthread_mutex_lock(&client_fds_mutex);
        for (unsigned int i = 0; i < MAX_CLIENTS; ++i) {
            if (client_fds[i].socket_fd < 0) continue;
            if (client_fds[i].type != STREAM_MP4) continue;

            if (!client_fds[i].mp4.header_sent) {
                struct BitBuf header_buf;
                err = mp4_get_header(&header_buf);
                chk_err_continue ssize_t len_size =
                    sprintf(len_buf, "%zX\r\n", header_buf.offset);
                if (send_to_client(i, len_buf, len_size) < 0)
                    continue; // send <SIZE>\r\n
                if (send_to_client(i, header_buf.buf, header_buf.offset) < 0)
                    continue; // send <DATA>
                if (send_to_client(i, "\r\n", 2) < 0)
                    continue; // send \r\n

                client_fds[i].mp4.sequence_number = 0;
                client_fds[i].mp4.base_data_offset = header_buf.offset;
                client_fds[i].mp4.base_media_decode_time = 0;
                client_fds[i].mp4.header_sent = true;
                client_fds[i].mp4.nals_count = 0;
                client_fds[i].mp4.default_sample_duration =
                    default_sample_size;
            }

            err = mp4_set_state(&client_fds[i].mp4);
            chk_err_continue {
                struct BitBuf moof_buf;
                err = mp4_get_moof(&moof_buf);
                chk_err_continue ssize_t len_size =
                    sprintf(len_buf, "%zX\r\n", (ssize_t)moof_buf.offset);
                if (send_to_client(i, len_buf, len_size) < 0)
                    continue; // send <SIZE>\r\n
                if (send_to_client(i, moof_buf.buf, moof_buf.offset) < 0)
                    continue; // send <DATA>
                if (send_to_client(i, "\r\n", 2) < 0)
                    continue; // send \r\n
            }
            {
                struct BitBuf mdat_buf;
                err = mp4_get_mdat(&mdat_buf);
                chk_err_continue ssize_t len_size =
                    sprintf(len_buf, "%zX\r\n", (ssize_t)mdat_buf.offset);
                if (send_to_client(i, len_buf, len_size) < 0)
                    continue; // send <SIZE>\r\n
                if (send_to_client(i, mdat_buf.buf, mdat_buf.offset) < 0)
                    continue; // send <DATA>
                if (send_to_client(i, "\r\n", 2) < 0)
                    continue; // send \r\n
            }
        }
        pthread_mutex_unlock(&client_fds_mutex);
    }
}

void send_mp3_to_client(char *buf, ssize_t size) {
    pthread_mutex_lock(&client_fds_mutex);
    for (unsigned int i = 0; i < MAX_CLIENTS; ++i) {
        if (client_fds[i].socket_fd < 0) continue;
        if (client_fds[i].type != STREAM_MP3) continue;

        static char len_buf[50];
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
    for (unsigned int i = 0; i < MAX_CLIENTS; ++i) {
        if (client_fds[i].socket_fd < 0) continue;
        if (client_fds[i].type != STREAM_PCM) continue;

        static char len_buf[50];
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
    for (unsigned int i = 0; i < MAX_CLIENTS; ++i) {
        if (client_fds[i].socket_fd < 0) continue;
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
        "HTTP/1.1 200 OK\r\nContent-Type: image/jpeg\r\nContent-Length: "
        "%lu\r\nConnection: close\r\n\r\n",
        size);
    buf[size++] = '\r';
    buf[size++] = '\n';

    pthread_mutex_lock(&client_fds_mutex);
    for (unsigned int i = 0; i < MAX_CLIENTS; ++i) {
        if (client_fds[i].socket_fd < 0) continue;
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
void *send_jpeg_thread(void *vargp) {
    struct jpegtask task = *((struct jpegtask *)vargp);
    hal_jpegdata jpeg = {0};
    HAL_INFO("server", "Requesting a JPEG snapshot (%ux%u, qfactor %u, color2Gray %d)...\n",
        task.width, task.height, task.qfactor, task.color2Gray);
    int ret =
        jpeg_get(task.width, task.height, task.qfactor, task.color2Gray, &jpeg);
    if (ret) {
        HAL_DANGER("server", "Failed to receive a JPEG snapshot...\n");
        static char response[] =
            "HTTP/1.1 503 Internal Error\r\n"
            "Connection: close\r\n\r\n";
        send_and_close(task.client_fd, response, sizeof(response) - 1); // zero ending string!
        return NULL;
    }
    HAL_INFO("server", "JPEG snapshot has been received!\n");
    char buf[1024];
    int buf_len = sprintf(
        buf, "HTTP/1.1 200 OK\r\n"
        "Content-Type: image/jpeg\r\n"
        "Content-Length: %lu\r\n"
        "Connection: close\r\n\r\n",
        jpeg.jpegSize);
    send_to_fd(task.client_fd, buf, buf_len);
    send_to_fd(task.client_fd, jpeg.data, jpeg.jpegSize);
    send_to_fd(task.client_fd, "\r\n", 2);
    close_socket_fd(task.client_fd);
    free(jpeg.data);
    HAL_INFO("server", "JPEG snapshot has been sent!\n");
    return NULL;
}

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
            send_to_fd(client_fd, len_buf, len_size); // send <SIZE>\r\n
            send_to_fd(client_fd, buf, size);         // send <DATA>\r\n
        }
        char end[] = "0\r\n\r\n";
        send_to_fd(client_fd, end, sizeof(end));
        fclose(file);
        close_socket_fd(client_fd);
        return EXIT_FAILURE;
    }
    send_and_close(client_fd, (char*)error404, strlen(error404));
    return EXIT_FAILURE;
}

void send_binary(const int client_fd, const char *data, const long size) {
    char *buf;
    int buf_len = asprintf(&buf,
        "HTTP/1.1 200 OK\r\n" \
        "Content-Type: application/octet-stream\r\n" \
        "Content-Length: %zu\r\n" \
        "Connection: close\r\n\r\n", size);
    send_to_fd(client_fd, buf, buf_len);
    send_to_fd(client_fd, (char*)data, size);
    send_to_fd(client_fd, "\r\n", 2);
    close_socket_fd(client_fd);
    free(buf);
}

void send_html(const int client_fd, const char *data) {
    char *buf;
    int buf_len = asprintf(&buf,
        "HTTP/1.1 200 OK\r\n" \
        "Content-Type: text/html\r\n" \
        "Content-Length: %zu\r\n" \
        "Connection: close\r\n" \
        "\r\n%s", strlen(data), data);
    buf[buf_len++] = 0;
    send_and_close(client_fd, buf, buf_len);
    free(buf);
}

char *request_header(const char *name)
{
    struct Header *h = reqhdr;
    for (; h->name; h++)
        if (!strcasecmp(h->name, name))
            return h->value;
    return NULL;
}

struct Header *request_headers(void) { return reqhdr; }

void parse_request(struct Request *req) {
    struct sockaddr_in client_sock;
    socklen_t client_sock_len = sizeof(client_sock);
    memset(&client_sock, 0, client_sock_len);

    req->total = 0;
    int received = recv(req->clntFd, req->input, REQSIZE, 0);
    if (received < 0)
        HAL_WARNING("server", "recv() error\n");
    else if (!received)
        HAL_WARNING("server", "Client disconnected unexpectedly\n");
    req->total += received;

    if (req->total <= 0) return;

    getpeername(req->clntFd,
        (struct sockaddr *)&client_sock, &client_sock_len);

    char *state = NULL;
    req->method = strtok_r(req->input, " \t\r\n", &state);
    req->uri = strtok_r(NULL, " \t", &state);
    req->prot = strtok_r(NULL, " \t\r\n", &state);

    HAL_INFO("server", "\x1b[32mNew request: (%s) %s\n"
        "         Received from: %s\x1b[0m\n",
        req->method, req->uri, inet_ntoa(client_sock.sin_addr));

    if (req->query = strchr(req->uri, '?'))
        *req->query++ = '\0';
    else
        req->query = req->uri - 1;

    struct Header *h = reqhdr;
    char *l;
    while (h < reqhdr + 16)
    {
        char *k, *v, *e;
        if (!(k = strtok_r(NULL, "\r\n: \t", &state)))
            break;
        v = strtok_r(NULL, "\r\n", &state);
        while (*v && *v == ' ' && v++);
        h->name = k;
        h++->value = v;
#ifdef DEBUG_HTTP
        fprintf(stderr, "         (H) %s: %s\n", k, v);
#endif
        e = v + 1 + strlen(v);
        if (e[1] == '\r' && e[2] == '\n')
            break;
    }

    l = request_header("Content-Length");
    req->paysize = l ? atol(l) : 0;

    while (l && req->total < req->paysize) {
        received = recv(req->clntFd, req->input + req->total, REQSIZE - req->total, 0);
        if (received < 0) {
            HAL_WARNING("server", "recv() error\n");
            break;
        } else if (!received) {
            HAL_WARNING("server", "Client disconnected unexpectedly\n");
            break;
        }
        req->total += received;
    }

    req->payload = strtok_r(NULL, "\r\n", &state);
}

void respond_request(struct Request *req) {
    char response[256];

    if (!EQUALS(req->method, "GET") && !EQUALS(req->method, "POST")) {
        send_and_close(req->clntFd, (char*)error405, strlen(error405));
        return;
    }

    if (app_config.web_enable_auth) {
        char *auth = request_header("Authorization");
        char cred[66], valid[256];

        strcpy(cred, app_config.web_auth_user);
        strcpy(cred + strlen(app_config.web_auth_user), ":");
        strcpy(cred + strlen(app_config.web_auth_user) + 1, app_config.web_auth_pass);
        strcpy(valid, "Basic ");
        base64_encode(valid + 6, cred, strlen(cred));

        if (!auth || !EQUALS(auth, valid)) {
            int respLen = sprintf(response,
                "HTTP/1.1 401 Unauthorized\r\n"
                "Content-Type: text/plain\r\n"
                "WWW-Authenticate: Basic realm=\"Access the camera services\"\r\n"
                "Connection: close\r\n\r\n");
            send_and_close(req->clntFd, response, respLen);
            return;
        }
    }

    if (EQUALS(req->uri, "/exit")) {
        int respLen = sprintf(response,
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
        int respLen = sprintf(
            response, "HTTP/1.1 200 OK\r\nContent-Type: "
                    "audio/mpeg\r\nTransfer-Encoding: "
                    "chunked\r\nConnection: keep-alive\r\n\r\n");
        send_to_fd(req->clntFd, response, respLen);
        pthread_mutex_lock(&client_fds_mutex);
        for (uint32_t i = 0; i < MAX_CLIENTS; ++i)
            if (client_fds[i].socket_fd < 0) {
                client_fds[i].socket_fd = req->clntFd;
                client_fds[i].type = STREAM_MP3;
                break;
            }
        pthread_mutex_unlock(&client_fds_mutex);
        return;
    }

    if (app_config.audio_enable && EQUALS(req->uri, "/audio.pcm")) {
        int respLen = sprintf(
            response, "HTTP/1.1 200 OK\r\nContent-Type: "
                    "audio/pcm\r\nTransfer-Encoding: "
                    "chunked\r\nConnection: keep-alive\r\n\r\n");
        send_to_fd(req->clntFd, response, respLen);
        pthread_mutex_lock(&client_fds_mutex);
        for (uint32_t i = 0; i < MAX_CLIENTS; ++i)
            if (client_fds[i].socket_fd < 0) {
                client_fds[i].socket_fd = req->clntFd;
                client_fds[i].type = STREAM_PCM;
                break;
            }
        pthread_mutex_unlock(&client_fds_mutex);
        return;
    }

    if ((!app_config.mp4_codecH265 && EQUALS(req->uri, "/video.264")) ||
        (app_config.mp4_codecH265 && EQUALS(req->uri, "/video.265"))) {
        request_idr();
        int respLen = sprintf(
            response, "HTTP/1.1 200 OK\r\nContent-Type: "
                    "application/octet-stream\r\nTransfer-Encoding: "
                    "chunked\r\nConnection: keep-alive\r\n\r\n");
        send_to_fd(req->clntFd, response, respLen);
        pthread_mutex_lock(&client_fds_mutex);
        for (uint32_t i = 0; i < MAX_CLIENTS; ++i)
            if (client_fds[i].socket_fd < 0) {
                client_fds[i].socket_fd = req->clntFd;
                client_fds[i].type = STREAM_H26X;
                client_fds[i].nalCnt = 0;
                break;
            }
        pthread_mutex_unlock(&client_fds_mutex);
        return;
    }

    if (app_config.mp4_enable && EQUALS(req->uri, "/video.mp4")) {
        request_idr();
        int respLen = sprintf(
            response, "HTTP/1.1 200 OK\r\nContent-Type: "
                    "video/mp4\r\nTransfer-Encoding: chunked\r\n"
                    "Connection: keep-alive\r\n\r\n");
        send_to_fd(req->clntFd, response, respLen);
        pthread_mutex_lock(&client_fds_mutex);
        for (uint32_t i = 0; i < MAX_CLIENTS; ++i)
            if (client_fds[i].socket_fd < 0) {
                client_fds[i].socket_fd = req->clntFd;
                client_fds[i].type = STREAM_MP4;
                client_fds[i].mp4.header_sent = false;
                break;
            }
        pthread_mutex_unlock(&client_fds_mutex);
        return;
    }

    if (app_config.mjpeg_enable && EQUALS(req->uri, "/mjpeg")) {
        int respLen = sprintf(
            response, "HTTP/1.0 200 OK\r\nCache-Control: no-cache\r\n"
                    "Pragma: no-cache\r\nConnection: close\r\n"
                    "Content-Type: multipart/x-mixed-replace; "
                    "boundary=boundarydonotcross\r\n\r\n");
        send_to_fd(req->clntFd, response, respLen);
        pthread_mutex_lock(&client_fds_mutex);
        for (uint32_t i = 0; i < MAX_CLIENTS; ++i)
            if (client_fds[i].socket_fd < 0) {
                client_fds[i].socket_fd = req->clntFd;
                client_fds[i].type = STREAM_MJPEG;
                break;
            }
        pthread_mutex_unlock(&client_fds_mutex);
        return;
    }

    if (app_config.jpeg_enable && STARTS_WITH(req->uri, "/image.jpg")) {
        {
            struct jpegtask task;
            task.client_fd = req->clntFd;
            task.width = app_config.jpeg_width;
            task.height = app_config.jpeg_height;
            task.qfactor = app_config.jpeg_qfactor;
            task.color2Gray = 0;

            if (!EMPTY(req->query)) {
                char *remain;
                while (req->query) {
                    char *value = split(&req->query, "&");
                    if (!value || !*value) continue;
                    char *key = split(&value, "=");
                    if (!key || !*key || !value || !*value) continue;
                    if (EQUALS(key, "width")) {
                        short result = strtol(value, &remain, 10);
                        if (remain != value)
                            task.width = result;
                    }
                    else if (EQUALS(key, "height")) {
                        short result = strtol(value, &remain, 10);
                        if (remain != value)
                            task.height = result;
                    }
                    else if (EQUALS(key, "qfactor")) {
                        short result = strtol(value, &remain, 10);
                        if (remain != value)
                            task.qfactor = result;
                    }
                    else if (EQUALS(key, "color2gray") || EQUALS(key, "gray")) {
                        if (EQUALS_CASE(value, "true") || EQUALS(value, "1"))
                            task.color2Gray = 1;
                        else if (EQUALS_CASE(value, "false") || EQUALS(value, "0"))
                            task.color2Gray = 0;
                    }
                }
            }

            pthread_t thread_id;
            pthread_attr_t thread_attr;
            pthread_attr_init(&thread_attr);
            size_t stacksize;
            pthread_attr_getstacksize(&thread_attr, &stacksize);
            size_t new_stacksize = 16 * 1024;
            if (pthread_attr_setstacksize(&thread_attr, new_stacksize))
                HAL_DANGER("jpeg", "Can't set stack size %zu\n", new_stacksize);
            pthread_create(
                &thread_id, &thread_attr, send_jpeg_thread, (void *)&task);
            if (pthread_attr_setstacksize(&thread_attr, stacksize))
                HAL_DANGER("jpeg", "Can't set stack size %zu\n", stacksize);
            pthread_attr_destroy(&thread_attr);
        }
        return;
    }

    if (app_config.audio_enable && EQUALS(req->uri, "/api/audio")) {
        if (!EMPTY(req->query)) {
            char *remain;
            while (req->query) {
                char *value = split(&req->query, "&");
                if (!value || !*value) continue;
                unescape_uri(value);
                char *key = split(&value, "=");
                if (!key || !*key || !value || !*value) continue;
                if (EQUALS(key, "bitrate")) {
                    short result = strtol(value, &remain, 10);
                    if (remain != value)
                        app_config.audio_bitrate = result;
                } else if (EQUALS(key, "gain")) {
                    short result = strtol(value, &remain, 10);
                    if (remain != value)
                        app_config.audio_gain = result;
                } else if (EQUALS(key, "srate")) {
                    short result = strtol(value, &remain, 10);
                    if (remain != value)
                        app_config.audio_srate = result;
                }
            }

            disable_audio();
            enable_audio();
        }

        int respLen = sprintf(response,
            "HTTP/1.1 200 OK\r\n"
            "Content-Type: application/json;charset=UTF-8\r\n"
            "Connection: close\r\n"
            "\r\n"
            "{\"bitrate\":%d,\"gain\":%d,\"srate\":%d}",
            app_config.audio_bitrate, app_config.audio_gain, app_config.audio_srate);
        send_and_close(req->clntFd, response, respLen);
        return;
    }

    if (EQUALS(req->uri, "/api/cmd")) {
        int result = -1;
        if (!EMPTY(req->query)) {
            char *remain;
            while (req->query) {
                char *value = split(&req->query, "&");
                if (!value || !*value) continue;
                unescape_uri(value);
                char *key = split(&value, "=");
                if (!key || !*key) continue;
                if (EQUALS(key, "save")) {
                    result = save_app_config();
                    if (!result)
                        HAL_INFO("server", "Configuration saved!\n");
                    else
                        HAL_WARNING("server", "Failed to save configuration!\n");
                    break;
                }
            }
        }

        int respLen = sprintf(response,
            "HTTP/1.1 200 OK\r\n"
            "Content-Type: application/json;charset=UTF-8\r\n"
            "Connection: close\r\n"
            "\r\n"
            "{\"code\":%d}", result);
        send_and_close(req->clntFd, response, respLen);
        return;
    }

    if (EQUALS(req->uri, "/api/jpeg")) {
        if (!EMPTY(req->query)) {
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

        int respLen = sprintf(response,
            "HTTP/1.1 200 OK\r\n"
            "Content-Type: application/json;charset=UTF-8\r\n"
            "Connection: close\r\n"
            "\r\n"
            "{\"width\":%d,\"height\":%d,\"qfactor\":%d}",
            app_config.jpeg_width, app_config.jpeg_height, app_config.jpeg_qfactor);
        send_and_close(req->clntFd, response, respLen);
        return;
    }

    if (EQUALS(req->uri, "/api/mjpeg")) {
        if (!EMPTY(req->query)) {
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
                        app_config.mjpeg_width = result;
                } else if (EQUALS(key, "height")) {
                    short result = strtol(value, &remain, 10);
                    if (remain != value)
                        app_config.mjpeg_height = result;
                } else if (EQUALS(key, "fps")) {
                    short result = strtol(value, &remain, 10);
                    if (remain != value)
                        app_config.mjpeg_fps = result;
                } else if (EQUALS(key, "mode")) {
                    if (EQUALS_CASE(value, "CBR"))
                        app_config.mjpeg_mode = HAL_VIDMODE_CBR;
                    else if (EQUALS_CASE(value, "VBR"))
                        app_config.mjpeg_mode = HAL_VIDMODE_VBR;
                    else if (EQUALS_CASE(value, "QP"))
                        app_config.mjpeg_mode = HAL_VIDMODE_QP;
                }
            }

            disable_mjpeg();
            if (app_config.mjpeg_enable) enable_mjpeg();
        }

        char mode[5] = "\0";
        switch (app_config.mjpeg_mode) {
            case HAL_VIDMODE_CBR: strcpy(mode, "CBR"); break;
            case HAL_VIDMODE_VBR: strcpy(mode, "VBR"); break;
            case HAL_VIDMODE_QP: strcpy(mode, "QP"); break;
        }
        int respLen = sprintf(response,
            "HTTP/1.1 200 OK\r\n"
            "Content-Type: application/json;charset=UTF-8\r\n"
            "Connection: close\r\n"
            "\r\n"
            "{\"width\":%d,\"height\":%d,\"fps\":%d,\"mode\":\"%s\",\"bitrate\":%d}",
            app_config.mjpeg_width, app_config.mjpeg_height, app_config.mjpeg_fps, mode,
            app_config.mjpeg_bitrate);
        send_and_close(req->clntFd, response, respLen);
        return;
    }

    if (EQUALS(req->uri, "/api/mp4")) {
        if (!EMPTY(req->query)) {
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

            disable_mp4();
            if (app_config.mp4_enable) enable_mp4();
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
        int respLen = sprintf(response,
            "HTTP/1.1 200 OK\r\n"
            "Content-Type: application/json;charset=UTF-8\r\n"
            "Connection: close\r\n"
            "\r\n"
            "{\"width\":%d,\"height\":%d,\"fps\":%d,\"h265\":%s,\"mode\":\"%s\",\"profile\":\"%s\",\"bitrate\":%d}",
            app_config.mp4_width, app_config.mp4_height, app_config.mp4_fps, h265, mode,
            profile, app_config.mp4_bitrate);
        send_and_close(req->clntFd, response, respLen);
        return;
    }

    if (app_config.night_mode_enable && EQUALS(req->uri, "/api/night")) {
        if (app_config.ir_sensor_pin == 999 && !EMPTY(req->query)) {
            char *remain;
            while (req->query) {
                char *value = split(&req->query, "&");
                if (!value || !*value) continue;
                unescape_uri(value);
                char *key = split(&value, "=");
                if (!key || !*key || !value || !*value) continue;
                if (EQUALS(key, "active")) {
                    if (EQUALS_CASE(value, "true") || EQUALS(value, "1"))
                        set_night_mode(1);
                    else if (EQUALS_CASE(value, "false") || EQUALS(value, "0"))
                        set_night_mode(0);
                }
            }
        }
        int respLen = sprintf(response,
            "HTTP/1.1 200 OK\r\n"
            "Content-Type: application/json;charset=UTF-8\r\n"
            "Connection: close\r\n"
            "\r\n"
            "{\"active\":%s}",
            night_mode_is_enabled() ? "true" : "false");
        send_and_close(req->clntFd, response, respLen);
        return;
    }

    if (app_config.osd_enable && STARTS_WITH(req->uri, "/api/osd/")) {
        char *remain;
        int respLen;
        short id = strtol(req->uri + 9, &remain, 10);
        if (remain == req->uri + 9 || id < 0 || id >= MAX_OSD) {
            respLen = sprintf(response,
                "HTTP/1.1 404 Not Found\r\n"
                "Content-Type: text/plain\r\n"
                "Connection: close\r\n"
                "\r\n"
                "The requested OSD ID was not found.\r\n"
            );
            send_and_close(req->clntFd, response, respLen);
            return;
        }
        if (EQUALS(req->method, "POST")) {
            char *type = request_header("Content-Type");
            if (STARTS_WITH(type, "multipart/form-data")) {
                char *bound = strstr(type, "boundary=") + strlen("boundary=");

                char *payloadb = strstr(req->payload, bound);
                payloadb = memstr(payloadb, "\r\n\r\n", req->total - (payloadb - req->input), 4);
                if (payloadb) payloadb += 4;

                char *payloade = memstr(payloadb, bound,
                    req->total - (payloadb - req->input), strlen(bound));
                if (payloade) payloade -= 4;

                char path[32];
                sprintf(path, "/tmp/osd%d.bmp", id);

                if (!memcmp(payloadb, "\x89\x50\x4E\x47\xD\xA\x1A\xA", 8)) {
                    spng_ctx *ctx = spng_ctx_new(0);
                    if (!ctx) {
                        HAL_DANGER("server", "Constructing the PNG decoder context failed!\n");
                        goto png_error;
                    }

                    spng_set_crc_action(ctx, SPNG_CRC_USE, SPNG_CRC_USE);
                    spng_set_chunk_limits(ctx,
                        app_config.web_server_thread_stack_size,
                        app_config.web_server_thread_stack_size);
                    spng_set_png_buffer(ctx, payloadb, payloade - payloadb);

                    struct spng_ihdr ihdr;
                    int err = spng_get_ihdr(ctx, &ihdr);
                    if (err) {
                        HAL_DANGER("server", "Parsing the PNG IHDR chunk failed!\nError: %s\n", spng_strerror(err));
                        goto png_error;
                    }
#ifdef DEBUG_IMAGE
    printf("[image] (PNG) width: %u, height: %u, depth: %u, color type: %u -> %s\n",
        ihdr.width, ihdr.height, ihdr.bit_depth, ihdr.color_type, color_type_str(ihdr.color_type));
    printf("        compress: %u, filter: %u, interl: %u\n",
        ihdr.compression_method, ihdr.filter_method, ihdr.interlace_method);
#endif

                    struct spng_plte plte = {0};
                    err = spng_get_plte(ctx, &plte);
                    if (err && err != SPNG_ECHUNKAVAIL) {
                        HAL_DANGER("server", "Parsing the PNG PLTE chunk failed!\nError: %s\n", spng_strerror(err));
                        goto png_error;
                    }
#ifdef DEBUG_IMAGE
    printf("        palette: %u\n", plte.n_entries);
#endif

                    size_t bitmapsize;
                    err = spng_decoded_image_size(ctx, SPNG_FMT_RGBA8, &bitmapsize);
                    if (err) {
                        HAL_DANGER("server", "Recovering the resulting image size failed!\nError: %s\n", spng_strerror(err));
                        goto png_error;
                    }

                    unsigned char *bitmap = malloc(bitmapsize);
                    if (!bitmap) {
                        HAL_DANGER("server", "Allocating the PNG bitmap output buffer for size %u failed!\n", bitmapsize);
                        goto png_error;
                    }

                    err = spng_decode_image(ctx, bitmap, bitmapsize, SPNG_FMT_RGBA8, 0);
                    if (!bitmap) {
                        HAL_DANGER("server", "Decoding the PNG image failed!\nError: %s\n", spng_strerror(err));
                        goto png_error;
                    }

                    int headersize = 2 + sizeof(bitmapfile) + sizeof(bitmapinfo) + sizeof(bitmapfields);
                    bitmapfile headerfile = {.size = bitmapsize + headersize, .offBits = headersize};
                    bitmapinfo headerinfo = {.size = sizeof(bitmapinfo), 
                                             .width = ihdr.width, .height = -ihdr.height, .planes = 1, .bitCount = 32,
                                             .compression = 3, .sizeImage = bitmapsize, .xPerMeter = 2835, .yPerMeter = 2835};
                    bitmapfields headerfields = {.redMask = 0xFF << 0, .greenMask = 0xFF << 8, .blueMask = 0xFF << 16, .alphaMask = 0xFF << 24,
                                                 .clrSpace = {'W', 'i', 'n', ' '}};

                    FILE *img = fopen(path, "wb");
                    fwrite("BM", sizeof(char), 2, img);
                    fwrite(&headerfile, sizeof(bitmapfile), 1, img);
                    fwrite(&headerinfo, sizeof(bitmapinfo), 1, img);
                    fwrite(&headerfields, sizeof(bitmapfields), 1, img);
                    fwrite(bitmap, sizeof(char), bitmapsize, img);
                    fclose(img);

png_error:
                    spng_ctx_free(ctx);
                    free(bitmap);
                    send_and_close(req->clntFd, (char*)error500, strlen(error500));
                } else {
                    FILE *img = fopen(path, "wb");
                    fwrite(payloadb, sizeof(char), payloade - payloadb, img);
                    fclose(img);
                }

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
        if (!EMPTY(req->query))
        {
            char *remain;
            while (req->query) {
                char *value = split(&req->query, "&");
                if (!value || !*value) continue;
                unescape_uri(value);
                char *key = split(&value, "=");
                if (!key || !*key || !value || !*value) continue;
                if (EQUALS(key, "font"))
                    strcpy(osds[id].font, !EMPTY(value) ? value : DEF_FONT);
                else if (EQUALS(key, "text"))
                    strcpy(osds[id].text, value);
                else if (EQUALS(key, "size")) {
                    double result = strtod(value, &remain);
                    if (remain == value) continue;
                    osds[id].size = (result != 0 ? result : DEF_SIZE);
                }
                else if (EQUALS(key, "color")) {
                    char base = 16;
                    if (strlen(value) > 1 && value[1] == 'x') base = 0;
                    short result = strtol(value, &remain, base);
                    if (remain != value)
                        osds[id].color = result;
                }
                else if (EQUALS(key, "opal")) {
                    short result = strtol(value, &remain, 10);
                    if (remain != value)
                        osds[id].opal = result & 0xFF;
                }
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
            }
            osds[id].updt = 1;
        }
        respLen = sprintf(response,
            "HTTP/1.1 200 OK\r\n"
            "Content-Type: application/json;charset=UTF-8\r\n"
            "Connection: close\r\n"
            "\r\n"
            "{\"id\":%d,\"color\":\"%#x\",\"opal\":%d,\"pos\":[%d,%d],\"font\":\"%s\",\"size\":%.1f,\"text\":\"%s\"}",
            id, osds[id].color, osds[id].opal, osds[id].posx, osds[id].posy, osds[id].font, osds[id].size, osds[id].text);
        send_and_close(req->clntFd, response, respLen);
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
        int respLen = sprintf(response,
            "HTTP/1.1 200 OK\r\n"
            "Content-Type: application/json;charset=UTF-8\r\n"
            "Connection: close\r\n"
            "\r\n"
            "{\"chip\":\"%s\",\"loadavg\":[%.2f,%.2f,%.2f],\"memory\":\"%s\",\"sensor\":\"%s\",\"uptime\":\"%s\"}",
            chip, si.loads[0] / 65536.0, si.loads[1] / 65536.0, si.loads[2] / 65536.0, memory, sensor, uptime);
        send_and_close(req->clntFd, response, respLen);
        return;
    }

    if (EQUALS(req->uri, "/api/time")) {
        struct timespec t;
        if (!EMPTY(req->query)) {
            char *remain;
            while (req->query) {
                char *value = split(&req->query, "&");
                if (!value || !*value) continue;
                unescape_uri(value);
                char *key = split(&value, "=");
                if (!key || !*key || !value || !*value) continue;
                if (EQUALS(key, "fmt")) {
                    strncpy(timefmt, value, 32);
                } else if (EQUALS(key, "ts")) {
                    short result = strtol(value, &remain, 10);
                    if (remain == value) continue;
                    t.tv_sec = result;
                    clock_settime(0, &t);
                }
            }
        }
        clock_gettime(0, &t);
        int respLen = sprintf(response,
            "HTTP/1.1 200 OK\r\n"
            "Content-Type: application/json;charset=UTF-8\r\n"
            "Connection: close\r\n"
            "\r\n"
            "{\"fmt\":\"%s\",\"ts\":%d}", timefmt, t.tv_sec);
        send_and_close(req->clntFd, response, respLen);
        return;
    }

    if (app_config.web_enable_static && send_file(req->clntFd, req->uri))
        return;

    send_and_close(req->clntFd, (char*)error400, strlen(error400));
}

void *server_thread(void *vargp) {
    struct Request req = {0};
    int ret, server_fd = *((int *)vargp);
    int enable = 1;
    if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &enable, sizeof(int)) < 0) {
        HAL_WARNING("server", "setsockopt(SO_REUSEADDR) failed");
        fflush(stdout);
    }
    struct sockaddr_in client, server = {
        .sin_family = AF_INET,
        .sin_port = htons(app_config.web_port),
        .sin_addr.s_addr = htonl(INADDR_ANY)
    };
    if (ret = bind(server_fd, (struct sockaddr *)&server, sizeof(server))) {
        HAL_DANGER("server", "%s (%d)\n", strerror(errno), errno);
        keepRunning = 0;
        close_socket_fd(server_fd);
        return NULL;
    }
    listen(server_fd, 128);

    req.input = malloc(REQSIZE);

    while (keepRunning) {
        // Waiting for a new connection
        if ((req.clntFd = accept(server_fd, NULL, NULL)) == -1)
            break;

        parse_request(&req);

        respond_request(&req);
    }

    if (req.input)
        free(req.input);

    close_socket_fd(server_fd);
    HAL_INFO("server", "Thread has exited\n");
    return NULL;
}

int start_server() {
    for (unsigned int i = 0; i < MAX_CLIENTS; i++) {
        client_fds[i].socket_fd = -1;
        client_fds[i].type = -1;
    }
    pthread_mutex_init(&client_fds_mutex, NULL);

    // Start the server and HTTP video streams thread
    server_fd = socket(AF_INET, SOCK_STREAM, 0);

    {
        pthread_attr_t thread_attr;
        pthread_attr_init(&thread_attr);
        size_t stacksize;
        pthread_attr_getstacksize(&thread_attr, &stacksize);
        size_t new_stacksize = app_config.web_server_thread_stack_size + REQSIZE;
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

int stop_server() {
    keepRunning = 0;

    // Stop server_thread when server_fd is closed
    close_socket_fd(server_fd);
    pthread_join(server_thread_id, NULL);

    pthread_mutex_destroy(&client_fds_mutex);
    HAL_INFO("server", "Shutting down server...\n");

    return EXIT_SUCCESS;
}