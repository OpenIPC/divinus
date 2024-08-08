#include "server.h"

char keepRunning = 1;

enum StreamType {
    STREAM_H26X,
    STREAM_JPEG,
    STREAM_MJPEG,
    STREAM_MP3,
    STREAM_MP4,
    STREAM_PCM
};

struct Client {
    int socket_fd;
    enum StreamType type;
    struct Mp4State mp4;
    unsigned int nalCnt;
};

#define MAX_CLIENTS 50
struct Client client_fds[MAX_CLIENTS];
pthread_mutex_t client_fds_mutex;

void close_socket_fd(int socket_fd) {
    shutdown(socket_fd, SHUT_RDWR);
    close(socket_fd);
}

void free_client(int i) {
    if (client_fds[i].socket_fd < 0)
        return;
    close_socket_fd(client_fds[i].socket_fd);
    client_fds[i].socket_fd = -1;
}

int send_to_fd(int client_fd, char *buf, ssize_t size) {
    ssize_t sent = 0, len = 0;
    if (client_fd < 0)
        return -1;
    while (sent < size) {
        len = send(client_fd, buf + sent, size - sent, MSG_NOSIGNAL);
        if (len < 0)
            return -1;
        sent += len;
    }
    return 0;
}

int send_to_fd_nonblock(int client_fd, char *buf, ssize_t size) {
    if (client_fd < 0)
        return -1;
    send(client_fd, buf, size, MSG_DONTWAIT | MSG_NOSIGNAL);
    return 0;
}

int send_to_client(int i, char *buf, ssize_t size) {
    ;
    if (send_to_fd(client_fds[i].socket_fd, buf, size) < 0) {
        free_client(i);
        return -1;
    }
    return 0;
}

void send_h26x_to_client(char index, hal_vidstream *stream) {
    for (unsigned int i = 0; i < stream->count; ++i) {
        hal_vidpack *pack = &stream->pack[i];
        unsigned int pack_len = pack->length - pack->offset;
        unsigned char *pack_data = pack->data + pack->offset;

        pthread_mutex_lock(&client_fds_mutex);
        for (unsigned int i = 0; i < MAX_CLIENTS; ++i) {
            if (client_fds[i].socket_fd < 0)
                continue;
            if (client_fds[i].type != STREAM_H26X)
                continue;

            for (char j = 0; j < pack->naluCnt; j++) {
                if (client_fds[i].nalCnt == 0 &&
                    pack->nalu[j].type != NalUnitType_SPS &&
                    pack->nalu[j].type != NalUnitType_SPS_HEVC)
                    continue;

#ifdef DEBUG
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
#ifdef DEBUG
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
            if (client_fds[i].socket_fd < 0)
                continue;
            if (client_fds[i].type != STREAM_MP4)
                continue;

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
        if (client_fds[i].socket_fd < 0)
            continue;
        if (client_fds[i].type != STREAM_MP3)
            continue;

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
        if (client_fds[i].socket_fd < 0)
            continue;
        if (client_fds[i].type != STREAM_PCM)
            continue;

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

void send_mjpeg(char index, char *buf, ssize_t size) {
    static char prefix_buf[128];
    ssize_t prefix_size = sprintf(
        prefix_buf,
        "--boundarydonotcross\r\nContent-Type:image/jpeg\r\nContent-Length: "
        "%lu\r\n\r\n",
        size);
    buf[size++] = '\r';
    buf[size++] = '\n';

    pthread_mutex_lock(&client_fds_mutex);
    for (unsigned int i = 0; i < MAX_CLIENTS; ++i) {
        if (client_fds[i].socket_fd < 0)
            continue;
        if (client_fds[i].type != STREAM_MJPEG)
            continue;
        if (send_to_client(i, prefix_buf, prefix_size) < 0)
            continue; // send <SIZE>\r\n
        if (send_to_client(i, buf, size) < 0)
            continue; // send <DATA>\r\n
    }
    pthread_mutex_unlock(&client_fds_mutex);
}

void send_jpeg(char index, char *buf, ssize_t size) {
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
        if (client_fds[i].socket_fd < 0)
            continue;
        if (client_fds[i].type != STREAM_JPEG)
            continue;
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
            "HTTP/1.1 503 Internal Error\r\nContent-Length: 11\r\nConnection: "
            "close\r\n\r\nHello, 503!";
        send_to_fd(
            task.client_fd, response,
            sizeof(response) - 1); // zero ending string!
        close_socket_fd(task.client_fd);
        return NULL;
    }
    HAL_INFO("server", "JPEG snapshot has been received!\n");
    char buf[1024];
    int buf_len = sprintf(
        buf,
        "HTTP/1.1 200 OK\r\nContent-Type: image/jpeg\r\nContent-Length: "
        "%lu\r\nConnection: close\r\n\r\n",
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
    if (access(path, F_OK) != -1) { // file exists
        const char *mime = (path);
        FILE *file = fopen(path, "r");
        if (file == NULL) {
            close_socket_fd(client_fd);
            return 0;
        }
        char header[1024];
        int header_len = sprintf(
            header,
            "HTTP/1.1 200 OK\r\nContent-Type: %s\r\nTransfer-Encoding: "
            "chunked\r\nConnection: keep-alive\r\n\r\n",
            mime);
        send_to_fd(client_fd, header, header_len); // zero ending string!
        const int buf_size = 1024;
        char buf[buf_size + 2];
        char len_buf[50];
        ssize_t len_size;
        while (1) {
            ssize_t size = fread(buf, sizeof(char), buf_size, file);
            if (size <= 0) {
                break;
            }
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
        return 1;
    }
    return 0;
}

int send_mjpeg_html(const int client_fd) {
    char html[] = "<html>\n"
                  "    <head>\n"
                  "        <title>Live stream - MJPEG</title>\n"
                  "    </head>\n"
                  "    <body>\n"
                  "        <center>\n"
                  "            <img src=\"mjpeg\" />\n"
                  "        </center>\n"
                  "    </body>\n"
                  "</html>";
    char buf[1024];
    int buf_len = sprintf(
        buf,
        "HTTP/1.1 200 OK\r\nContent-Type: text/html\r\nContent-Length: "
        "%lu\r\nConnection: close\r\n\r\n%s",
        strlen(html), html);
    buf[buf_len++] = 0;
    send_to_fd(client_fd, buf, buf_len);
    close_socket_fd(client_fd);
    return 1;
}

int send_video_html(const int client_fd) {
    char html[] = "<html>\n"
                  "    <head>\n"
                  "        <title>Live stream - fMP4</title>\n"
                  "    </head>\n"
                  "    <body>\n"
                  "        <center>\n"
                  "            <video width=\"700\" src=\"video.mp4\" autoplay "
                  "controls />\n"
                  "        </center>\n"
                  "    </body>\n"
                  "</html>";
    char buf[1024];
    int buf_len = sprintf(
        buf,
        "HTTP/1.1 200 OK\r\nContent-Type: text/html\r\nContent-Length: "
        "%lu\r\nConnection: close\r\n\r\n%s",
        strlen(html), html);
    buf[buf_len++] = 0;
    send_to_fd(client_fd, buf, buf_len);
    close_socket_fd(client_fd);
    return 1;
}

#define REQSIZE 512 * 1024
char response[256];
char *method, *payload, *prot, *request, *query, *uri;
int paysize, received, total = 0;

typedef struct {
    char *name, *value;
} header_t;

header_t reqhdr[17] = {{"\0", "\0"}};

void unescape_uri(char *uri)
{
    char *src = uri;
    char *dst = uri;

    while (*src && !isspace((int)(*src)) && (*src != '%'))
        src++;

    dst = src;
    while (*src && !isspace((int)(*src)))
    {
        *dst++ = (*src == '+') ? ' ' :
                 ((*src == '%') && src[1] && src[2]) ?
                 ((*++src & 0x0F) + 9 * (*src > '9')) * 16 + ((*++src & 0x0F) + 9 * (*src > '9')) :
                 *src;
        src++;
    }
    *dst = '\0';
}

char *split(char **input, char *sep) {
    char *curr = (char *)"";
    while (curr && !curr[0] && *input) curr = strsep(input, sep);
    return (curr);
}

char *request_header(const char *name)
{
    header_t *h = reqhdr;
    for (; h->name; h++)
        if (!strcasecmp(h->name, name))
            return h->value;
    return NULL;
}

header_t *request_headers(void) { return reqhdr; }

void parse_request(int client_fd, char *request) {
    struct sockaddr_in client_sock;
    socklen_t client_sock_len = sizeof(client_sock);
    memset(&client_sock, 0, client_sock_len);

    getpeername(client_fd, 
        (struct sockaddr *)&client_sock, &client_sock_len);

    char *state = NULL;
    method = strtok_r(request, " \t\r\n", &state);
    uri = strtok_r(NULL, " \t", &state);
    prot = strtok_r(NULL, " \t\r\n", &state);

    HAL_INFO("server", "\x1b[32mNew request: (%s) %s\n"
        "         Received from: %s\x1b[0m\n",
        method, uri, inet_ntoa(client_sock.sin_addr));

    if (query = strchr(uri, '?'))
        *query++ = '\0';
    else
        query = uri - 1;

    header_t *h = reqhdr;
    char *l;
    while (h < reqhdr + 16)
    {
        char *k, *v, *e;
        k = strtok_r(NULL, "\r\n: \t", &state);
        if (!k)
            break;
        v = strtok_r(NULL, "\r\n", &state);
        while (*v && *v == ' ' && v++);
        h->name = k;
        h++->value = v;
#ifdef DEBUG
        fprintf(stderr, "         (H) %s: %s\n", k, v);
#endif
        e = v + 1 + strlen(v);
        if (e[1] == '\r' && e[2] == '\n')
            break;
    }

    l = request_header("Content-Length");
    paysize = l ? atol(l) : 0;

    while (l && total < paysize) {
        received = recv(client_fd, request + total, REQSIZE - total, 0);
        if (received < 0) {
            HAL_WARNING("server", "recv() error\n", stderr);
            break;
        } else if (!received) {
            HAL_WARNING("server", "Client disconnected unexpectedly\n", stderr);
            break;
        }
        total += received;
    }

    payload = strtok_r(NULL, "\r\n", &state);
}

void *server_thread(void *vargp) {
    int server_fd = *((int *)vargp);
    int enable = 1;
    if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &enable, sizeof(int)) < 0) {
        HAL_WARNING("server", "setsockopt(SO_REUSEADDR) failed");
        fflush(stdout);
    }
    struct sockaddr_in server, client;
    server.sin_family = AF_INET;
    server.sin_port = htons(app_config.web_port);
    server.sin_addr.s_addr = htonl(INADDR_ANY);
    int res = bind(server_fd, (struct sockaddr *)&server, sizeof(server));
    if (res != 0) {
        HAL_DANGER("server", "%s (%d)\n", strerror(errno), errno);
        keepRunning = 0;
        close_socket_fd(server_fd);
        return NULL;
    }
    listen(server_fd, 128);

    request = malloc(REQSIZE);

    while (keepRunning) {
        // waiting for a new connection
        int client_fd = accept(server_fd, NULL, NULL);
        if (client_fd == -1)
            break;

        total = 0;
        received = recv(client_fd, request, REQSIZE, 0);
        if (received < 0)
            HAL_WARNING("server", "recv() error\n", stderr);           
        else if (!received)
            HAL_WARNING("server", "Client disconnected unexpectedly\n", stderr); 
        total += received;

        if (total <= 0) continue;

        parse_request(client_fd, request);

        if (app_config.web_enable_auth) {
            char *auth = request_header("Authorization");
            char cred[65], valid[256];

            strcpy(cred, app_config.web_auth_user);
            strcpy(cred + strlen(app_config.web_auth_user), ":");
            strcpy(cred + strlen(app_config.web_auth_user) + 1, app_config.web_auth_pass);
            strcpy(valid, "Basic ");
            base64_encode(valid + 6, cred, strlen(cred));
            
            if (!auth || !equals(auth, valid)) {
                int respLen = sprintf(response,
                    "HTTP/1.1 401 Unauthorized\r\n" \
                    "Content-Type: text/plain\r\n" \
                    "WWW-Authenticate: Basic realm=\"Access the camera services\"\r\n" \
                    "Connection: close\r\n\r\n"
                );
                send_to_fd(client_fd, response, respLen);
                close_socket_fd(client_fd);
                continue;
            }
        }

        if (equals(uri, "/exit")) {
            // exit
            char response2[] = "HTTP/1.1 200 OK\r\nContent-Length: "
                              "11\r\nConnection: close\r\n\r\nClosing...";
            send_to_fd(
                client_fd, response2,
                sizeof(response2) - 1);
            close_socket_fd(client_fd);
            keepRunning = 0;
            break;
        }

        if (equals(uri, "/mjpeg.html") &&
            app_config.mjpeg_enable) {
            send_mjpeg_html(client_fd);
            continue;
        }

        if (equals(uri, "/video.html") &&
            app_config.mp4_enable) {
            send_video_html(client_fd);
            continue;
        }

        if (app_config.audio_enable && equals(uri, "/audio.mp3")) {
            int respLen = sprintf(
                response, "HTTP/1.1 200 OK\r\nContent-Type: "
                        "audio/mpeg\r\nTransfer-Encoding: "
                        "chunked\r\nConnection: keep-alive\r\n\r\n");
            send_to_fd(client_fd, response, respLen);
            pthread_mutex_lock(&client_fds_mutex);
            for (uint32_t i = 0; i < MAX_CLIENTS; ++i)
                if (client_fds[i].socket_fd < 0) {
                    client_fds[i].socket_fd = client_fd;
                    client_fds[i].type = STREAM_MP3;
                    break;
                }
            pthread_mutex_unlock(&client_fds_mutex);
            continue;
        }

        if (app_config.audio_enable && equals(uri, "/audio.pcm")) {
            int respLen = sprintf(
                response, "HTTP/1.1 200 OK\r\nContent-Type: "
                        "audio/pcm\r\nTransfer-Encoding: "
                        "chunked\r\nConnection: keep-alive\r\n\r\n");
            send_to_fd(client_fd, response, respLen);
            pthread_mutex_lock(&client_fds_mutex);
            for (uint32_t i = 0; i < MAX_CLIENTS; ++i)
                if (client_fds[i].socket_fd < 0) {
                    client_fds[i].socket_fd = client_fd;
                    client_fds[i].type = STREAM_PCM;
                    break;
                }
            pthread_mutex_unlock(&client_fds_mutex);
            continue;
        }

        if ((!app_config.mp4_codecH265 && equals(uri, "/video.264")) ||
            (app_config.mp4_codecH265 && equals(uri, "/video.265"))) {
            request_idr();
            int respLen = sprintf(
                response, "HTTP/1.1 200 OK\r\nContent-Type: "
                        "application/octet-stream\r\nTransfer-Encoding: "
                        "chunked\r\nConnection: keep-alive\r\n\r\n");
            send_to_fd(client_fd, response, respLen);
            pthread_mutex_lock(&client_fds_mutex);
            for (uint32_t i = 0; i < MAX_CLIENTS; ++i)
                if (client_fds[i].socket_fd < 0) {
                    client_fds[i].socket_fd = client_fd;
                    client_fds[i].type = STREAM_H26X;
                    client_fds[i].nalCnt = 0;
                    break;
                }
            pthread_mutex_unlock(&client_fds_mutex);
            continue;
        }

        if (equals(uri, "/video.mp4") && app_config.mp4_enable) {
            request_idr();
            int respLen = sprintf(
                response, "HTTP/1.1 200 OK\r\nContent-Type: "
                        "video/mp4\r\nTransfer-Encoding: "
                        "chunked\r\nConnection: keep-alive\r\n\r\n");
            send_to_fd(client_fd, response, respLen);
            pthread_mutex_lock(&client_fds_mutex);
            for (uint32_t i = 0; i < MAX_CLIENTS; ++i)
                if (client_fds[i].socket_fd < 0) {
                    client_fds[i].socket_fd = client_fd;
                    client_fds[i].type = STREAM_MP4;
                    client_fds[i].mp4.header_sent = false;
                    break;
                }
            pthread_mutex_unlock(&client_fds_mutex);
            continue;
        }

        if (app_config.mjpeg_enable && equals(uri, "/mjpeg")) {
            int respLen = sprintf(
                response, "HTTP/1.0 200 OK\r\nCache-Control: no-cache\r\nPragma: "
                        "no-cache\r\nConnection: close\r\nContent-Type: "
                        "multipart/x-mixed-replace; "
                        "boundary=boundarydonotcross\r\n\r\n");
            send_to_fd(client_fd, response, respLen);
            pthread_mutex_lock(&client_fds_mutex);
            for (uint32_t i = 0; i < MAX_CLIENTS; ++i)
                if (client_fds[i].socket_fd < 0) {
                    client_fds[i].socket_fd = client_fd;
                    client_fds[i].type = STREAM_MJPEG;
                    break;
                }
            pthread_mutex_unlock(&client_fds_mutex);
            continue;
        }

        if (app_config.jpeg_enable && starts_with(uri, "/image.jpg")) {
            {
                struct jpegtask task;
                task.client_fd = client_fd;
                task.width = app_config.jpeg_width;
                task.height = app_config.jpeg_height;
                task.qfactor = app_config.jpeg_qfactor;
                task.color2Gray = 0;

                if (!empty(query)) {
                    char *remain;
                    while (query) {
                        char *value = split(&query, "&");
                        if (!value || !*value) continue;
                        char *key = split(&value, "=");
                        if (!key || !*key || !value || !*value) continue;
                        if (equals(key, "width")) {
                            short result = strtol(value, &remain, 10);
                            if (remain != value)
                                task.width = result;
                        }
                        else if (equals(key, "height")) {
                            short result = strtol(value, &remain, 10);
                            if (remain != value)
                                task.height = result;
                        }
                        else if (equals(key, "qfactor")) {
                            short result = strtol(value, &remain, 10);
                            if (remain != value)
                                task.qfactor = result;
                        }
                        else if (equals(key, "color2gray")) {
                            short result = strtol(value, &remain, 10);
                            if (remain != value)
                                task.color2Gray = result;
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
            continue;
        }

        if (app_config.audio_enable && equals(uri, "/api/audio")) {
            int respLen;
            if (equals(method, "GET")) {
                if (!empty(query)) {
                    char *remain;
                    while (query) {
                        char *value = split(&query, "&");
                        if (!value || !*value) continue;
                        unescape_uri(value);
                        char *key = split(&value, "=");
                        if (!key || !*key || !value || !*value) continue;
                        if (equals(key, "bitrate")) {
                            short result = strtol(value, &remain, 10);
                            if (remain != value)
                                app_config.audio_bitrate = result;
                        } else if (equals(key, "srate")) {
                            short result = strtol(value, &remain, 10);
                            if (remain != value)
                                app_config.audio_srate = result;
                        }
                    }
                }

                disable_audio();
                enable_audio();

                respLen = sprintf(response,
                    "HTTP/1.1 200 OK\r\n" \
                    "Content-Type: application/json;charset=UTF-8\r\n" \
                    "Connection: close\r\n" \
                    "\r\n" \
                    "{\"bitrate\":%d,\"srate\":%d}", 
                    app_config.audio_bitrate, app_config.audio_srate);
            } else {
                respLen = sprintf(response,
                    "HTTP/1.1 400 Bad Request\r\n" \
                    "Content-Type: text/plain\r\n" \
                    "Connection: close\r\n" \
                    "\r\n" \
                    "The server has no handler to the request.\r\n" \
                );
            }
            send_to_fd(client_fd, response, respLen);
            close_socket_fd(client_fd);
            continue;
        }

        if (app_config.jpeg_enable && equals(uri, "/api/jpeg")) {
            int respLen;
            if (equals(method, "GET")) {
                if (!empty(query)) {
                    char *remain;
                    while (query) {
                        char *value = split(&query, "&");
                        if (!value || !*value) continue;
                        unescape_uri(value);
                        char *key = split(&value, "=");
                        if (!key || !*key || !value || !*value) continue;
                        if (equals(key, "width")) {
                            short result = strtol(value, &remain, 10);
                            if (remain != value)
                                app_config.jpeg_width = result;
                        } else if (equals(key, "height")) {
                            short result = strtol(value, &remain, 10);
                            if (remain != value)
                                app_config.jpeg_height = result;
                        } else if (equals(key, "qfactor")) {
                            short result = strtol(value, &remain, 10);
                            if (remain != value)
                                app_config.jpeg_qfactor = result;
                        }
                    }
                }

                jpeg_deinit();
                jpeg_init();

                respLen = sprintf(response,
                    "HTTP/1.1 200 OK\r\n" \
                    "Content-Type: application/json;charset=UTF-8\r\n" \
                    "Connection: close\r\n" \
                    "\r\n" \
                    "{\"width\":%d,\"height\":%d,\"qfactor\":%d}", 
                    app_config.jpeg_width, app_config.jpeg_height, app_config.jpeg_qfactor);
            } else {
                respLen = sprintf(response,
                    "HTTP/1.1 400 Bad Request\r\n" \
                    "Content-Type: text/plain\r\n" \
                    "Connection: close\r\n" \
                    "\r\n" \
                    "The server has no handler to the request.\r\n" \
                );
            }
            send_to_fd(client_fd, response, respLen);
            close_socket_fd(client_fd);
            continue;
        }

        if (app_config.mjpeg_enable && equals(uri, "/api/mjpeg")) {
            int respLen;
            if (equals(method, "GET")) {
                if (!empty(query)) {
                    char *remain;
                    while (query) {
                        char *value = split(&query, "&");
                        if (!value || !*value) continue;
                        unescape_uri(value);
                        char *key = split(&value, "=");
                        if (!key || !*key || !value || !*value) continue;
                        if (equals(key, "width")) {
                            short result = strtol(value, &remain, 10);
                            if (remain != value)
                                app_config.mjpeg_width = result;
                        } else if (equals(key, "height")) {
                            short result = strtol(value, &remain, 10);
                            if (remain != value)
                                app_config.mjpeg_height = result;
                        } else if (equals(key, "fps")) {
                            short result = strtol(value, &remain, 10);
                            if (remain != value)
                                app_config.mjpeg_fps = result;
                        } else if (equals(key, "mode")) {
                            if (equals_case(value, "CBR"))
                                app_config.mjpeg_mode = HAL_VIDMODE_CBR;
                            else if (equals_case(value, "VBR"))
                                app_config.mjpeg_mode = HAL_VIDMODE_VBR;
                            else if (equals_case(value, "QP"))
                                app_config.mjpeg_mode = HAL_VIDMODE_QP;
                        }
                    }
                }

                disable_mjpeg();
                enable_mjpeg();

                char mode[5] = "\0";
                switch (app_config.mjpeg_mode) {
                    case HAL_VIDMODE_CBR: strcpy(mode, "CBR"); break;
                    case HAL_VIDMODE_VBR: strcpy(mode, "VBR"); break;
                    case HAL_VIDMODE_QP: strcpy(mode, "QP"); break;
                }
                respLen = sprintf(response,
                    "HTTP/1.1 200 OK\r\n" \
                    "Content-Type: application/json;charset=UTF-8\r\n" \
                    "Connection: close\r\n" \
                    "\r\n" \
                    "{\"width\":%d,\"height\":%d,\"fps\":%d,\"mode\":\"%s\",\"bitrate\":%d}", 
                    app_config.mjpeg_width, app_config.mjpeg_height, app_config.mjpeg_fps, mode,
                    app_config.mjpeg_bitrate);
            } else {
                respLen = sprintf(response,
                    "HTTP/1.1 400 Bad Request\r\n" \
                    "Content-Type: text/plain\r\n" \
                    "Connection: close\r\n" \
                    "\r\n" \
                    "The server has no handler to the request.\r\n" \
                );
            }
            send_to_fd(client_fd, response, respLen);
            close_socket_fd(client_fd);
            continue;
        }

        if (app_config.mp4_enable && equals(uri, "/api/mp4")) {
            int respLen;
            if (equals(method, "GET")) {
                if (!empty(query)) {
                    char *remain;
                    while (query) {
                        char *value = split(&query, "&");
                        if (!value || !*value) continue;
                        unescape_uri(value);
                        char *key = split(&value, "=");
                        if (!key || !*key || !value || !*value) continue;
                        if (equals(key, "width")) {
                            short result = strtol(value, &remain, 10);
                            if (remain != value)
                                app_config.mp4_width = result;
                        } else if (equals(key, "height")) {
                            short result = strtol(value, &remain, 10);
                            if (remain != value)
                                app_config.mp4_height = result;
                        } else if (equals(key, "fps")) {
                            short result = strtol(value, &remain, 10);
                            if (remain != value)
                                app_config.mp4_fps = result;
                        } else if (equals(key, "bitrate")) {
                            short result = strtol(value, &remain, 10);
                            if (remain != value)
                                app_config.mp4_bitrate = result;
                        } else if (equals(key, "h265")) {
                            if (equals_case(value, "true") || equals(value, "1"))
                                app_config.mp4_codecH265 = 1;
                            else if (equals_case(value, "false") || equals(value, "0"))
                                app_config.mp4_codecH265 = 0;
                        } else if (equals(key, "mode")) {
                            if (equals_case(value, "CBR"))
                                app_config.mp4_mode = HAL_VIDMODE_CBR;
                            else if (equals_case(value, "VBR"))
                                app_config.mp4_mode = HAL_VIDMODE_VBR;
                            else if (equals_case(value, "QP"))
                                app_config.mp4_mode = HAL_VIDMODE_QP;
                            else if (equals_case(value, "ABR"))
                                app_config.mp4_mode = HAL_VIDMODE_ABR;
                            else if (equals_case(value, "AVBR"))
                                app_config.mp4_mode = HAL_VIDMODE_AVBR;
                        } else if (equals(key, "profile")) {
                            if (equals_case(value, "BP") || equals_case(value, "BASELINE"))
                                app_config.mp4_profile = HAL_VIDPROFILE_BASELINE;
                            else if (equals_case(value, "MP") || equals_case(value, "MAIN"))
                                app_config.mp4_profile = HAL_VIDPROFILE_MAIN;
                            else if (equals_case(value, "HP") || equals_case(value, "HIGH"))
                                app_config.mp4_profile = HAL_VIDPROFILE_HIGH;
                        }
                    }
                }

                disable_mp4();
                enable_mp4();

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
                    "HTTP/1.1 200 OK\r\n" \
                    "Content-Type: application/json;charset=UTF-8\r\n" \
                    "Connection: close\r\n" \
                    "\r\n" \
                    "{\"width\":%d,\"height\":%d,\"fps\":%d,\"h265\":%s,\"mode\":\"%s\",\"profile\":\"%s\",\"bitrate\":%d}", 
                    app_config.mp4_width, app_config.mp4_height, app_config.mp4_fps, h265, mode,
                    profile, app_config.mp4_bitrate);
            } else {
                respLen = sprintf(response,
                    "HTTP/1.1 400 Bad Request\r\n" \
                    "Content-Type: text/plain\r\n" \
                    "Connection: close\r\n" \
                    "\r\n" \
                    "The server has no handler to the request.\r\n" \
                );
            }
            send_to_fd(client_fd, response, respLen);
            close_socket_fd(client_fd);
            continue;
        }

        if (app_config.night_mode_enable && equals(uri, "/api/night")) {
            int respLen;
            if (equals(method, "GET")) {
                if (app_config.ir_sensor_pin == 999 && !empty(query)) {
                    char *remain;
                    while (query) {
                        char *value = split(&query, "&");
                        if (!value || !*value) continue;
                        unescape_uri(value);
                        char *key = split(&value, "=");
                        if (!key || !*key || !value || !*value) continue;
                        if (equals(key, "active")) {
                            if (equals_case(value, "true") || equals(value, "1"))
                                set_night_mode(1);
                            else if (equals_case(value, "false") || equals(value, "0"))
                                set_night_mode(0);
                        }
                    }
                }
                respLen = sprintf(response,
                    "HTTP/1.1 200 OK\r\n" \
                    "Content-Type: application/json;charset=UTF-8\r\n" \
                    "Connection: close\r\n" \
                    "\r\n" \
                    "{\"active\":%s}", 
                    night_mode_is_enabled() ? "true" : "false");
            } else {
                respLen = sprintf(response,
                    "HTTP/1.1 400 Bad Request\r\n" \
                    "Content-Type: text/plain\r\n" \
                    "Connection: close\r\n" \
                    "\r\n" \
                    "The server has no handler to the request.\r\n" \
                );
            }
            send_to_fd(client_fd, response, respLen);
            close_socket_fd(client_fd);
            continue;
        }

        if (app_config.osd_enable && starts_with(uri, "/api/osd/") &&
            uri[9] && uri[9] >= '0' && uri[9] <= (MAX_OSD - 1 + '0')) {
            char id = uri[9] - '0';
            int respLen;
            if (equals(method, "POST")) {
                char *type = request_header("Content-Type");
                if (starts_with(type, "multipart/form-data")) {
                    char *bound = strstr(type, "boundary=") + strlen("boundary=");

                    char *payloadb = strstr(payload, bound);
                    payloadb = memstr(payloadb, "\r\n\r\n", total - (payloadb - request), 4);
                    if (payloadb) payloadb += 4;

                    char *payloade = memstr(payloadb, bound, 
                        total - (payloadb - request), strlen(bound));
                    if (payloade) payloade -= 4;

                    char path[32];
                    sprintf(path, "/tmp/osd%d.bmp", id);
                    FILE *img = fopen(path, "wb");
                    fwrite(payloadb, sizeof(char), payloade - payloadb, img);
                    fclose(img);

                    strcpy(osds[id].text, "");
                    osds[id].updt = 1;

                    respLen = sprintf(response,
                        "HTTP/1.1 200 OK\r\n" \
                        "Connection: close\r\n" \
                        "\r\n" \
                    );
                } else {
                    respLen = sprintf(response,
                        "HTTP/1.1 415 Unsupported Media Type\r\n" \
                        "Content-Type: text/plain\r\n" \
                        "Connection: close\r\n" \
                        "\r\n" \
                        "The payload must be presented as multipart/form-data.\r\n" \
                    );
                }
                send_to_fd(client_fd, response, respLen);
                close_socket_fd(client_fd);
                continue;
            }
            else if (!empty(query))
            {
                char *remain;
                while (query) {
                    char *value = split(&query, "&");
                    if (!value || !*value) continue;
                    unescape_uri(value);
                    char *key = split(&value, "=");
                    if (!key || !*key || !value || !*value) continue;
                    if (equals(key, "font"))
                        strcpy(osds[id].font, !empty(value) ? value : DEF_FONT);
                    else if (equals(key, "text"))
                        strcpy(osds[id].text, value);
                    else if (equals(key, "size")) {
                        double result = strtod(value, &remain);
                        if (remain == value) continue;
                        osds[id].size = (result != 0 ? result : DEF_SIZE);
                    }
                    else if (equals(key, "color")) {
                        char base = 16;
                        if (strlen(value) > 1 && value[1] == 'x') base = 0;
                        short result = strtol(value, &remain, base);
                        if (remain != value)
                            osds[id].color = result;
                    }
                    else if (equals(key, "opal")) {
                        short result = strtol(value, &remain, 10);
                        if (remain != value)
                            osds[id].opal = result & 0xFF;
                    }
                    else if (equals(key, "posx")) {
                        short result = strtol(value, &remain, 10);
                        if (remain != value)
                            osds[id].posx = result;
                    }
                    else if (equals(key, "posy")) {
                        short result = strtol(value, &remain, 10);
                        if (remain != value)
                            osds[id].posy = result;
                    }
                }
                osds[id].updt = 1;
            }
            respLen = sprintf(response,
                "HTTP/1.1 200 OK\r\n" \
                "Content-Type: application/json;charset=UTF-8\r\n" \
                "Connection: close\r\n" \
                "\r\n" \
                "{\"id\":%d,\"color\":%#x,\"opal\":%d\"pos\":[%d,%d],\"font\":\"%s\",\"size\":%.1f,\"text\":\"%s\"}", 
                id, osds[id].color, osds[id].opal, osds[id].posx, osds[id].posy, osds[id].font, osds[id].size, osds[id].text);
            send_to_fd(client_fd, response, respLen);
            close_socket_fd(client_fd);
            continue;
        }

        if (starts_with(uri, "/api/time")) {
            int respLen;
            if (equals(method, "GET")) {
                struct timespec t;
                if (!empty(query)) {
                    char *remain;
                    while (query) {
                        char *value = split(&query, "&");
                        if (!value || !*value) continue;
                        unescape_uri(value);
                        char *key = split(&value, "=");
                        if (!key || !*key || !value || !*value) continue;
                        if (equals(key, "fmt")) {
                            strncpy(timefmt, value, 32);
                        } else if (equals(key, "ts")) {
                            short result = strtol(value, &remain, 10);
                            if (remain == value) continue;
                            t.tv_sec = result;
                            clock_settime(CLOCK_REALTIME, &t);
                        }
                    }
                }
                clock_gettime(CLOCK_REALTIME, &t);
                respLen = sprintf(response,
                    "HTTP/1.1 200 OK\r\n" \
                    "Content-Type: application/json;charset=UTF-8\r\n" \
                    "Connection: close\r\n" \
                    "\r\n" \
                    "{\"fmt\":\"%s\",\"ts\":%d}", timefmt, t.tv_sec);
            } else {
                respLen = sprintf(response,
                    "HTTP/1.1 400 Bad Request\r\n" \
                    "Content-Type: text/plain\r\n" \
                    "Connection: close\r\n" \
                    "\r\n" \
                    "The server has no handler to the request.\r\n" \
                );
            }
            send_to_fd(client_fd, response, respLen);
            close_socket_fd(client_fd);
            continue;
        }

        if (app_config.web_enable_static && send_file(client_fd, uri))
            continue;

        static char response2[] = "HTTP/1.1 404 Not Found\r\nContent-Length: "
                                 "11\r\nConnection: close\r\n\r\n";
        send_to_fd(
            client_fd, response2, sizeof(response2) - 1);
        close_socket_fd(client_fd);
    }

    if (request)
        free(request);

    close_socket_fd(server_fd);
    HAL_INFO("server", "Thread has exited\n");
    return NULL;
}

void sig_handler(int signo) {
    HAL_INFO("server", "Graceful shutdown...\n");
    keepRunning = 0;
}
void epipe_handler(int signo) { printf("EPIPE\n"); }
void spipe_handler(int signo) { printf("SIGPIPE\n"); }

int server_fd = -1;
pthread_t server_thread_id;

int start_server() {
    signal(SIGINT, sig_handler);
    signal(SIGQUIT, sig_handler);
    signal(SIGTERM, sig_handler);

    signal(SIGPIPE, spipe_handler);
    signal(EPIPE, epipe_handler);

    if (app_config.watchdog)
        watchdog_start(app_config.watchdog);

    for (uint32_t i = 0; i < MAX_CLIENTS; ++i) {
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

    if (app_config.watchdog)
        watchdog_stop();

    return EXIT_SUCCESS;
}
