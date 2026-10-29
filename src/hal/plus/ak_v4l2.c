#if defined(__arm__) && !defined(__ARM_PCS_VFP)

#include "ak_v4l2.h"

#ifndef V4L2_PIX_FMT_H264
#define V4L2_PIX_FMT_H264 v4l2_fourcc('H', '2', '6', '4')
#endif
#ifndef V4L2_CID_MPEG_VIDEO_FORCE_KEY_FRAME
#define V4L2_CID_MPEG_VIDEO_FORCE_KEY_FRAME 0x00980912
#endif
#ifndef V4L2_CID_JPEG_COMPRESSION_QUALITY
#define V4L2_CID_JPEG_COMPRESSION_QUALITY 0x009d0003
#endif

pthread_mutex_t ak_v4l2_mtx = PTHREAD_MUTEX_INITIALIZER;
static unsigned int capGen[2];

unsigned int ak_v4l2_cap_gen(int index)
{
    return capGen[index & 1];
}

int ak_v4l2_cap_open(int index, int width, int height)
{
    struct v4l2_cap *c;
    struct v4l2_format f;
    struct v4l2_requestbuffers rb;
    char path[32];
    int type = V4L2_BUF_TYPE_VIDEO_CAPTURE;

    if (index < 0 || index > 1)
        return -1;
    c = &caps[index];
    if (c->fd >= 0)
        return 0;

    snprintf(path, sizeof(path), "/dev/video%d", index + 1);
    if ((c->fd = open(path, O_RDWR | O_NONBLOCK)) < 0)
        return -1;

    memset(&f, 0, sizeof(f));
    f.type = type;
    f.fmt.pix.width = width;
    f.fmt.pix.height = height;
    f.fmt.pix.pixelformat = V4L2_PIX_FMT_YUV420;
    f.fmt.pix.field = V4L2_FIELD_NONE;
    if (xioctl(c->fd, VIDIOC_S_FMT, &f) < 0)
        goto fail;

    memset(&rb, 0, sizeof(rb));
    rb.count = CAP_BUFS;
    rb.type = type;
    rb.memory = V4L2_MEMORY_MMAP;
    if (xioctl(c->fd, VIDIOC_REQBUFS, &rb) < 0)
        goto fail;
    if (mmap_bufs(c->fd, type, c->buf, CAP_BUFS) < 0)
        goto fail;

    for (int i = 0; i < CAP_BUFS; i++) {
        struct v4l2_buffer b;
        memset(&b, 0, sizeof(b));
        b.type = type;
        b.memory = V4L2_MEMORY_MMAP;
        b.index = i;
        if (xioctl(c->fd, VIDIOC_QBUF, &b) < 0)
            goto fail;
    }

    c->width = f.fmt.pix.width;
    c->height = f.fmt.pix.height;
    capGen[index & 1]++;
    return 0;

fail:
    munmap_bufs(c->buf, CAP_BUFS);
    close(c->fd);
    c->fd = -1;
    return -1;
}

int ak_v4l2_cap_streamon(int index)
{
    struct v4l2_cap *c = &caps[index];
    int type = V4L2_BUF_TYPE_VIDEO_CAPTURE;

    if (c->fd < 0 || c->streaming)
        return -1;
    if (xioctl(c->fd, VIDIOC_STREAMON, &type) < 0)
        return -1;
    c->streaming = 1;
    return 0;
}

void ak_v4l2_cap_close(int index)
{
    struct v4l2_cap *c = &caps[index];
    int type = V4L2_BUF_TYPE_VIDEO_CAPTURE;

    if (c->fd < 0)
        return;
    if (c->streaming)
        xioctl(c->fd, VIDIOC_STREAMOFF, &type);
    pthread_mutex_lock(&ak_v4l2_mtx);
    munmap_bufs(c->buf, CAP_BUFS);
    close(c->fd);
    c->fd = -1;
    c->streaming = 0;
    capGen[index & 1]++;
    pthread_mutex_unlock(&ak_v4l2_mtx);
}

int ak_v4l2_cap_dequeue(int index, void **data, size_t *len, uint64_t *ts, int *bufIndex, int timeout_ms)
{
    struct v4l2_cap *c = &caps[index];
    struct pollfd p;
    struct v4l2_buffer b;

    if (c->fd < 0)
        return -1;
    p.fd = c->fd;
    p.events = POLLIN;
    p.revents = 0;
    if (poll(&p, 1, timeout_ms) <= 0)
        return -1;
    memset(&b, 0, sizeof(b));
    b.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    b.memory = V4L2_MEMORY_MMAP;
    if (xioctl(c->fd, VIDIOC_DQBUF, &b) < 0)
        return -1;
    *data = c->buf[b.index].addr;
    *len = b.bytesused ? b.bytesused : b.length;
    if (bufIndex)
        *bufIndex = b.index;
    if (ts)
        *ts = (uint64_t)b.timestamp.tv_sec * 1000000ULL + b.timestamp.tv_usec;
    return 0;
}

int ak_v4l2_cap_requeue(int index, int bufIndex)
{
    struct v4l2_cap *c = &caps[index];
    struct v4l2_buffer b;

    if (c->fd < 0 || bufIndex < 0)
        return -1;
    memset(&b, 0, sizeof(b));
    b.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    b.memory = V4L2_MEMORY_MMAP;
    b.index = bufIndex;
    return xioctl(c->fd, VIDIOC_QBUF, &b);
}

int ak_v4l2_enc_open(int index, int width, int height, int codec,
                     int fps, int bitrate, int gop)
{
    struct v4l2_enc *e;
    struct v4l2_format f;
    struct v4l2_requestbuffers rb;
    struct v4l2_control c;
    uint32_t pix;

    if (index < 0 || index > 1)
        return -1;
    e = &encs[index];
    if (e->fd >= 0)
        return 0;

    switch (codec) {
    case 1: /* HAL_VIDCODEC_H264 */
        pix = V4L2_PIX_FMT_H264;
        break;
    case 2: /* HAL_VIDCODEC_H265 */
#ifdef V4L2_PIX_FMT_HEVC
        pix = V4L2_PIX_FMT_HEVC;
#else
        pix = V4L2_PIX_FMT_H264;
#endif
        break;
    default: /* MJPG / JPG */
        pix = V4L2_PIX_FMT_JPEG;
        break;
    }

    if ((e->fd = open("/dev/video0", O_RDWR | O_NONBLOCK)) < 0)
        return -1;

    memset(&f, 0, sizeof(f));
    f.type = V4L2_BUF_TYPE_VIDEO_OUTPUT;
    f.fmt.pix.width = width;
    f.fmt.pix.height = height;
    f.fmt.pix.pixelformat = V4L2_PIX_FMT_YUV420;
    f.fmt.pix.field = V4L2_FIELD_NONE;
    if (xioctl(e->fd, VIDIOC_S_FMT, &f) < 0) goto fail;

    memset(&f, 0, sizeof(f));
    f.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    f.fmt.pix.width = width;
    f.fmt.pix.height = height;
    f.fmt.pix.pixelformat = pix;
    f.fmt.pix.field = V4L2_FIELD_NONE;
    if (xioctl(e->fd, VIDIOC_S_FMT, &f) < 0) goto fail;

    memset(&rb, 0, sizeof(rb));
    rb.count = ENC_BUFS;
    rb.type = V4L2_BUF_TYPE_VIDEO_OUTPUT;
    rb.memory = V4L2_MEMORY_MMAP;
    if (xioctl(e->fd, VIDIOC_REQBUFS, &rb) < 0) goto fail;
    e->outCnt = rb.count > ENC_BUFS ? ENC_BUFS : (rb.count > 0 ? rb.count : 1);
    if (mmap_bufs(e->fd, V4L2_BUF_TYPE_VIDEO_OUTPUT, e->out, e->outCnt) < 0) goto fail;

    memset(&rb, 0, sizeof(rb));
    rb.count = pix == V4L2_PIX_FMT_JPEG ? 1 : ENC_BUFS;
    rb.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    rb.memory = V4L2_MEMORY_MMAP;
    if (xioctl(e->fd, VIDIOC_REQBUFS, &rb) < 0) goto fail;
    e->capCnt = rb.count > ENC_BUFS ? ENC_BUFS : (rb.count > 0 ? rb.count : 1);
    if (mmap_bufs(e->fd, V4L2_BUF_TYPE_VIDEO_CAPTURE, e->cap, e->capCnt) < 0) goto fail;

    e->width = width;
    e->height = height;
    e->outCur = 0;
    e->capCur = -1;
    return 0;

fail:
    munmap_bufs(e->out, e->outCnt);
    munmap_bufs(e->cap, e->capCnt);
    close(e->fd);
    e->fd = -1;
    return -1;
}

int ak_v4l2_enc_streamon(int index)
{
    struct v4l2_enc *e = &encs[index];
    int out = V4L2_BUF_TYPE_VIDEO_OUTPUT;
    int cap = V4L2_BUF_TYPE_VIDEO_CAPTURE;

    if (e->fd < 0 || e->streaming)
        return -1;

    for (int i = 0; i < e->capCnt; i++) {
        struct v4l2_buffer b;
        memset(&b, 0, sizeof(b));
        b.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        b.memory = V4L2_MEMORY_MMAP;
        b.index = i;
        if (xioctl(e->fd, VIDIOC_QBUF, &b) < 0)
            return -1;
    }

    if (xioctl(e->fd, VIDIOC_STREAMON, &out) < 0)
        return -1;
    if (xioctl(e->fd, VIDIOC_STREAMON, &cap) < 0) {
        xioctl(e->fd, VIDIOC_STREAMOFF, &out);
        return -1;
    }
    e->streaming = 1;
    return 0;
}

int ak_v4l2_enc_streamoff(int index)
{
    struct v4l2_enc *e = &encs[index];
    int out = V4L2_BUF_TYPE_VIDEO_OUTPUT;
    int cap = V4L2_BUF_TYPE_VIDEO_CAPTURE;

    if (e->fd < 0 || !e->streaming)
        return 0;
    xioctl(e->fd, VIDIOC_STREAMOFF, &cap);
    xioctl(e->fd, VIDIOC_STREAMOFF, &out);
    e->streaming = 0;
    return 0;
}

void ak_v4l2_enc_close(int index)
{
    struct v4l2_enc *e = &encs[index];

    if (e->fd < 0)
        return;
    ak_v4l2_enc_streamoff(index);
    munmap_bufs(e->out, e->outCnt);
    munmap_bufs(e->cap, e->capCnt);
    close(e->fd);
    e->fd = -1;
    e->outCur = 0;
    e->capCur = -1;
}

int ak_v4l2_enc_push(int index, const void *data, size_t len,
                     void **out, size_t *outlen, int timeout_ms)
{
    struct v4l2_enc *e = &encs[index];
    struct v4l2_buffer b;
    struct pollfd p;
    int slot;

    if (e->fd < 0 || !e->streaming)
        return -1;

    slot = e->outCur;
    if (len > e->out[slot].len)
        len = e->out[slot].len;
    memcpy(e->out[slot].addr, data, len);

    memset(&b, 0, sizeof(b));
    b.type = V4L2_BUF_TYPE_VIDEO_OUTPUT;
    b.memory = V4L2_MEMORY_MMAP;
    b.index = slot;
    b.bytesused = len;
    if (xioctl(e->fd, VIDIOC_QBUF, &b) < 0)
        return -1;
    e->outCur = (slot + 1) % e->outCnt;

    p.fd = e->fd;
    p.events = POLLIN;
    p.revents = 0;
    if (poll(&p, 1, timeout_ms) <= 0)
        return -1;

    memset(&b, 0, sizeof(b));
    b.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    b.memory = V4L2_MEMORY_MMAP;
    if (xioctl(e->fd, VIDIOC_DQBUF, &b) < 0)
        return -1;
    e->capCur = b.index;
    *out = e->cap[b.index].addr;
    *outlen = b.bytesused;

    memset(&b, 0, sizeof(b));
    b.type = V4L2_BUF_TYPE_VIDEO_OUTPUT;
    b.memory = V4L2_MEMORY_MMAP;
    if (xioctl(e->fd, VIDIOC_DQBUF, &b) < 0)
        return -1;

    return 0;
}

int ak_v4l2_enc_requeue(int index)
{
    struct v4l2_enc *e = &encs[index];
    struct v4l2_buffer b;

    if (e->fd < 0 || e->capCur < 0)
        return -1;
    memset(&b, 0, sizeof(b));
    b.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    b.memory = V4L2_MEMORY_MMAP;
    b.index = e->capCur;
    e->capCur = -1;
    return xioctl(e->fd, VIDIOC_QBUF, &b);
}

int ak_v4l2_enc_request_idr(int index)
{
    struct v4l2_enc *e = &encs[index];
    struct v4l2_control c;

    if (e->fd < 0)
        return -1;
    memset(&c, 0, sizeof(c));
    c.id = V4L2_CID_MPEG_VIDEO_FORCE_KEY_FRAME;
    c.value = 1;
    return xioctl(e->fd, VIDIOC_S_CTRL, &c);
}

int ak_v4l2_enc_grayscale(int index, int on)
{
    struct v4l2_enc *e = &encs[index];
    struct v4l2_control c;

    if (e->fd < 0)
        return -1;
    memset(&c, 0, sizeof(c));
    c.id = V4L2_CID_SATURATION;
    c.value = on ? 0 : 128;
    return xioctl(e->fd, VIDIOC_S_CTRL, &c);
}

int ak_v4l2_snapshot(int width, int height, int quality,
                     void **data, size_t *len)
{
    struct v4l2_enc e;
    struct v4l2_buf out[ENC_BUFS], cap[ENC_BUFS];
    int outCnt = 0, capCnt = 0;
    struct v4l2_format f;
    struct v4l2_requestbuffers rb;
    struct v4l2_control c;
    struct v4l2_buffer b;
    struct pollfd p;
    int outType = V4L2_BUF_TYPE_VIDEO_OUTPUT;
    int capType = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    int ret = -1;

    memset(&e, 0, sizeof(e));
    e.fd = -1;
    memset(out, 0, sizeof(out));
    memset(cap, 0, sizeof(cap));
    *data = NULL;
    *len = 0;

    if ((e.fd = open("/dev/video0", O_RDWR | O_NONBLOCK)) < 0)
        return -1;

    memset(&f, 0, sizeof(f));
    f.type = outType;
    f.fmt.pix.width = width;
    f.fmt.pix.height = height;
    f.fmt.pix.pixelformat = V4L2_PIX_FMT_YUV420;
    f.fmt.pix.field = V4L2_FIELD_NONE;
    if (xioctl(e.fd, VIDIOC_S_FMT, &f) < 0) {
        goto done;
    }

    memset(&f, 0, sizeof(f));
    f.type = capType;
    f.fmt.pix.width = width;
    f.fmt.pix.height = height;
    f.fmt.pix.pixelformat = V4L2_PIX_FMT_MJPEG;
    f.fmt.pix.field = V4L2_FIELD_NONE;
    if (xioctl(e.fd, VIDIOC_S_FMT, &f) < 0) {
        goto done;
    }

    memset(&c, 0, sizeof(c));
    c.id = V4L2_CID_JPEG_COMPRESSION_QUALITY;
    c.value = quality;
    xioctl(e.fd, VIDIOC_S_CTRL, &c);

    memset(&rb, 0, sizeof(rb));
    rb.count = 1;
    rb.type = outType;
    rb.memory = V4L2_MEMORY_MMAP;
    if (xioctl(e.fd, VIDIOC_REQBUFS, &rb) < 0) {
        goto done;
    }
    outCnt = rb.count > ENC_BUFS ? ENC_BUFS : (rb.count > 0 ? rb.count : 1);
    if (mmap_bufs(e.fd, outType, out, outCnt) < 0)
        goto done;

    memset(&rb, 0, sizeof(rb));
    rb.count = 1;
    rb.type = capType;
    rb.memory = V4L2_MEMORY_MMAP;
    if (xioctl(e.fd, VIDIOC_REQBUFS, &rb) < 0) {
        goto done;
    }
    capCnt = rb.count > ENC_BUFS ? ENC_BUFS : (rb.count > 0 ? rb.count : 1);
    if (mmap_bufs(e.fd, capType, cap, capCnt) < 0)
        goto done;

    {
        void *frame = NULL;
        size_t flen = 0;
        int snapIdx = -1;
        if (ak_v4l2_cap_dequeue(0, &frame, &flen, NULL, &snapIdx, 2000) < 0) {
            goto done;
        }
        if (flen > out[0].len)
            flen = out[0].len;
        memcpy(out[0].addr, frame, flen);
        ak_v4l2_cap_requeue(0, snapIdx);
        memset(&b, 0, sizeof(b));
        b.type = outType;
        b.memory = V4L2_MEMORY_MMAP;
        b.index = 0;
        b.bytesused = flen;
        if (xioctl(e.fd, VIDIOC_QBUF, &b) < 0) {
            goto done;
        }
    }

    for (int i = 0; i < capCnt; i++) {
        memset(&b, 0, sizeof(b));
        b.type = capType;
        b.memory = V4L2_MEMORY_MMAP;
        b.index = i;
        if (xioctl(e.fd, VIDIOC_QBUF, &b) < 0) {
            goto done;
        }
    }

    if (xioctl(e.fd, VIDIOC_STREAMON, &outType) < 0) {
        goto done;
    }
    if (xioctl(e.fd, VIDIOC_STREAMON, &capType) < 0) {
        goto done;
    }

    p.fd = e.fd;
    p.events = POLLIN;
    p.revents = 0;
    if (poll(&p, 1, 2000) <= 0) {
        goto done;
    }

    memset(&b, 0, sizeof(b));
    b.type = capType;
    b.memory = V4L2_MEMORY_MMAP;
    if (xioctl(e.fd, VIDIOC_DQBUF, &b) < 0) {
        goto done;
    }

    *data = malloc(b.bytesused);
    if (*data) {
        memcpy(*data, cap[b.index].addr, b.bytesused);
        *len = b.bytesused;
        ret = 0;
    }
    xioctl(e.fd, VIDIOC_QBUF, &b);

done:
    munmap_bufs(out, outCnt);
    munmap_bufs(cap, capCnt);
    close(e.fd);
    return ret;
}

#endif
