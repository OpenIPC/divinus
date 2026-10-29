#pragma once

#include <errno.h>
#include <fcntl.h>
#include <poll.h>
#include <pthread.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <linux/videodev2.h>

#define CAP_BUFS 4
#define ENC_BUFS 2

int ak_v4l2_cap_open(int index, int width, int height);
int ak_v4l2_cap_streamon(int index);
void ak_v4l2_cap_close(int index);
unsigned int ak_v4l2_cap_gen(int index);
extern pthread_mutex_t ak_v4l2_mtx;
int ak_v4l2_cap_dequeue(int index, void **data, size_t *len, uint64_t *ts, int *bufIndex, int timeout_ms);
int ak_v4l2_cap_requeue(int index, int bufIndex);

int ak_v4l2_enc_open(int index, int width, int height, int codec,
                     int fps, int bitrate, int gop);
void ak_v4l2_enc_close(int index);
int ak_v4l2_enc_streamon(int index);
int ak_v4l2_enc_streamoff(int index);
int ak_v4l2_enc_push(int index, const void *data, size_t len,
                     void **out, size_t *outlen, int timeout_ms);
int ak_v4l2_enc_requeue(int index);
int ak_v4l2_enc_request_idr(int index);
int ak_v4l2_enc_grayscale(int index, int on);
int ak_v4l2_snapshot(int width, int height, int quality,
                     void **data, size_t *len);

struct v4l2_buf {
    void *addr;
    size_t len;
};

struct v4l2_cap {
    int fd, streaming, width, height;
    struct v4l2_buf buf[CAP_BUFS];
};

struct v4l2_enc {
    int fd, streaming, width, height, outCur, capCur, outCnt, capCnt;
    struct v4l2_buf out[ENC_BUFS];
    struct v4l2_buf cap[ENC_BUFS];
};

static struct v4l2_cap caps[2] = { { .fd = -1 }, { .fd = -1 } };
static struct v4l2_enc encs[2] = { { .fd = -1 }, { .fd = -1 } };

static int xioctl(int fd, unsigned long req, void *arg)
{
    int r;
    do { r = ioctl(fd, req, arg); } while (r < 0 && errno == EINTR);
    return r;
}

static void munmap_bufs(struct v4l2_buf *bufs, int n)
{
    for (int i = 0; i < n; i++) {
        if (bufs[i].addr && bufs[i].addr != MAP_FAILED)
            munmap(bufs[i].addr, bufs[i].len);
        bufs[i].addr = NULL;
        bufs[i].len = 0;
    }
}

static int mmap_bufs(int fd, unsigned type, struct v4l2_buf *bufs, int n)
{
    for (int i = 0; i < n; i++) {
        struct v4l2_buffer b;
        memset(&b, 0, sizeof(b));
        b.type = type;
        b.memory = V4L2_MEMORY_MMAP;
        b.index = i;
        if (xioctl(fd, VIDIOC_QUERYBUF, &b) < 0) {
            fprintf(stderr, "mmap_bufs: query type=%u idx=%d errno=%d\n", type, i, errno);
            return -1;
        }
        bufs[i].len = b.length;
        bufs[i].addr = mmap(NULL, b.length, PROT_READ | PROT_WRITE,
                            MAP_SHARED, fd, b.m.offset);
        if (bufs[i].addr == MAP_FAILED) {
            fprintf(stderr, "mmap_bufs: mmap fd=%d type=%u idx=%d len=%zu off=%u errno=%d\n",
                    fd, type, i, (size_t)b.length, (unsigned)b.m.offset, errno);
            bufs[i].addr = NULL;
            return -1;
        }
    }
    return 0;
}
