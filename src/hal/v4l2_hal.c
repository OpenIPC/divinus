#include "v4l2_hal.h"

int (*v4l2_aud_cb)(hal_audframe*);
int (*v4l2_vid_cb)(char, hal_vidstream*);

#ifdef DIVINUS_V4L2

#include "support.h"
#include "../app_config.h"

#include <alsa/asoundlib.h>
#include <errno.h>
#include <fcntl.h>
#include <linux/videodev2.h>
#include <poll.h>
#include <pthread.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <time.h>
#include <unistd.h>

#define V4L2_CHN_NUM 4
#define V4L2_BUF_NUM 4
#define V4L2_DEV_NUM 64
#define V4L2_WAIT_MS 2000

// Toolchains with headers older than Linux 4.10 lack it
#ifndef V4L2_PIX_FMT_HEVC
#define V4L2_PIX_FMT_HEVC v4l2_fourcc('H', 'E', 'V', 'C')
#endif

typedef struct {
    void *data;
    size_t length;
    int dmabuf;
} v4l2_map;

typedef struct {
    int fd;
    hal_vidconfig config;
    unsigned int fourcc, quality;
    char dmabuf, failed;
    unsigned int inCount, inStride, inSize;
    v4l2_map in[V4L2_BUF_NUM];
    unsigned int outCount;
    v4l2_map out[V4L2_BUF_NUM];
    unsigned long long nextDue;
} v4l2_enc;

static hal_chnstate _v4l2_state[V4L2_CHN_NUM];
static v4l2_enc _v4l2_enc[V4L2_CHN_NUM];
static char _v4l2_idr[V4L2_CHN_NUM];

static char _v4l2_capPath[32], _v4l2_h264Path[32], _v4l2_h265Path[32],
    _v4l2_jpegPath[32], _v4l2_mjpegPath[32];
static int _v4l2_cap = -1;
static unsigned int _v4l2_width, _v4l2_height, _v4l2_stride, _v4l2_size;
static unsigned int _v4l2_capCount;
static v4l2_map _v4l2_capMap[V4L2_BUF_NUM];

static pthread_mutex_t _v4l2_pipeLock = PTHREAD_MUTEX_INITIALIZER;
static pthread_mutex_t _v4l2_snapLock = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t _v4l2_snapCond = PTHREAD_COND_INITIALIZER;
static unsigned int _v4l2_snapReq, _v4l2_snapDone, _v4l2_snapQuality;
static void *_v4l2_snapData;
static size_t _v4l2_snapSize;

static snd_pcm_t *_v4l2_pcm;

static int v4l2_ioctl(int fd, unsigned long request, void *arg)
{
    int ret;
    do ret = ioctl(fd, request, arg); while (ret < 0 && errno == EINTR);
    return ret;
}

static int v4l2_control(int fd, unsigned int id, int value)
{
    struct v4l2_control ctrl = { .id = id, .value = value };
    return v4l2_ioctl(fd, VIDIOC_S_CTRL, &ctrl);
}

static unsigned int v4l2_caps(int fd, struct v4l2_capability *cap)
{
    memset(cap, 0, sizeof(*cap));
    if (v4l2_ioctl(fd, VIDIOC_QUERYCAP, cap)) return 0;
    return cap->capabilities & V4L2_CAP_DEVICE_CAPS ?
        cap->device_caps : cap->capabilities;
}

static char v4l2_has_format(int fd, unsigned int type, unsigned int fourcc)
{
    struct v4l2_fmtdesc desc = { .type = type };
    for (; !v4l2_ioctl(fd, VIDIOC_ENUM_FMT, &desc); desc.index++)
        if (desc.pixelformat == fourcc) return 1;
    return 0;
}

static void v4l2_unmap(v4l2_map *map, unsigned int count)
{
    for (unsigned int i = 0; i < count; i++) {
        if (map[i].data && map[i].data != MAP_FAILED)
            munmap(map[i].data, map[i].length);
        if (map[i].dmabuf >= 0) close(map[i].dmabuf);
        map[i].data = NULL;
        map[i].dmabuf = -1;
    }
}

static int v4l2_map_buffers(int fd, unsigned int type, v4l2_map *map,
    unsigned int *count, char export, char queue)
{
    struct v4l2_requestbuffers req = { .count = *count, .type = type,
        .memory = V4L2_MEMORY_MMAP };
    *count = 0;
    for (unsigned int i = 0; i < V4L2_BUF_NUM; i++) {
        map[i].data = NULL;
        map[i].dmabuf = -1;
    }
    /* Drivers may raise the count to the minimum they need */
    if (v4l2_ioctl(fd, VIDIOC_REQBUFS, &req) || !req.count) return -1;
    if (req.count > V4L2_BUF_NUM) return -1;

    for (unsigned int i = 0; i < req.count; i++) {
        struct v4l2_buffer buf = { .index = i, .type = type,
            .memory = V4L2_MEMORY_MMAP };
        if (v4l2_ioctl(fd, VIDIOC_QUERYBUF, &buf)) return -1;
        map[i].length = buf.length;
        map[i].data = mmap(NULL, buf.length, PROT_READ | PROT_WRITE,
            MAP_SHARED, fd, buf.m.offset);
        *count = i + 1;
        if (map[i].data == MAP_FAILED) return -1;
        if (export) {
            struct v4l2_exportbuffer exp = { .type = type, .index = i,
                .flags = O_CLOEXEC | O_RDONLY };
            if (!v4l2_ioctl(fd, VIDIOC_EXPBUF, &exp))
                map[i].dmabuf = exp.fd;
        }
        if (queue && v4l2_ioctl(fd, VIDIOC_QBUF, &buf)) return -1;
    }

    return 0;
}

static void v4l2_release_buffers(int fd, unsigned int type, unsigned int memory)
{
    struct v4l2_requestbuffers req = { .type = type, .memory = memory };
    v4l2_ioctl(fd, VIDIOC_REQBUFS, &req);
}

int v4l2_system_identify(void)
{
    const char *override = getenv("DIVINUS_V4L2_CAPTURE");
    char card[32] = {0};

    for (unsigned int i = 0; i < V4L2_CHN_NUM; i++) _v4l2_enc[i].fd = -1;

    for (unsigned int n = 0; n < V4L2_DEV_NUM; n++) {
        char path[32];
        struct v4l2_capability cap;
        snprintf(path, sizeof(path), "/dev/video%u", n);
        int fd = open(path, O_RDWR | O_NONBLOCK | O_CLOEXEC);
        if (fd < 0) continue;

        unsigned int caps = v4l2_caps(fd, &cap);
        char m2m = (caps & (V4L2_CAP_VIDEO_M2M)) ||
            ((caps & V4L2_CAP_VIDEO_CAPTURE) && (caps & V4L2_CAP_VIDEO_OUTPUT));

        if (!(caps & V4L2_CAP_STREAMING)) {
        } else if (m2m) {
            if (v4l2_has_format(fd, V4L2_BUF_TYPE_VIDEO_OUTPUT, V4L2_PIX_FMT_NV12)) {
                const struct { unsigned int fourcc; char *path; } coded[] = {
                    { V4L2_PIX_FMT_H264, _v4l2_h264Path },
                    { V4L2_PIX_FMT_HEVC, _v4l2_h265Path },
                    { V4L2_PIX_FMT_JPEG, _v4l2_jpegPath },
                    { V4L2_PIX_FMT_MJPEG, _v4l2_mjpegPath } };
                for (unsigned int c = 0; c < sizeof(coded) / sizeof(*coded); c++)
                    if (!coded[c].path[0] && v4l2_has_format(fd,
                        V4L2_BUF_TYPE_VIDEO_CAPTURE, coded[c].fourcc))
                        strcpy(coded[c].path, path);
            }
        } else if ((caps & V4L2_CAP_VIDEO_CAPTURE) && !_v4l2_capPath[0] &&
            (override ? !strcmp(override, path) : 1) &&
            v4l2_has_format(fd, V4L2_BUF_TYPE_VIDEO_CAPTURE, V4L2_PIX_FMT_NV12)) {
            strcpy(_v4l2_capPath, path);
            snprintf(card, sizeof(card), "%s", (char*)cap.card);
        }
        close(fd);
    }

    if (!_v4l2_capPath[0] || (!_v4l2_h264Path[0] && !_v4l2_h265Path[0] &&
        !_v4l2_jpegPath[0] && !_v4l2_mjpegPath[0]))
        return 0;

    snprintf(chip, sizeof(chip), "%s", card[0] ? card : "V4L2");
    strcpy(family, "V4L2");
    plat = HAL_PLATFORM_V4L2;
    chnCount = V4L2_CHN_NUM;
    chnState = _v4l2_state;
    aud_thread = v4l2_audio_thread;
    isp_thread = NULL;
    vid_thread = v4l2_video_thread;
    return 1;
}

int v4l2_hal_init(void)
{
    HAL_INFO("v4l2", "Capture %s, H.264 %s, H.265 %s, JPEG %s, MJPEG %s\n",
        _v4l2_capPath, _v4l2_h264Path[0] ? _v4l2_h264Path : "-",
        _v4l2_h265Path[0] ? _v4l2_h265Path : "-",
        _v4l2_jpegPath[0] ? _v4l2_jpegPath : "-",
        _v4l2_mjpegPath[0] ? _v4l2_mjpegPath : "-");
    return EXIT_SUCCESS;
}

void v4l2_hal_deinit(void) {}

int v4l2_audio_init(int samplerate)
{
    const char *device = getenv("DIVINUS_ALSA_DEVICE");
    int ret;

    if (!device) device = "default";
    if ((ret = snd_pcm_open(&_v4l2_pcm, device, SND_PCM_STREAM_CAPTURE,
        SND_PCM_NONBLOCK)) < 0) {
        HAL_DANGER("v4l2_aud", "Opening %s failed: %s\n", device, snd_strerror(ret));
        _v4l2_pcm = NULL;
        return ret;
    }
    /* The default device converts format, rate and channel count as needed. */
    if ((ret = snd_pcm_set_params(_v4l2_pcm, SND_PCM_FORMAT_S16_LE,
        SND_PCM_ACCESS_RW_INTERLEAVED, 1, samplerate, 1, 100000)) < 0) {
        HAL_DANGER("v4l2_aud", "Mono S16 %d Hz on %s failed: %s\n",
            samplerate, device, snd_strerror(ret));
        snd_pcm_close(_v4l2_pcm);
        _v4l2_pcm = NULL;
        return ret;
    }

    return EXIT_SUCCESS;
}

void v4l2_audio_deinit(void)
{
    if (_v4l2_pcm) snd_pcm_close(_v4l2_pcm);
    _v4l2_pcm = NULL;
}

void *v4l2_audio_thread(void)
{
    static short samples[1024];
    unsigned int sequence = 0;

    while (keepRunning && audioOn && _v4l2_pcm) {
        /* A prepared capture stream never becomes readable on its own. */
        if (snd_pcm_state(_v4l2_pcm) == SND_PCM_STATE_PREPARED)
            snd_pcm_start(_v4l2_pcm);
        int ret = snd_pcm_wait(_v4l2_pcm, 200);
        if (!ret) continue;

        snd_pcm_sframes_t count = ret < 0 ? ret :
            snd_pcm_readi(_v4l2_pcm, samples, sizeof(samples) / sizeof(*samples));
        if (count == -EAGAIN || !count) continue;
        if (count < 0) {
            if (snd_pcm_recover(_v4l2_pcm, count, 1) < 0) {
                HAL_WARNING("v4l2_aud", "Capture failed: %s\n", snd_strerror(count));
                usleep(100000);
                snd_pcm_prepare(_v4l2_pcm);
            }
            continue;
        }

        if (v4l2_aud_cb) {
            hal_audframe frame = { .channelCnt = 1, .interleavedOn = 1,
                .data = { (unsigned char*)samples },
                .length = { count * sizeof(*samples) },
                .seq = sequence++, .timestamp = millis() };
            (*v4l2_aud_cb)(&frame);
        }
    }

    return NULL;
}

int v4l2_channel_bind(char index) { return EXIT_SUCCESS; }

int v4l2_channel_create(char index, short width, short height, char jpeg)
{
    if (index < 0 || index >= V4L2_CHN_NUM || _v4l2_cap < 0) return EXIT_FAILURE;
    /* Every encoder reads the shared capture frame; there is no scaler. */
    if ((unsigned int)width != _v4l2_width || (unsigned int)height != _v4l2_height)
        HAL_WARNING("v4l2", "Channel %d encodes %ux%u, not %dx%d\n",
            index, _v4l2_width, _v4l2_height, width, height);
    return EXIT_SUCCESS;
}

void v4l2_channel_grayscale(char enable)
{
    if (_v4l2_cap < 0) return;
    if (!v4l2_control(_v4l2_cap, V4L2_CID_COLORFX,
        enable ? V4L2_COLORFX_BW : V4L2_COLORFX_NONE)) return;

    struct v4l2_queryctrl query = { .id = V4L2_CID_SATURATION };
    if (v4l2_ioctl(_v4l2_cap, VIDIOC_QUERYCTRL, &query) ||
        query.flags & V4L2_CTRL_FLAG_DISABLED) return;
    v4l2_control(_v4l2_cap, V4L2_CID_SATURATION,
        enable ? query.minimum : query.default_value);
}

int v4l2_channel_unbind(char index) { return EXIT_SUCCESS; }

static void v4l2_pipeline_close(void)
{
    if (_v4l2_cap < 0) return;
    unsigned int type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    v4l2_ioctl(_v4l2_cap, VIDIOC_STREAMOFF, &type);
    v4l2_unmap(_v4l2_capMap, _v4l2_capCount);
    v4l2_release_buffers(_v4l2_cap, type, V4L2_MEMORY_MMAP);
    close(_v4l2_cap);
    _v4l2_cap = -1;
    _v4l2_capCount = 0;
}

static void v4l2_pipeline_interval(char framerate)
{
    struct v4l2_streamparm parm = { .type = V4L2_BUF_TYPE_VIDEO_CAPTURE };
    struct v4l2_fract best = {0}, fastest = {0};
    struct v4l2_frmivalenum ival = { .pixel_format = V4L2_PIX_FMT_NV12,
        .width = _v4l2_width, .height = _v4l2_height };

    if (framerate <= 0) return;

    /* The configured rate is a cap: pick the slowest sensor mode that can
     * still supply it, and decimate in the encoder loop. */
    for (; !v4l2_ioctl(_v4l2_cap, VIDIOC_ENUM_FRAMEINTERVALS, &ival); ival.index++) {
        if (ival.type != V4L2_FRMIVAL_TYPE_DISCRETE) break;
        struct v4l2_fract f = ival.discrete;
        if (!f.numerator || !f.denominator) continue;
        if (!fastest.denominator || (unsigned long long)f.numerator * fastest.denominator <
            (unsigned long long)fastest.numerator * f.denominator) fastest = f;
        if ((unsigned long long)f.numerator * framerate <= f.denominator &&
            (!best.denominator || (unsigned long long)f.numerator * best.denominator >
            (unsigned long long)best.numerator * f.denominator)) best = f;
    }
    if (!best.denominator) best = fastest;
    if (!best.denominator) best = (struct v4l2_fract){ 1, framerate };

    parm.parm.capture.timeperframe = best;
    if (v4l2_ioctl(_v4l2_cap, VIDIOC_S_PARM, &parm))
        HAL_WARNING("v4l2", "Setting the capture interval failed: %s\n", strerror(errno));
}

int v4l2_pipeline_create(short width, short height, char mirror, char flip,
    char framerate, int antiflicker)
{
    struct v4l2_format fmt = { .type = V4L2_BUF_TYPE_VIDEO_CAPTURE };
    unsigned int type = fmt.type;

    if ((_v4l2_cap = open(_v4l2_capPath, O_RDWR | O_NONBLOCK | O_CLOEXEC)) < 0) {
        HAL_DANGER("v4l2", "Opening %s failed: %s\n", _v4l2_capPath, strerror(errno));
        return EXIT_FAILURE;
    }

    fmt.fmt.pix.width = width;
    fmt.fmt.pix.height = height;
    fmt.fmt.pix.pixelformat = V4L2_PIX_FMT_NV12;
    fmt.fmt.pix.field = V4L2_FIELD_NONE;
    if (v4l2_ioctl(_v4l2_cap, VIDIOC_S_FMT, &fmt) ||
        fmt.fmt.pix.pixelformat != V4L2_PIX_FMT_NV12) {
        HAL_DANGER("v4l2", "NV12 %dx%d capture is not supported!\n", width, height);
        goto failed;
    }
    _v4l2_width = fmt.fmt.pix.width;
    _v4l2_height = fmt.fmt.pix.height;
    _v4l2_stride = fmt.fmt.pix.bytesperline ? fmt.fmt.pix.bytesperline : _v4l2_width;
    _v4l2_size = fmt.fmt.pix.sizeimage;
    if (_v4l2_size < _v4l2_stride * _v4l2_height * 3 / 2) goto failed;
    if (_v4l2_width != (unsigned int)width || _v4l2_height != (unsigned int)height)
        HAL_WARNING("v4l2", "Capture adjusted to %ux%u\n", _v4l2_width, _v4l2_height);

    v4l2_pipeline_interval(framerate);

    /* Image controls are optional; only report the ones requested. */
    if (v4l2_control(_v4l2_cap, V4L2_CID_HFLIP, mirror ? 1 : 0) && mirror)
        HAL_WARNING("v4l2", "Mirroring is not supported\n");
    if (v4l2_control(_v4l2_cap, V4L2_CID_VFLIP, flip ? 1 : 0) && flip)
        HAL_WARNING("v4l2", "Flipping is not supported\n");
    if (v4l2_control(_v4l2_cap, V4L2_CID_POWER_LINE_FREQUENCY,
        antiflicker == 50 ? V4L2_CID_POWER_LINE_FREQUENCY_50HZ :
        antiflicker == 60 ? V4L2_CID_POWER_LINE_FREQUENCY_60HZ :
        V4L2_CID_POWER_LINE_FREQUENCY_DISABLED) && antiflicker)
        HAL_WARNING("v4l2", "Antiflicker is not supported\n");

    _v4l2_capCount = V4L2_BUF_NUM;
    if (v4l2_map_buffers(_v4l2_cap, type, _v4l2_capMap, &_v4l2_capCount, 1, 1)) {
        HAL_DANGER("v4l2", "Allocating capture buffers failed: %s\n", strerror(errno));
        goto failed;
    }
    if (v4l2_ioctl(_v4l2_cap, VIDIOC_STREAMON, &type)) {
        HAL_DANGER("v4l2", "Starting the capture failed: %s\n", strerror(errno));
        goto failed;
    }

    HAL_INFO("v4l2", "Capturing NV12 %ux%u\n", _v4l2_width, _v4l2_height);
    return EXIT_SUCCESS;

failed:
    v4l2_pipeline_close();
    return EXIT_FAILURE;
}

void v4l2_pipeline_destroy(void)
{
    pthread_mutex_lock(&_v4l2_pipeLock);
    v4l2_pipeline_close();
    pthread_mutex_unlock(&_v4l2_pipeLock);
}

static void v4l2_enc_close(v4l2_enc *enc)
{
    if (enc->fd >= 0) {
        unsigned int type = V4L2_BUF_TYPE_VIDEO_OUTPUT;
        v4l2_ioctl(enc->fd, VIDIOC_STREAMOFF, &type);
        type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        v4l2_ioctl(enc->fd, VIDIOC_STREAMOFF, &type);
        if (!enc->dmabuf) v4l2_unmap(enc->in, enc->inCount);
        v4l2_unmap(enc->out, enc->outCount);
        close(enc->fd);
    }
    memset(enc, 0, sizeof(*enc));
    enc->fd = -1;
}

static void v4l2_enc_controls(v4l2_enc *enc, hal_vidconfig *config)
{
    if (config->codec == HAL_VIDCODEC_JPG || config->codec == HAL_VIDCODEC_MJPG) {
        if (v4l2_control(enc->fd, V4L2_CID_JPEG_COMPRESSION_QUALITY, enc->quality))
            HAL_WARNING("v4l2_venc", "JPEG quality is not adjustable\n");
        return;
    }

    if (config->mode == HAL_VIDMODE_CBR || config->mode == HAL_VIDMODE_VBR) {
        if (v4l2_control(enc->fd, V4L2_CID_MPEG_VIDEO_BITRATE_MODE,
            config->mode == HAL_VIDMODE_CBR ? V4L2_MPEG_VIDEO_BITRATE_MODE_CBR :
            V4L2_MPEG_VIDEO_BITRATE_MODE_VBR))
            HAL_WARNING("v4l2_venc", "Rate control mode is not adjustable\n");
    } else HAL_WARNING("v4l2_venc", "Rate control mode %d is unsupported\n", config->mode);
    if (v4l2_control(enc->fd, V4L2_CID_MPEG_VIDEO_BITRATE, config->bitrate * 1000))
        HAL_WARNING("v4l2_venc", "Bitrate is not adjustable\n");
    if (config->gop && v4l2_control(enc->fd, V4L2_CID_MPEG_VIDEO_GOP_SIZE, config->gop))
        HAL_WARNING("v4l2_venc", "GOP size is not adjustable\n");

    if (config->codec == HAL_VIDCODEC_H264) {
        int profile = config->profile == HAL_VIDPROFILE_BASELINE ?
            V4L2_MPEG_VIDEO_H264_PROFILE_BASELINE :
            config->profile == HAL_VIDPROFILE_MAIN ?
            V4L2_MPEG_VIDEO_H264_PROFILE_MAIN : V4L2_MPEG_VIDEO_H264_PROFILE_HIGH;
        struct v4l2_control ctrl = { .id = V4L2_CID_MPEG_VIDEO_H264_PROFILE };
        if (v4l2_control(enc->fd, ctrl.id, profile) &&
            !v4l2_ioctl(enc->fd, VIDIOC_G_CTRL, &ctrl) && ctrl.value != profile)
            HAL_WARNING("v4l2_venc", "H.264 profile is fixed at %d\n", ctrl.value);
    }
}

int v4l2_video_create(char index, hal_vidconfig *config)
{
    const char *path;
    unsigned int fourcc;

    if (index < 0 || index >= V4L2_CHN_NUM || _v4l2_cap < 0) return EXIT_FAILURE;

    switch (config->codec) {
        case HAL_VIDCODEC_H264: path = _v4l2_h264Path; fourcc = V4L2_PIX_FMT_H264; break;
        case HAL_VIDCODEC_H265: path = _v4l2_h265Path; fourcc = V4L2_PIX_FMT_HEVC; break;
        case HAL_VIDCODEC_MJPG:
            path = _v4l2_mjpegPath[0] ? _v4l2_mjpegPath : _v4l2_jpegPath;
            fourcc = _v4l2_mjpegPath[0] ? V4L2_PIX_FMT_MJPEG : V4L2_PIX_FMT_JPEG;
            break;
        case HAL_VIDCODEC_JPG:
            path = _v4l2_jpegPath[0] ? _v4l2_jpegPath : _v4l2_mjpegPath;
            fourcc = _v4l2_jpegPath[0] ? V4L2_PIX_FMT_JPEG : V4L2_PIX_FMT_MJPEG;
            break;
        default: return EXIT_FAILURE;
    }
    if (!path[0]) {
        HAL_DANGER("v4l2_venc", "No encoder for codec %d\n", config->codec);
        return EXIT_FAILURE;
    }

    pthread_mutex_lock(&_v4l2_pipeLock);
    v4l2_enc *enc = &_v4l2_enc[(unsigned char)index];
    v4l2_enc_close(enc);
    enc->config = *config;
    enc->fourcc = fourcc;
    enc->quality = config->codec == HAL_VIDCODEC_MJPG ? app_config.mjpeg_qfactor :
        config->maxQual;
    if (enc->quality < 1 || enc->quality > 100) enc->quality = 80;

    if ((enc->fd = open(path, O_RDWR | O_NONBLOCK | O_CLOEXEC)) < 0) goto failed;

    /* Stateful encoder order: coded format first, then the raw format. */
    struct v4l2_format fmt = { .type = V4L2_BUF_TYPE_VIDEO_CAPTURE };
    fmt.fmt.pix.width = _v4l2_width;
    fmt.fmt.pix.height = _v4l2_height;
    fmt.fmt.pix.pixelformat = fourcc;
    if (v4l2_ioctl(enc->fd, VIDIOC_S_FMT, &fmt) || fmt.fmt.pix.pixelformat != fourcc)
        goto failed;

    memset(&fmt, 0, sizeof(fmt));
    fmt.type = V4L2_BUF_TYPE_VIDEO_OUTPUT;
    fmt.fmt.pix.width = _v4l2_width;
    fmt.fmt.pix.height = _v4l2_height;
    fmt.fmt.pix.pixelformat = V4L2_PIX_FMT_NV12;
    fmt.fmt.pix.bytesperline = _v4l2_stride;
    fmt.fmt.pix.sizeimage = _v4l2_size;
    if (v4l2_ioctl(enc->fd, VIDIOC_S_FMT, &fmt) ||
        fmt.fmt.pix.pixelformat != V4L2_PIX_FMT_NV12 ||
        fmt.fmt.pix.width != _v4l2_width || fmt.fmt.pix.height != _v4l2_height)
        goto failed;
    enc->inStride = fmt.fmt.pix.bytesperline ? fmt.fmt.pix.bytesperline : _v4l2_width;
    enc->inSize = fmt.fmt.pix.sizeimage;
    if (enc->inSize < enc->inStride * _v4l2_height * 3 / 2) goto failed;

    if (config->codec != HAL_VIDCODEC_JPG && config->framerate) {
        struct v4l2_streamparm parm = { .type = V4L2_BUF_TYPE_VIDEO_OUTPUT };
        parm.parm.output.timeperframe = (struct v4l2_fract){ 1, config->framerate };
        if (v4l2_ioctl(enc->fd, VIDIOC_S_PARM, &parm))
            HAL_WARNING("v4l2_venc", "Encoder frame rate is not adjustable\n");
    }

    v4l2_enc_controls(enc, config);

    /* Import the capture buffers when layouts match, otherwise copy. */
    char exported = _v4l2_capCount > 0;
    for (unsigned int i = 0; i < _v4l2_capCount; i++)
        if (_v4l2_capMap[i].dmabuf < 0 || _v4l2_capMap[i].length < enc->inSize)
            exported = 0;
    if (exported && enc->inStride == _v4l2_stride) {
        struct v4l2_requestbuffers req = { .count = _v4l2_capCount,
            .type = V4L2_BUF_TYPE_VIDEO_OUTPUT, .memory = V4L2_MEMORY_DMABUF };
        if (!v4l2_ioctl(enc->fd, VIDIOC_REQBUFS, &req) && req.count) {
            enc->dmabuf = 1;
            enc->inCount = req.count;
        }
    }
    if (!enc->dmabuf) {
        enc->inCount = 1;
        if (v4l2_map_buffers(enc->fd, V4L2_BUF_TYPE_VIDEO_OUTPUT, enc->in,
            &enc->inCount, 0, 0)) goto failed;
    }

    enc->outCount = V4L2_BUF_NUM;
    if (v4l2_map_buffers(enc->fd, V4L2_BUF_TYPE_VIDEO_CAPTURE, enc->out,
        &enc->outCount, 0, 1)) goto failed;

    unsigned int type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    if (v4l2_ioctl(enc->fd, VIDIOC_STREAMON, &type)) goto failed;
    type = V4L2_BUF_TYPE_VIDEO_OUTPUT;
    if (v4l2_ioctl(enc->fd, VIDIOC_STREAMON, &type)) goto failed;

    _v4l2_state[(unsigned char)index].fileDesc = enc->fd;
    _v4l2_state[(unsigned char)index].payload = config->codec;
    pthread_mutex_unlock(&_v4l2_pipeLock);
    HAL_INFO("v4l2_venc", "Channel %d uses %s (%s input)\n", index, path,
        enc->dmabuf ? "shared" : "copied");
    return EXIT_SUCCESS;

failed:
    HAL_DANGER("v4l2_venc", "Creating encoder %d on %s failed: %s\n",
        index, path, strerror(errno));
    v4l2_enc_close(enc);
    pthread_mutex_unlock(&_v4l2_pipeLock);
    return EXIT_FAILURE;
}

int v4l2_video_destroy(char index)
{
    if (index < 0 || index >= V4L2_CHN_NUM) return EXIT_FAILURE;
    pthread_mutex_lock(&_v4l2_pipeLock);
    v4l2_enc_close(&_v4l2_enc[(unsigned char)index]);
    _v4l2_state[(unsigned char)index].enable = 0;
    _v4l2_state[(unsigned char)index].payload = HAL_VIDCODEC_UNSPEC;
    pthread_mutex_unlock(&_v4l2_pipeLock);
    return EXIT_SUCCESS;
}

int v4l2_video_destroy_all(void)
{
    for (char i = 0; i < V4L2_CHN_NUM; i++)
        if (_v4l2_state[(unsigned char)i].enable)
            v4l2_video_destroy(i);
    pthread_mutex_lock(&_v4l2_snapLock);
    free(_v4l2_snapData);
    _v4l2_snapData = NULL;
    _v4l2_snapSize = 0;
    pthread_mutex_unlock(&_v4l2_snapLock);
    return EXIT_SUCCESS;
}

void v4l2_video_request_idr(char index)
{
    /* Called from the RTSP thread: never wait for the encoder loop here. */
    if (index >= 0 && index < V4L2_CHN_NUM)
        __atomic_store_n(&_v4l2_idr[(unsigned char)index], 1, __ATOMIC_RELAXED);
}

int v4l2_video_snapshot_grab(char index, char quality, hal_jpegdata *jpeg)
{
    struct timespec deadline;
    int ret = EXIT_FAILURE;

    if (index < 0 || index >= V4L2_CHN_NUM) return ret;

    clock_gettime(CLOCK_REALTIME, &deadline);
    deadline.tv_sec += 3;

    pthread_mutex_lock(&_v4l2_snapLock);
    _v4l2_snapQuality = quality > 0 && quality <= 100 ? quality : 0;
    unsigned int request = ++_v4l2_snapReq;
    while (_v4l2_snapDone != request && keepRunning)
        if (pthread_cond_timedwait(&_v4l2_snapCond, &_v4l2_snapLock, &deadline))
            break;
    if (_v4l2_snapDone == request && _v4l2_snapSize) {
        void *data = realloc(jpeg->data, _v4l2_snapSize);
        if (data) {
            jpeg->data = data;
            memcpy(jpeg->data, _v4l2_snapData, _v4l2_snapSize);
            jpeg->length = jpeg->jpegSize = _v4l2_snapSize;
            ret = EXIT_SUCCESS;
        }
    }
    pthread_mutex_unlock(&_v4l2_snapLock);

    return ret;
}

/* Recover from a stalled or failed job: STREAMOFF returns every buffer,
 * including an imported capture buffer, before it is queued for capture. */
static void v4l2_enc_restart(unsigned int index)
{
    v4l2_enc *enc = &_v4l2_enc[index];
    unsigned int out = V4L2_BUF_TYPE_VIDEO_OUTPUT, cap = V4L2_BUF_TYPE_VIDEO_CAPTURE;

    HAL_WARNING("v4l2_venc", "Restarting encoder %u: %s\n", index, strerror(errno));
    v4l2_ioctl(enc->fd, VIDIOC_STREAMOFF, &out);
    v4l2_ioctl(enc->fd, VIDIOC_STREAMOFF, &cap);
    for (unsigned int i = 0; i < enc->outCount; i++) {
        struct v4l2_buffer buf = { .index = i, .type = cap, .memory = V4L2_MEMORY_MMAP };
        if (v4l2_ioctl(enc->fd, VIDIOC_QBUF, &buf)) goto failed;
    }
    if (v4l2_ioctl(enc->fd, VIDIOC_STREAMON, &cap) ||
        v4l2_ioctl(enc->fd, VIDIOC_STREAMON, &out)) goto failed;
    enc->nextDue = 0;
    return;

failed:
    HAL_DANGER("v4l2_venc", "Encoder %u stopped: %s\n", index, strerror(errno));
    v4l2_ioctl(enc->fd, VIDIOC_STREAMOFF, &cap);
    enc->failed = 1;
}

static void v4l2_deliver(unsigned int index, unsigned char *data, unsigned int size,
    unsigned long long timestamp, unsigned int sequence)
{
    v4l2_enc *enc = &_v4l2_enc[index];
    hal_vidpack pack[4] = { { .data = data, .length = size, .timestamp = timestamp } };
    hal_vidstream stream = { .pack = pack, .count = 1, .seq = sequence };

    if (enc->config.codec == HAL_VIDCODEC_JPG) {
        pthread_mutex_lock(&_v4l2_snapLock);
        void *copy = realloc(_v4l2_snapData, size);
        if (copy) {
            _v4l2_snapData = copy;
            _v4l2_snapSize = size;
            memcpy(_v4l2_snapData, data, size);
        } else _v4l2_snapSize = 0;
        _v4l2_snapDone = _v4l2_snapReq;
        pthread_cond_broadcast(&_v4l2_snapCond);
        pthread_mutex_unlock(&_v4l2_snapLock);
        return;
    }

    /* One entry per NAL, as the muxers expect. An access unit with more
     * NALs than a pack holds continues in further packs of the stream. */
    if (enc->config.codec == HAL_VIDCODEC_H264 || enc->config.codec == HAL_VIDCODEC_H265) {
        hal_vidpack *p = pack;
        unsigned int base = 0;

        for (unsigned int pos = 0; pos + 3 < size; pos++) {
            unsigned int prefix = !data[pos] && !data[pos + 1] ?
                (data[pos + 2] == 1 ? 3 : !data[pos + 2] && data[pos + 3] == 1 ? 4 : 0) : 0;
            if (!prefix || pos + prefix >= size) continue;
            if (p->naluCnt == sizeof(p->nalu) / sizeof(*p->nalu)) {
                if (stream.count == sizeof(pack) / sizeof(*pack)) break;
                p->nalu[p->naluCnt - 1].length = pos - base - p->nalu[p->naluCnt - 1].offset;
                p->length = pos - base;
                base = pos;
                p = &pack[stream.count++];
                *p = (hal_vidpack){ .data = data + pos, .length = size - pos,
                    .timestamp = timestamp };
            }
            if (p->naluCnt)
                p->nalu[p->naluCnt - 1].length = pos - base - p->nalu[p->naluCnt - 1].offset;
            unsigned char header = data[pos + prefix];
            p->nalu[p->naluCnt].offset = pos - base;
            p->nalu[p->naluCnt].type = enc->config.codec == HAL_VIDCODEC_H264 ?
                header & 0x1F : (header >> 1) & 0x3F;
            p->naluCnt++;
            pos += prefix - 1;
        }
        if (!p->naluCnt) return;
        p->nalu[p->naluCnt - 1].length = size - base - p->nalu[p->naluCnt - 1].offset;
    }

    if (v4l2_vid_cb) (*v4l2_vid_cb)(index, &stream);
}

static int v4l2_wait(int fd, struct v4l2_buffer *buf, char *done)
{
    if (*done) return 0;
    if (!v4l2_ioctl(fd, VIDIOC_DQBUF, buf)) {
        *done = 1;
        return 0;
    }
    return errno == EAGAIN ? 0 : -1;
}

static void v4l2_encode(unsigned int index, struct v4l2_buffer *raw)
{
    v4l2_enc *enc = &_v4l2_enc[index];
    unsigned long long timestamp =
        (unsigned long long)raw->timestamp.tv_sec * 1000000 + raw->timestamp.tv_usec;

    if (enc->config.codec == HAL_VIDCODEC_JPG) {
        pthread_mutex_lock(&_v4l2_snapLock);
        char pending = _v4l2_snapReq != _v4l2_snapDone;
        unsigned int quality = _v4l2_snapQuality;
        pthread_mutex_unlock(&_v4l2_snapLock);
        if (!pending) return;
        if (quality && quality != enc->quality &&
            !v4l2_control(enc->fd, V4L2_CID_JPEG_COMPRESSION_QUALITY, quality))
            enc->quality = quality;
    } else if (enc->config.framerate) {
        /* Decimate faster sensor modes to the configured rate. */
        unsigned long long period = 1000000 / enc->config.framerate;
        if (enc->nextDue && timestamp + 1000 < enc->nextDue) return;
        enc->nextDue = !enc->nextDue || timestamp > enc->nextDue + period ?
            timestamp + period : enc->nextDue + period;
    }

    if (__atomic_exchange_n(&_v4l2_idr[index], 0, __ATOMIC_RELAXED))
        v4l2_control(enc->fd, V4L2_CID_MPEG_VIDEO_FORCE_KEY_FRAME, 0);

    struct v4l2_buffer in = { .type = V4L2_BUF_TYPE_VIDEO_OUTPUT,
        .bytesused = enc->inSize, .timestamp = raw->timestamp };
    if (enc->dmabuf) {
        in.memory = V4L2_MEMORY_DMABUF;
        in.index = raw->index % enc->inCount;
        in.m.fd = _v4l2_capMap[raw->index].dmabuf;
        in.length = _v4l2_capMap[raw->index].length;
    } else {
        unsigned char *src = _v4l2_capMap[raw->index].data, *dst = enc->in[0].data;
        for (unsigned int y = 0; y < _v4l2_height * 3 / 2; y++)
            memcpy(dst + y * enc->inStride, src + y * _v4l2_stride, _v4l2_width);
        in.memory = V4L2_MEMORY_MMAP;
        in.index = 0;
    }
    if (v4l2_ioctl(enc->fd, VIDIOC_QBUF, &in)) {
        v4l2_enc_restart(index);
        return;
    }

    struct v4l2_buffer coded = { .type = V4L2_BUF_TYPE_VIDEO_CAPTURE,
        .memory = V4L2_MEMORY_MMAP };
    struct v4l2_buffer used = { .type = V4L2_BUF_TYPE_VIDEO_OUTPUT,
        .memory = in.memory };
    char haveCoded = 0, haveUsed = 0;
    unsigned int started = millis();
    while (!haveCoded || !haveUsed) {
        if (v4l2_wait(enc->fd, &coded, &haveCoded) || v4l2_wait(enc->fd, &used, &haveUsed) ||
            (unsigned int)(millis() - started) > V4L2_WAIT_MS) {
            if (!errno || errno == EAGAIN) errno = ETIMEDOUT;
            v4l2_enc_restart(index);
            return;
        }
        if (!haveCoded || !haveUsed) {
            struct pollfd pfd = { .fd = enc->fd, .events = POLLIN | POLLOUT };
            poll(&pfd, 1, 20);
        }
    }

    if (coded.index < enc->outCount && !(coded.flags & V4L2_BUF_FLAG_ERROR) &&
        coded.bytesused && coded.bytesused <= enc->out[coded.index].length)
        v4l2_deliver(index, enc->out[coded.index].data, coded.bytesused,
            timestamp, raw->sequence);

    if (v4l2_ioctl(enc->fd, VIDIOC_QBUF, &coded)) v4l2_enc_restart(index);
}

void *v4l2_video_thread(void)
{
    while (keepRunning) {
        struct pollfd pfd = { .fd = _v4l2_cap, .events = POLLIN };
        int ready = poll(&pfd, 1, 500);
        if (ready < 0 && errno == EINTR) continue;
        if (ready < 0 || pfd.revents & (POLLERR | POLLNVAL)) {
            HAL_DANGER("v4l2", "Capture failed: %s\n", strerror(errno));
            break;
        }
        if (!ready) continue;

        struct v4l2_buffer raw = { .type = V4L2_BUF_TYPE_VIDEO_CAPTURE,
            .memory = V4L2_MEMORY_MMAP };
        if (v4l2_ioctl(_v4l2_cap, VIDIOC_DQBUF, &raw)) {
            if (errno == EAGAIN) continue;
            HAL_DANGER("v4l2", "Dequeuing a frame failed: %s\n", strerror(errno));
            break;
        }

        if (raw.index < _v4l2_capCount && !(raw.flags & V4L2_BUF_FLAG_ERROR)) {
            pthread_mutex_lock(&_v4l2_pipeLock);
            for (unsigned int i = 0; i < V4L2_CHN_NUM; i++)
                if (_v4l2_state[i].enable && _v4l2_enc[i].fd >= 0 && !_v4l2_enc[i].failed)
                    v4l2_encode(i, &raw);
            pthread_mutex_unlock(&_v4l2_pipeLock);
        }

        if (v4l2_ioctl(_v4l2_cap, VIDIOC_QBUF, &raw)) {
            HAL_DANGER("v4l2", "Requeuing a frame failed: %s\n", strerror(errno));
            break;
        }
    }

    HAL_INFO("v4l2", "Shutting down the capture thread...\n");
    return NULL;
}

#else

#include <stdlib.h>

int v4l2_system_identify(void) { return 0; }
int v4l2_hal_init(void) { return EXIT_FAILURE; }
void v4l2_hal_deinit(void) {}
int v4l2_audio_init(int samplerate) { return EXIT_FAILURE; }
void v4l2_audio_deinit(void) {}
void *v4l2_audio_thread(void) { return NULL; }
int v4l2_channel_bind(char index) { return EXIT_FAILURE; }
int v4l2_channel_create(char index, short width, short height, char jpeg) { return EXIT_FAILURE; }
void v4l2_channel_grayscale(char enable) {}
int v4l2_channel_unbind(char index) { return EXIT_FAILURE; }
int v4l2_pipeline_create(short width, short height, char mirror, char flip,
    char framerate, int antiflicker) { return EXIT_FAILURE; }
void v4l2_pipeline_destroy(void) {}
int v4l2_video_create(char index, hal_vidconfig *config) { return EXIT_FAILURE; }
int v4l2_video_destroy(char index) { return EXIT_FAILURE; }
int v4l2_video_destroy_all(void) { return EXIT_FAILURE; }
void v4l2_video_request_idr(char index) {}
int v4l2_video_snapshot_grab(char index, char quality, hal_jpegdata *jpeg) { return EXIT_FAILURE; }
void *v4l2_video_thread(void) { return NULL; }

#endif
