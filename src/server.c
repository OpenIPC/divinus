#include "server.h"

#define tag "[server] "

char keepRunning = 1;

enum StreamType { STREAM_H26X, STREAM_JPEG, STREAM_MJPEG, STREAM_MP4 };

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

void send_h26x_to_client(unsigned char index, const void *p) {
    const hal_vidstream *stream = (const hal_vidstream *)p;

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

void send_mp4_to_client(unsigned char index, const void *p, char isH265) {
    const hal_vidstream *stream = (const hal_vidstream *)p;

    for (unsigned int i = 0; i < stream->count; ++i) {
        hal_vidpack *pack = &stream->pack[i];
        unsigned int pack_len = pack->length - pack->offset;
        unsigned char *pack_data = pack->data + pack->offset;

        for (char j = 0; j < pack->naluCnt; j++) {
#ifdef DEBUG
            printf("NAL: %s received in packet %d\n", nal_type_to_str(pack->nalu[j].type), i);
            printf("     starts at %p, ends at %p\n", pack_data + pack->nalu[j].offset, pack_data + pack->nalu[j].length);
#endif

            if ((pack->nalu[j].type == NalUnitType_SPS || pack->nalu[j].type == NalUnitType_SPS_HEVC) 
                && pack->nalu[j].length >= 4 && pack->nalu[j].length <= UINT16_MAX)
                set_sps(pack_data + pack->nalu[j].offset, pack->nalu[j].length, isH265);
            else if ((pack->nalu[j].type == NalUnitType_PPS || pack->nalu[j].type == NalUnitType_PPS_HEVC)
                && pack->nalu[j].length <= UINT16_MAX)
                set_pps(pack_data + pack->nalu[j].offset, pack->nalu[j].length, isH265);
            else if (pack->nalu[j].type == NalUnitType_VPS_HEVC && pack->nalu[j].length <= UINT16_MAX)
                set_vps(pack_data + pack->nalu[j].offset, pack->nalu[j].length);
            else if (pack->nalu[j].type == NalUnitType_CodedSliceIdr || pack->nalu[j].type == NalUnitType_CodedSliceAux)
                set_slice(pack_data + pack->nalu[j].offset, pack->nalu[j].length, isH265);
            else if (pack->nalu[j].type == NalUnitType_CodedSliceNonIdr)
                set_slice(pack_data + pack->nalu[j].offset, pack->nalu[j].length, isH265);
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
                err = get_header(&header_buf);
                chk_err_continue ssize_t len_size =
                    sprintf(len_buf, "%zX\r\n", header_buf.offset);
                if (send_to_client(i, len_buf, len_size) < 0)
                    continue; // send <SIZE>\r\n
                if (send_to_client(i, header_buf.buf, header_buf.offset) < 0)
                    continue; // send <DATA>
                if (send_to_client(i, "\r\n", 2) < 0)
                    continue; // send \r\n

                client_fds[i].mp4.sequence_number = 1;
                client_fds[i].mp4.base_data_offset = header_buf.offset;
                client_fds[i].mp4.base_media_decode_time = 0;
                client_fds[i].mp4.header_sent = true;
                client_fds[i].mp4.nals_count = 0;
                client_fds[i].mp4.default_sample_duration =
                    default_sample_size;
            }

            err = set_mp4_state(&client_fds[i].mp4);
            chk_err_continue {
                struct BitBuf moof_buf;
                err = get_moof(&moof_buf);
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
                err = get_mdat(&mdat_buf);
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

void send_mjpeg(unsigned char index, char *buf, ssize_t size) {
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

void send_jpeg(unsigned char index, char *buf, ssize_t size) {
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
    printf(
        tag "Requesting a JPEG snapshot (%ux%u, qfactor %u, color2Gray %d)...\n",
        task.width, task.height, task.qfactor, task.color2Gray);
    int ret =
        jpeg_get(task.width, task.height, task.qfactor, task.color2Gray, &jpeg);
    if (ret) {
        printf("Failed to receive a JPEG snapshot...\n");
        static char response[] =
            "HTTP/1.1 503 Internal Error\r\nContent-Length: 11\r\nConnection: "
            "close\r\n\r\nHello, 503!";
        send_to_fd(
            task.client_fd, response,
            sizeof(response) - 1); // zero ending string!
        close_socket_fd(task.client_fd);
        return NULL;
    }
    printf(tag "JPEG snapshot has been received!\n");
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
    printf(tag "JPEG snapshot has been sent!\n");
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

int send_image_html(const int client_fd) {
    char html[] = "<html>\n"
                  "    <head>\n"
                  "        <title>Snapshot</title>\n"
                  "    </head>\n"
                  "    <body>\n"
                  "        <center>\n"
                  "            <img src=\"image.jpg\"/>\n"
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

#define MAX_REQSIZE 8192
char request[MAX_REQSIZE], response[256];
char *method, *prot, *query, *uri;

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

void parse_request(char *request) {
    method = strtok(request, " \t\r\n");
    uri = strtok(NULL, " \t");
    prot = strtok(NULL, " \t\r\n");

    fprintf(stderr, tag "\x1b[32m New request: (%s) %s\x1b[0m\n", method, uri);

    if (query = strchr(uri, '?'))
        *query++ = '\0';
    else
        query = uri - 1;

    header_t *h = reqhdr;
    while (h < reqhdr + 16)
    {
        char *k, *v, *e;
        k = strtok(NULL, "\r\n: \t");
        if (!k)
            break;
        v = strtok(NULL, "\r\n");
        while (*v && *v == ' ' && v++);
        h->name = k;
        h++->value = v;
        fprintf(stderr, "         (H) %s: %s\n", k, v);
        e = v + 1 + strlen(v);
        if (e[1] == '\r' && e[2] == '\n')
            break;
    }
}

char *request_header(const char *name)
{
    header_t *h = reqhdr;
    for (; h->name; h++)
        if (!strcmp(h->name, name))
            return h->value;
    return NULL;
}

header_t *request_headers(void) { return reqhdr; }

void *server_thread(void *vargp) {
    int server_fd = *((int *)vargp);
    int enable = 1;
    if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &enable, sizeof(int)) <
        0) {
        printf(tag "setsockopt(SO_REUSEADDR) failed");
        fflush(stdout);
    }
    struct sockaddr_in server;
    server.sin_family = AF_INET;
    server.sin_port = htons(app_config.web_port);
    server.sin_addr.s_addr = htonl(INADDR_ANY);
    int res = bind(server_fd, (struct sockaddr *)&server, sizeof(server));
    if (res != 0) {
        printf(tag "%s (%d)\n", strerror(errno), errno);
        keepRunning = 0;
        close_socket_fd(server_fd);
        return NULL;
    }
    listen(server_fd, 128);

    while (keepRunning) {
        // waiting for a new connection
        int client_fd = accept(server_fd, NULL, NULL);
        if (client_fd == -1)
            break;

        recv(client_fd, request, MAX_REQSIZE, 0);
        parse_request(request);

        if (equals(uri, "/exit")) {
            // exit
            char response2[] = "HTTP/1.1 200 OK\r\nContent-Length: "
                              "11\r\nConnection: close\r\n\r\nClosing...";
            send_to_fd(
                client_fd, response2,
                sizeof(response2) - 1); // zero ending string!
            close_socket_fd(client_fd);
            keepRunning = 0;
            break;
        }

        // send JPEG html page
        if (equals(uri, "/image.html") &&
            app_config.jpeg_enable) {
            send_image_html(client_fd);
            continue;
        }
        // send MJPEG html page
        if (equals(uri, "/mjpeg.html") &&
            app_config.mjpeg_enable) {
            send_mjpeg_html(client_fd);
            continue;
        }
        // send MP4 html page
        if (equals(uri, "/video.html") &&
            app_config.mp4_enable) {
            send_video_html(client_fd);
            continue;
        }

        // if h26x stream is requested add client_fd socket to client_fds array
        // and send h26x stream with http_thread
        if (equals(uri, "/video.264") || equals(uri, "/video.265")) {
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

        // If the MJPEG stream is requested add client_fd socket to client_fds array
        // and send it with the HTTP thread
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
                if (pthread_attr_setstacksize(&thread_attr, new_stacksize)) {
                    printf("[jpeg] Can't set stack size %ld\n", new_stacksize);
                }
                pthread_create(
                    &thread_id, &thread_attr, send_jpeg_thread, (void *)&task);
                if (pthread_attr_setstacksize(&thread_attr, stacksize)) {
                    printf("[jpeg] Can't set stack size %ld\n", stacksize);
                }
                pthread_attr_destroy(&thread_attr);
            }
            continue;
        }

        if (app_config.osd_enable && starts_with(uri, "/api/osd/") &&
            uri[9] && uri[9] >= '0' && uri[9] <= (MAX_OSD - 1 + '0'))
        {
            char id = uri[9] - '0';
            if (!empty(query))
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
            int respLen = sprintf(response,
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

        if (app_config.web_enable_static && send_file(client_fd, uri))
            continue;

        static char response2[] = "HTTP/1.1 404 Not Found\r\nContent-Length: "
                                 "11\r\nConnection: close\r\n\r\n";
        send_to_fd(
            client_fd, response2, sizeof(response2) - 1); // zero ending string!
        close_socket_fd(client_fd);
    }
    close_socket_fd(server_fd);
    printf(tag "Thread has exited\n");
    return NULL;
}

void sig_handler(int signo) {
    printf(tag "Graceful shutdown...\n");
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
        size_t new_stacksize = app_config.web_server_thread_stack_size;
        if (pthread_attr_setstacksize(&thread_attr, new_stacksize)) {
            printf(tag "Can't set stack size %zu\n", new_stacksize);
        }
        pthread_create(
            &server_thread_id, &thread_attr, server_thread, (void *)&server_fd);
        if (pthread_attr_setstacksize(&thread_attr, stacksize)) {
            printf(tag "Can't set stack size %zu\n", stacksize);
        }
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
    printf(tag "Shutting down server...\n");
    return EXIT_SUCCESS;
}
