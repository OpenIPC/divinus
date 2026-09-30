#include <sys/socket.h>
#include <sys/types.h>
#include <sys/select.h>
#include <fcntl.h>
#include <pthread.h>
#include <unistd.h>

#ifndef SO_SNDBUFFORCE
#define SO_SNDBUFFORCE SO_SNDBUF
#endif

#include "check.h"
#undef CHECK
#include "../src/rtsp/rtsp.c"
#undef CHECK
#define CHECK(cond) \
    do { \
        if (!(cond)) { \
            fprintf(stderr, "%s:%d: CHECK failed: %s\n", __FILE__, __LINE__, #cond); \
            checkFailures++; \
        } \
    } while (0)

void request_idr(void) {}

static rtsp_handle make_server(int max_con)
{
    rtsp_handle h = calloc(1, sizeof(*h));
    pthread_mutex_init(&h->mutex, NULL);
    h->audioPt = 255;
    h->max_con = max_con;
    h->con_pool = __connectionpool_create(max_con);
    h->transfer_pool = __transpool_create(max_con);
    return h;
}

/* Returns the client end of a socketpair accepted by the server */
static int connect_client(rtsp_handle h, struct connection_item_t **con)
{
    int sv[2];
    struct sockaddr_in addr = {};

    if (socketpair(AF_UNIX, SOCK_STREAM, 0, sv)) return -1;
    fcntl(sv[0], F_SETFL, fcntl(sv[0], F_GETFL) | O_NONBLOCK);
    if (__connection_list_add(h->con_pool, &h->con_list, sv[0], addr) != SUCCESS) return -1;
    for (int i = 0; i < h->max_con; i++)
        if (__connection_pool[i].client_fd == sv[0]) *con = &__connection_pool[i];
    return sv[1];
}

static void serve_once(rtsp_handle h)
{
    struct sock_select_t socks = {};
    struct timeval tv = { .tv_sec = 1 };

    socks.h_rtsp = h;
    FD_ZERO(&socks.rfds);
    list_map_inline(&h->con_list, __set_select_sock, &socks);
    select(__find_fd_max(&h->con_list) + 1, &socks.rfds, NULL, NULL, &tv);
    list_map_inline(&h->con_list, __message_proc_sock, &socks);
    list_sweep(&h->con_list, __connection_is_dead);
}

static int pool_ref(rtsp_handle h, struct connection_item_t *con)
{
    return h->con_pool->elems[con - __connection_pool].ref_count;
}

static void send_str(int fd, const char *s)
{
    if (write(fd, s, strlen(s)) != (ssize_t)strlen(s)) perror("write");
}

/* A request cut short by the peer must drop the connection exactly once */
static void test_truncated_request(void)
{
    const char *requests[] = {
        "TEARDOWN rtsp://cam/ RTSP/1.0\r\nCSeq: 2\r\n",
        "GARBAGE rtsp://cam/ RTSP/1.0\r\nCSeq: 2\r\n",
    };

    for (int i = 0; i < 2; i++) {
        rtsp_handle h = make_server(1);
        struct connection_item_t *con = NULL;
        int peer = connect_client(h, &con);
        CHECK(peer >= 0 && con);

        send_str(peer, requests[i]);
        shutdown(peer, SHUT_WR);
        serve_once(h);

        CHECK(con->con_state == __CON_S_DISCONNECTED);
        CHECK(pool_ref(h, con) == 0);
        CHECK(h->con_list.list == NULL);
        CHECK(list_length(&h->con_pool->free_list) == 1);
        close(peer);
    }
}

int main(void)
{
    test_truncated_request();
    CHECK_DONE();
}
