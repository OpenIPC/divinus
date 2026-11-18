// SPDX-License-Identifier: GPL-2.0
/*
 * Ingenic T23 V4L2/MC + Zero-Copy Helix DMABUF + ALSA HAL for Divinus
 *
 * Copyright (C) 2026 Difan Zhang
 */
#include "t23_hal.h"

hal_chnstate t23_state[T23_VENC_CHN_NUM] = {0};
int (*t23_aud_cb)(hal_audframe *) = NULL;
int (*t23_vid_cb)(char, hal_vidstream *) = NULL;

#ifdef DIVINUS_T23

#include <alsa/asoundlib.h>
#include <errno.h>
#include <fcntl.h>
#include <linux/media.h>
#include <linux/v4l2-controls.h>
#include <linux/v4l2-subdev.h>
#include <linux/videodev2.h>
#include <poll.h>
#include <pthread.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <time.h>

/* Compatibility definitions for older toolchain kernel headers (e.g. 3.10). */
#ifndef MEDIA_PAD_FL_SINK
#define MEDIA_PAD_FL_SINK (1 << 0)
#endif
#ifndef MEDIA_PAD_FL_SOURCE
#define MEDIA_PAD_FL_SOURCE (1 << 1)
#endif
#ifndef MEDIA_LNK_FL_ENABLED
#define MEDIA_LNK_FL_ENABLED (1 << 0)
#endif
#ifndef MEDIA_LNK_FL_IMMUTABLE
#define MEDIA_LNK_FL_IMMUTABLE (1 << 1)
#endif
#ifndef MEDIA_LNK_FL_DYNAMIC
#define MEDIA_LNK_FL_DYNAMIC (1 << 2)
#endif
#ifndef MEDIA_LNK_FL_LINK_TYPE
#define MEDIA_LNK_FL_LINK_TYPE (0xf << 28)
#endif
#ifndef MEDIA_LNK_FL_DATA_LINK
#define MEDIA_LNK_FL_DATA_LINK (0U << 28)
#endif
#ifndef MEDIA_LNK_FL_INTERFACE_LINK
#define MEDIA_LNK_FL_INTERFACE_LINK (1U << 28)
#endif
#ifndef MEDIA_ENT_F_CAM_SENSOR
#define MEDIA_ENT_F_CAM_SENSOR 0x00020001U
#endif

#ifndef MEDIA_IOC_G_TOPOLOGY
struct media_v2_entity {
    __u32 id;
    char name[64];
    __u32 function;
    __u32 flags;
    __u32 reserved[5];
} __attribute__((packed));

struct media_v2_intf_devnode {
    __u32 major;
    __u32 minor;
} __attribute__((packed));

struct media_v2_interface {
    __u32 id;
    __u32 intf_type;
    __u32 flags;
    __u32 reserved[9];
    union {
        struct media_v2_intf_devnode devnode;
        __u32 raw[16];
    };
} __attribute__((packed));

struct media_v2_pad {
    __u32 id;
    __u32 entity_id;
    __u32 flags;
    __u32 index;
    __u32 reserved[4];
} __attribute__((packed));

struct media_v2_link {
    __u32 id;
    __u32 source_id;
    __u32 sink_id;
    __u32 flags;
    __u32 reserved[6];
} __attribute__((packed));

struct media_v2_topology {
    __u64 topology_version;
    __u32 num_entities;
    __u32 reserved1;
    __u64 ptr_entities;
    __u32 num_interfaces;
    __u32 reserved2;
    __u64 ptr_interfaces;
    __u32 num_pads;
    __u32 reserved3;
    __u64 ptr_pads;
    __u32 num_links;
    __u32 reserved4;
    __u64 ptr_links;
} __attribute__((packed));

#define MEDIA_IOC_G_TOPOLOGY _IOWR('|', 0x04, struct media_v2_topology)
#endif

#ifndef VIDIOC_SUBDEV_G_FMT
#ifndef V4L2_SUBDEV_FORMAT_ACTIVE
#define V4L2_SUBDEV_FORMAT_ACTIVE 1
#endif
struct v4l2_subdev_format {
    __u32 which;
    __u32 pad;
    struct v4l2_mbus_framefmt format;
    __u32 reserved[8];
};
#define VIDIOC_SUBDEV_G_FMT _IOWR('V', 4, struct v4l2_subdev_format)
#define VIDIOC_SUBDEV_S_FMT _IOWR('V', 5, struct v4l2_subdev_format)
#endif

#ifndef V4L2_CAP_VIDEO_M2M
#define V4L2_CAP_VIDEO_M2M 0x00008000
#endif
#ifndef V4L2_CAP_IO_MC
#define V4L2_CAP_IO_MC 0x20000000
#endif
#ifndef V4L2_CAP_DEVICE_CAPS
#define V4L2_CAP_DEVICE_CAPS 0x80000000
#endif
#ifndef V4L2_PIX_FMT_NV12
#define V4L2_PIX_FMT_NV12 v4l2_fourcc('N', 'V', '1', '2')
#endif
#ifndef V4L2_PIX_FMT_H264
#define V4L2_PIX_FMT_H264 v4l2_fourcc('H', '2', '6', '4')
#endif
#ifndef V4L2_PIX_FMT_JPEG
#define V4L2_PIX_FMT_JPEG v4l2_fourcc('J', 'P', 'E', 'G')
#endif
#ifndef V4L2_EVENT_EOS
#define V4L2_EVENT_EOS 2
#endif
#ifndef V4L2_ENC_CMD_STOP
#define V4L2_ENC_CMD_STOP 1
#endif

#ifndef V4L2_CID_MPEG_BASE
#define V4L2_CID_MPEG_BASE (V4L2_CTRL_CLASS_MPEG | 0x900)
#endif
#ifndef V4L2_CID_MPEG_VIDEO_GOP_SIZE
#define V4L2_CID_MPEG_VIDEO_GOP_SIZE (V4L2_CID_MPEG_BASE + 203)
#endif
#ifndef V4L2_CID_MPEG_VIDEO_BITRATE_MODE
#define V4L2_CID_MPEG_VIDEO_BITRATE_MODE (V4L2_CID_MPEG_BASE + 206)
#endif
#ifndef V4L2_CID_MPEG_VIDEO_BITRATE
#define V4L2_CID_MPEG_VIDEO_BITRATE (V4L2_CID_MPEG_BASE + 207)
#endif
#ifndef V4L2_CID_MPEG_VIDEO_BITRATE_PEAK
#define V4L2_CID_MPEG_VIDEO_BITRATE_PEAK (V4L2_CID_MPEG_BASE + 208)
#endif
#ifndef V4L2_CID_MPEG_VIDEO_FRAME_RC_ENABLE
#define V4L2_CID_MPEG_VIDEO_FRAME_RC_ENABLE (V4L2_CID_MPEG_BASE + 215)
#endif
#ifndef V4L2_CID_MPEG_VIDEO_HEADER_MODE
#define V4L2_CID_MPEG_VIDEO_HEADER_MODE (V4L2_CID_MPEG_BASE + 216)
#endif
#ifndef V4L2_CID_MPEG_VIDEO_REPEAT_SEQ_HEADER
#define V4L2_CID_MPEG_VIDEO_REPEAT_SEQ_HEADER (V4L2_CID_MPEG_BASE + 226)
#endif
#ifndef V4L2_CID_MPEG_VIDEO_FORCE_KEY_FRAME
#define V4L2_CID_MPEG_VIDEO_FORCE_KEY_FRAME (V4L2_CID_MPEG_BASE + 229)
#endif
#ifndef V4L2_CID_MPEG_VIDEO_H264_LEVEL
#define V4L2_CID_MPEG_VIDEO_H264_LEVEL (V4L2_CID_MPEG_BASE + 359)
#endif
#ifndef V4L2_CID_MPEG_VIDEO_H264_PROFILE
#define V4L2_CID_MPEG_VIDEO_H264_PROFILE (V4L2_CID_MPEG_BASE + 363)
#endif
#ifndef V4L2_CID_JPEG_COMPRESSION_QUALITY
#define V4L2_CID_JPEG_COMPRESSION_QUALITY (V4L2_CTRL_CLASS_JPEG | 0x903)
#endif
#ifndef V4L2_MPEG_VIDEO_BITRATE_MODE_VBR
#define V4L2_MPEG_VIDEO_BITRATE_MODE_VBR 0
#endif
#ifndef V4L2_MPEG_VIDEO_BITRATE_MODE_CBR
#define V4L2_MPEG_VIDEO_BITRATE_MODE_CBR 1
#endif
#ifndef V4L2_MPEG_VIDEO_HEADER_MODE_JOINED_WITH_1ST_FRAME
#define V4L2_MPEG_VIDEO_HEADER_MODE_JOINED_WITH_1ST_FRAME 1
#endif
#ifndef V4L2_MPEG_VIDEO_H264_PROFILE_BASELINE
#define V4L2_MPEG_VIDEO_H264_PROFILE_BASELINE 0
#endif
#ifndef V4L2_MPEG_VIDEO_H264_PROFILE_MAIN
#define V4L2_MPEG_VIDEO_H264_PROFILE_MAIN 2
#endif
#ifndef V4L2_MPEG_VIDEO_H264_PROFILE_HIGH
#define V4L2_MPEG_VIDEO_H264_PROFILE_HIGH 4
#endif

#define T23_CAP_BUFS     3
#define T23_ENC_CAP_BUFS 2
#define T23_SUB_NODES    2

static const char *const t23_sub_names[T23_SUB_NODES] = {
    "T23 scaled0",
    "T23 scaled0.1",
};

struct t23_cap_node {
    int fd;
    int streaming;
    unsigned int width;
    unsigned int height;
    size_t sizeimage;
    unsigned int count;
    int dmabuf_fd[T23_CAP_BUFS];
};

struct t23_venc_chn {
    int created;
    int bound;
    int fd;
    int streaming;
    int sub_idx;
    unsigned int req_w;
    unsigned int req_h;
    unsigned int width;
    unsigned int height;
    unsigned int fps;
    hal_vidcodec codec;
    size_t in_sizeimage;
    unsigned int cap_cnt;
    void *cap_map[T23_ENC_CAP_BUFS];
    size_t cap_len[T23_ENC_CAP_BUFS];
    uint32_t seq;
    int idr_req;
};

static pthread_mutex_t t23_mtx = PTHREAD_MUTEX_INITIALIZER;
static pthread_mutex_t t23_snap_mtx = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t t23_snap_cond = PTHREAD_COND_INITIALIZER;

static struct t23_cap_node t23_cap_main = { .fd = -1 };
static struct t23_cap_node t23_cap_sub[T23_SUB_NODES] = {
    { .fd = -1 }, { .fd = -1 },
};
static struct t23_venc_chn t23_chn[T23_VENC_CHN_NUM];

static char t23_helix_path[64] = {0};
static int t23_isp_sd_fd = -1;
static unsigned int t23_sensor_w = 1280;
static unsigned int t23_sensor_h = 720;
static bool t23_has_sub[T23_SUB_NODES] = { false, false };
static bool t23_pipe_active = false;

static struct t23_venc_chn t23_snap_enc = { .fd = -1, .sub_idx = -1 };
static int t23_snap_req = 0;
static int t23_snap_done = 0;
static int t23_snap_err = 0;
static int t23_snap_node = -1;
static int t23_snap_qual = 80;
static int t23_snap_sub_ref[T23_SUB_NODES] = { 0, 0 };
static unsigned char *t23_snap_buf = NULL;
static size_t t23_snap_len = 0;

static snd_pcm_t *t23_pcm = NULL;
static short *t23_pcm_buf = NULL;
static snd_pcm_uframes_t t23_pcm_period = 640;

static int xioctl(int fd, unsigned long req, void *arg)
{
    int r;
    do {
        r = ioctl(fd, req, arg);
    } while (r < 0 && errno == EINTR);
    return r;
}

static unsigned int t23_nal_start(const unsigned char *buf,
                                  unsigned int off, unsigned int len)
{
    for (; off + 2 < len; off++) {
        if (buf[off] == 0 && buf[off + 1] == 0) {
            if (buf[off + 2] == 1)
                return off;
            if (off + 3 < len && buf[off + 2] == 0 && buf[off + 3] == 1)
                return off;
        }
    }
    return len;
}

static int t23_open_v4l_by_name(const char *prefix, const char *want,
                                int flags, char *out_path, size_t out_sz)
{
    char sys_path[80], line[64], dev_path[64];

    for (unsigned int i = 0; i < 64; i++) {
        FILE *f;

        snprintf(sys_path, sizeof(sys_path),
                 "/sys/class/video4linux/%s%u/name", prefix, i);
        f = fopen(sys_path, "r");
        if (!f)
            continue;
        if (fgets(line, sizeof(line), f)) {
            line[strcspn(line, "\r\n")] = '\0';
            if (!strcmp(line, want)) {
                fclose(f);
                snprintf(dev_path, sizeof(dev_path), "/dev/%s%u", prefix, i);
                if (out_path && out_sz)
                    snprintf(out_path, out_sz, "%s", dev_path);
                return open(dev_path, flags);
            }
        }
        fclose(f);
    }
    errno = ENODEV;
    return -1;
}

static int t23_find_helix(char *out_path, size_t out_sz)
{
    char dev_path[64];

    for (unsigned int i = 0; i < 64; i++) {
        struct v4l2_capability cap;
        uint32_t caps;
        int fd;

        snprintf(dev_path, sizeof(dev_path), "/dev/video%u", i);
        fd = open(dev_path, O_RDWR | O_NONBLOCK | O_CLOEXEC);
        if (fd < 0)
            continue;
        memset(&cap, 0, sizeof(cap));
        if (xioctl(fd, VIDIOC_QUERYCAP, &cap) == 0) {
            caps = (cap.capabilities & V4L2_CAP_DEVICE_CAPS) ?
                   cap.device_caps : cap.capabilities;
            if ((caps & V4L2_CAP_VIDEO_M2M) &&
                (strstr((const char *)cap.card, "Helix") ||
                 strstr((const char *)cap.driver, "helix"))) {
                close(fd);
                snprintf(out_path, out_sz, "%s", dev_path);
                return 0;
            }
        }
        close(fd);
    }
    errno = ENODEV;
    return -1;
}

static int t23_open_char_dev(unsigned int major, unsigned int minor, int flags)
{
    char sys_path[64], link_buf[128], dev_path[160];
    const char *base;
    ssize_t len;

    snprintf(sys_path, sizeof(sys_path), "/sys/dev/char/%u:%u", major, minor);
    len = readlink(sys_path, link_buf, sizeof(link_buf) - 1);
    if (len <= 0)
        return -1;
    link_buf[len] = '\0';
    base = strrchr(link_buf, '/');
    base = base ? base + 1 : link_buf;
    snprintf(dev_path, sizeof(dev_path), "/dev/%s", base);
    return open(dev_path, flags);
}

static const struct media_v2_entity *
t23_mc_find_entity(const struct media_v2_entity *ents, unsigned int n, uint32_t id)
{
    for (unsigned int i = 0; i < n; i++) {
        if (ents[i].id == id)
            return &ents[i];
    }
    return NULL;
}

static uint32_t
t23_mc_upstream_entity(const struct media_v2_topology *topo,
                       const struct media_v2_pad *pads,
                       const struct media_v2_link *links,
                       uint32_t cur_id)
{
    uint32_t fallback = 0;

    for (unsigned int p = 0; p < topo->num_pads; p++) {
        if (pads[p].entity_id != cur_id ||
            !(pads[p].flags & MEDIA_PAD_FL_SINK))
            continue;
        for (unsigned int l = 0; l < topo->num_links; l++) {
            uint32_t src_ent = 0;

            if ((links[l].flags & MEDIA_LNK_FL_LINK_TYPE) != MEDIA_LNK_FL_DATA_LINK ||
                links[l].sink_id != pads[p].id)
                continue;
            for (unsigned int sp = 0; sp < topo->num_pads; sp++) {
                if (pads[sp].id == links[l].source_id) {
                    src_ent = pads[sp].entity_id;
                    break;
                }
            }
            if (!src_ent || src_ent == cur_id)
                continue;
            if (links[l].flags & MEDIA_LNK_FL_ENABLED)
                return src_ent;
            if (!fallback)
                fallback = src_ent;
        }
    }
    return fallback;
}

static void t23_probe_mc_sensor(char *sensor_name, size_t name_sz,
                                unsigned int *out_w, unsigned int *out_h)
{
    for (unsigned int m = 0; m < 8; m++) {
        char mpath[32];
        struct media_v2_topology topo;
        struct media_v2_entity *ents = NULL;
        struct media_v2_interface *intfs = NULL;
        struct media_v2_pad *pads = NULL;
        struct media_v2_link *links = NULL;
        const struct media_v2_entity *sens_ent = NULL;
        uint32_t isp_id = 0, cur_id = 0, sens_id = 0, intf_id = 0;
        int mfd;

        snprintf(mpath, sizeof(mpath), "/dev/media%u", m);
        mfd = open(mpath, O_RDWR | O_CLOEXEC);
        if (mfd < 0)
            continue;

        memset(&topo, 0, sizeof(topo));
        if (xioctl(mfd, MEDIA_IOC_G_TOPOLOGY, &topo) < 0 || !topo.num_entities) {
            close(mfd);
            continue;
        }

        ents  = calloc(topo.num_entities, sizeof(*ents));
        intfs = calloc(topo.num_interfaces ? topo.num_interfaces : 1, sizeof(*intfs));
        pads  = calloc(topo.num_pads ? topo.num_pads : 1, sizeof(*pads));
        links = calloc(topo.num_links ? topo.num_links : 1, sizeof(*links));
        if (!ents || !intfs || !pads || !links) {
            free(ents); free(intfs); free(pads); free(links);
            close(mfd);
            continue;
        }

        topo.ptr_entities   = (uintptr_t)ents;
        topo.ptr_interfaces = (uintptr_t)intfs;
        topo.ptr_pads       = (uintptr_t)pads;
        topo.ptr_links      = (uintptr_t)links;
        if (xioctl(mfd, MEDIA_IOC_G_TOPOLOGY, &topo) < 0) {
            free(ents); free(intfs); free(pads); free(links);
            close(mfd);
            continue;
        }

        for (unsigned int i = 0; i < topo.num_entities; i++) {
            if (!strcmp(ents[i].name, "t23-isp-core0")) {
                isp_id = ents[i].id;
                break;
            }
        }

        /* Walk upstream links (ISP <- [VIC <- CSI <-] sensor) until reaching the sensor entity. */
        cur_id = isp_id;
        for (unsigned int hop = 0; cur_id && hop < topo.num_entities; hop++) {
            uint32_t next_id = t23_mc_upstream_entity(&topo, pads, links, cur_id);
            const struct media_v2_entity *ent;

            if (!next_id)
                break;
            sens_id = next_id;
            ent = t23_mc_find_entity(ents, topo.num_entities, next_id);
            if (ent && ent->function == MEDIA_ENT_F_CAM_SENSOR) {
                sens_ent = ent;
                break;
            }
            cur_id = next_id;
        }

        if (isp_id && (!sens_ent || sens_ent->function != MEDIA_ENT_F_CAM_SENSOR)) {
            for (unsigned int i = 0; i < topo.num_entities; i++) {
                if (ents[i].function == MEDIA_ENT_F_CAM_SENSOR) {
                    sens_ent = &ents[i];
                    sens_id = ents[i].id;
                    break;
                }
            }
        }

        if (sens_id) {
            if (!sens_ent)
                sens_ent = t23_mc_find_entity(ents, topo.num_entities, sens_id);
            if (sens_ent && sensor_name && name_sz) {
                const char *src = sens_ent->name;
                size_t nlen = strnlen(src, sizeof(sens_ent->name));
                const char *sp = memchr(src, ' ', nlen);
                size_t slen = sp ? (size_t)(sp - src) : nlen;
                if (slen >= name_sz)
                    slen = name_sz - 1;
                memcpy(sensor_name, src, slen);
                sensor_name[slen] = '\0';
            }
            for (unsigned int i = 0; i < topo.num_links; i++) {
                if ((links[i].flags & MEDIA_LNK_FL_LINK_TYPE) == MEDIA_LNK_FL_INTERFACE_LINK &&
                    links[i].sink_id == sens_id) {
                    intf_id = links[i].source_id;
                    break;
                }
            }
            for (unsigned int i = 0; i < topo.num_interfaces; i++) {
                if (intfs[i].id == intf_id) {
                    int sfd = t23_open_char_dev(intfs[i].devnode.major,
                                                intfs[i].devnode.minor,
                                                O_RDWR | O_CLOEXEC);
                    if (sfd >= 0) {
                        struct v4l2_subdev_format sdfmt;
                        memset(&sdfmt, 0, sizeof(sdfmt));
                        sdfmt.which = V4L2_SUBDEV_FORMAT_ACTIVE;
                        sdfmt.pad = 0;
                        if (xioctl(sfd, VIDIOC_SUBDEV_G_FMT, &sdfmt) == 0 &&
                            sdfmt.format.width && sdfmt.format.height) {
                            if (out_w) *out_w = sdfmt.format.width;
                            if (out_h) *out_h = sdfmt.format.height;
                        }
                        close(sfd);
                    }
                    break;
                }
            }
        }

        free(ents); free(intfs); free(pads); free(links);
        close(mfd);
        if (isp_id)
            return;
    }
}

static void t23_cap_close(struct t23_cap_node *cap)
{
    enum v4l2_buf_type type = V4L2_BUF_TYPE_VIDEO_CAPTURE;

    if (cap->fd >= 0) {
        if (cap->streaming)
            xioctl(cap->fd, VIDIOC_STREAMOFF, &type);
        for (unsigned int i = 0; i < cap->count && i < T23_CAP_BUFS; i++) {
            if (cap->dmabuf_fd[i] >= 0) {
                close(cap->dmabuf_fd[i]);
                cap->dmabuf_fd[i] = -1;
            }
        }
        struct v4l2_requestbuffers req = {
            .count = 0,
            .type = V4L2_BUF_TYPE_VIDEO_CAPTURE,
            .memory = V4L2_MEMORY_MMAP,
        };
        xioctl(cap->fd, VIDIOC_REQBUFS, &req);
        close(cap->fd);
    }
    cap->fd = -1;
    cap->streaming = 0;
    cap->count = 0;
}

static int t23_cap_start(struct t23_cap_node *cap, const char *vdev_name,
                         unsigned int width, unsigned int height)
{
    struct v4l2_format fmt;
    struct v4l2_requestbuffers req;
    enum v4l2_buf_type type = V4L2_BUF_TYPE_VIDEO_CAPTURE;

    for (unsigned int i = 0; i < T23_CAP_BUFS; i++)
        cap->dmabuf_fd[i] = -1;

    cap->fd = t23_open_v4l_by_name("video", vdev_name,
                                   O_RDWR | O_NONBLOCK | O_CLOEXEC,
                                   NULL, 0);
    if (cap->fd < 0) {
        HAL_DANGER("t23_isp", "Failed to open '%s': %s\n", vdev_name, strerror(errno));
        return EXIT_FAILURE;
    }

    memset(&fmt, 0, sizeof(fmt));
    fmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    fmt.fmt.pix.width = width;
    fmt.fmt.pix.height = height;
    fmt.fmt.pix.pixelformat = V4L2_PIX_FMT_NV12;
    if (xioctl(cap->fd, VIDIOC_S_FMT, &fmt) < 0) {
        HAL_DANGER("t23_isp", "S_FMT failed on '%s' (%ux%u): %s\n",
                   vdev_name, width, height, strerror(errno));
        t23_cap_close(cap);
        return EXIT_FAILURE;
    }

    cap->width = fmt.fmt.pix.width;
    cap->height = fmt.fmt.pix.height;
    cap->sizeimage = fmt.fmt.pix.sizeimage;

    memset(&req, 0, sizeof(req));
    req.count = T23_CAP_BUFS;
    req.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    req.memory = V4L2_MEMORY_MMAP;
    if (xioctl(cap->fd, VIDIOC_REQBUFS, &req) < 0 || req.count < 1) {
        HAL_DANGER("t23_isp", "REQBUFS failed on '%s': %s\n", vdev_name, strerror(errno));
        t23_cap_close(cap);
        return EXIT_FAILURE;
    }
    cap->count = req.count > T23_CAP_BUFS ? T23_CAP_BUFS : req.count;

    for (unsigned int i = 0; i < cap->count; i++) {
        struct v4l2_exportbuffer exp;
        struct v4l2_buffer b;

        memset(&exp, 0, sizeof(exp));
        exp.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        exp.index = i;
        exp.flags = O_RDWR | O_CLOEXEC;
        if (xioctl(cap->fd, VIDIOC_EXPBUF, &exp) < 0) {
            HAL_DANGER("t23_isp", "EXPBUF %u failed on '%s': %s\n",
                       i, vdev_name, strerror(errno));
            t23_cap_close(cap);
            return EXIT_FAILURE;
        }
        cap->dmabuf_fd[i] = exp.fd;

        memset(&b, 0, sizeof(b));
        b.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        b.memory = V4L2_MEMORY_MMAP;
        b.index = i;
        if (xioctl(cap->fd, VIDIOC_QBUF, &b) < 0) {
            HAL_DANGER("t23_isp", "QBUF %u failed on '%s': %s\n",
                       i, vdev_name, strerror(errno));
            t23_cap_close(cap);
            return EXIT_FAILURE;
        }
    }

    if (xioctl(cap->fd, VIDIOC_STREAMON, &type) < 0) {
        HAL_DANGER("t23_isp", "STREAMON failed on '%s': %s\n", vdev_name, strerror(errno));
        t23_cap_close(cap);
        return EXIT_FAILURE;
    }
    cap->streaming = 1;
    HAL_INFO("t23_isp", "Capture node '%s' streaming NV12 %ux%u (%u DMABUF slots)\n",
             vdev_name, cap->width, cap->height, cap->count);
    return EXIT_SUCCESS;
}

static int t23_s_ctrl(int fd, uint32_t id, int32_t val)
{
    struct v4l2_control c = { .id = id, .value = val };
    return xioctl(fd, VIDIOC_S_CTRL, &c);
}

static void t23_enc_close(struct t23_venc_chn *e)
{
    enum v4l2_buf_type t_out = V4L2_BUF_TYPE_VIDEO_OUTPUT;
    enum v4l2_buf_type t_cap = V4L2_BUF_TYPE_VIDEO_CAPTURE;

    if (e->fd >= 0) {
        if (e->streaming) {
            xioctl(e->fd, VIDIOC_STREAMOFF, &t_out);
            xioctl(e->fd, VIDIOC_STREAMOFF, &t_cap);
        }
        for (unsigned int i = 0; i < e->cap_cnt && i < T23_ENC_CAP_BUFS; i++) {
            if (e->cap_map[i] && e->cap_map[i] != MAP_FAILED) {
                munmap(e->cap_map[i], e->cap_len[i]);
                e->cap_map[i] = NULL;
                e->cap_len[i] = 0;
            }
        }
        struct v4l2_requestbuffers req_out = {
            .count = 0, .type = t_out, .memory = V4L2_MEMORY_DMABUF
        };
        struct v4l2_requestbuffers req_cap = {
            .count = 0, .type = t_cap, .memory = V4L2_MEMORY_MMAP
        };
        xioctl(e->fd, VIDIOC_REQBUFS, &req_out);
        xioctl(e->fd, VIDIOC_REQBUFS, &req_cap);
        close(e->fd);
    }
    e->fd = -1;
    e->streaming = 0;
    e->bound = 0;
    e->created = 0;
    e->cap_cnt = 0;
    e->seq = 0;
    e->idr_req = 0;
}

static int t23_enc_open_m2m(struct t23_venc_chn *e, unsigned int w, unsigned int h,
                            hal_vidconfig *config)
{
    struct v4l2_format fmt;
    struct v4l2_requestbuffers req;
    enum v4l2_buf_type t_out = V4L2_BUF_TYPE_VIDEO_OUTPUT;
    enum v4l2_buf_type t_cap = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    uint32_t out_fourcc;

    if (!t23_helix_path[0] && t23_find_helix(t23_helix_path, sizeof(t23_helix_path)) < 0) {
        HAL_DANGER("t23_venc", "Helix V4L2 M2M device not found\n");
        return EXIT_FAILURE;
    }

    e->fd = open(t23_helix_path, O_RDWR | O_NONBLOCK | O_CLOEXEC);
    if (e->fd < 0) {
        HAL_DANGER("t23_venc", "Failed to open %s: %s\n", t23_helix_path, strerror(errno));
        return EXIT_FAILURE;
    }

    if (config->codec == HAL_VIDCODEC_H264) {
        int32_t prof = V4L2_MPEG_VIDEO_H264_PROFILE_HIGH;
        int32_t bps = config->bitrate > 0 ? (int32_t)config->bitrate * 1000 : 1536000;
        int32_t peak = config->maxBitrate > 0 ? (int32_t)config->maxBitrate * 1000 : bps * 5 / 4;
        if (bps < 16000) bps = 16000;
        if (bps > 20000000) bps = 20000000;
        if (peak < bps) peak = bps;
        if (peak > 20000000) peak = 20000000;

        if (config->profile == HAL_VIDPROFILE_BASELINE)
            prof = V4L2_MPEG_VIDEO_H264_PROFILE_BASELINE;
        else if (config->profile == HAL_VIDPROFILE_MAIN)
            prof = V4L2_MPEG_VIDEO_H264_PROFILE_MAIN;

        if (t23_s_ctrl(e->fd, V4L2_CID_MPEG_VIDEO_H264_PROFILE, prof) < 0)
            t23_s_ctrl(e->fd, V4L2_CID_MPEG_VIDEO_H264_PROFILE,
                       V4L2_MPEG_VIDEO_H264_PROFILE_HIGH);

        t23_s_ctrl(e->fd, V4L2_CID_MPEG_VIDEO_GOP_SIZE,
                   config->gop ? config->gop : 50);
        t23_s_ctrl(e->fd, V4L2_CID_MPEG_VIDEO_HEADER_MODE,
                   V4L2_MPEG_VIDEO_HEADER_MODE_JOINED_WITH_1ST_FRAME);
        t23_s_ctrl(e->fd, V4L2_CID_MPEG_VIDEO_REPEAT_SEQ_HEADER, 1);
        t23_s_ctrl(e->fd, V4L2_CID_MPEG_VIDEO_FRAME_RC_ENABLE,
                   config->mode != HAL_VIDMODE_QP ? 1 : 0);
        t23_s_ctrl(e->fd, V4L2_CID_MPEG_VIDEO_BITRATE_MODE,
                   config->mode == HAL_VIDMODE_CBR ?
                   V4L2_MPEG_VIDEO_BITRATE_MODE_CBR :
                   V4L2_MPEG_VIDEO_BITRATE_MODE_VBR);
        t23_s_ctrl(e->fd, V4L2_CID_MPEG_VIDEO_BITRATE, bps);
        t23_s_ctrl(e->fd, V4L2_CID_MPEG_VIDEO_BITRATE_PEAK, peak);

        if (config->framerate > 0) {
            struct v4l2_streamparm sp;
            memset(&sp, 0, sizeof(sp));
            sp.type = t_out;
            sp.parm.output.timeperframe.numerator = 1;
            sp.parm.output.timeperframe.denominator = config->framerate;
            xioctl(e->fd, VIDIOC_S_PARM, &sp);
        }
        out_fourcc = V4L2_PIX_FMT_H264;
    } else if (config->codec == HAL_VIDCODEC_JPG || config->codec == HAL_VIDCODEC_MJPG) {
        int q = config->maxQual ? config->maxQual : (config->minQual ? config->minQual : 80);
        if (q < 1) q = 1;
        if (q > 100) q = 100;
        t23_s_ctrl(e->fd, V4L2_CID_JPEG_COMPRESSION_QUALITY, q);
        out_fourcc = V4L2_PIX_FMT_JPEG;
    } else {
        HAL_DANGER("t23_venc", "Unsupported codec %d (Helix supports H264/JPEG)\n",
                   config->codec);
        t23_enc_close(e);
        return EXIT_FAILURE;
    }

    memset(&fmt, 0, sizeof(fmt));
    fmt.type = t_out;
    fmt.fmt.pix.width = w;
    fmt.fmt.pix.height = h;
    fmt.fmt.pix.pixelformat = V4L2_PIX_FMT_NV12;
    if (xioctl(e->fd, VIDIOC_S_FMT, &fmt) < 0) {
        HAL_DANGER("t23_venc", "Helix OUTPUT S_FMT (%ux%u) failed: %s\n",
                   w, h, strerror(errno));
        t23_enc_close(e);
        return EXIT_FAILURE;
    }
    e->width = fmt.fmt.pix.width;
    e->height = fmt.fmt.pix.height;
    e->in_sizeimage = fmt.fmt.pix.sizeimage;

    memset(&fmt, 0, sizeof(fmt));
    fmt.type = t_cap;
    fmt.fmt.pix.width = e->width;
    fmt.fmt.pix.height = e->height;
    fmt.fmt.pix.pixelformat = out_fourcc;
    if (xioctl(e->fd, VIDIOC_S_FMT, &fmt) < 0) {
        HAL_DANGER("t23_venc", "Helix CAPTURE S_FMT failed: %s\n", strerror(errno));
        t23_enc_close(e);
        return EXIT_FAILURE;
    }

    memset(&req, 0, sizeof(req));
    req.count = T23_CAP_BUFS;
    req.type = t_out;
    req.memory = V4L2_MEMORY_DMABUF;
    if (xioctl(e->fd, VIDIOC_REQBUFS, &req) < 0 || req.count < 1) {
        HAL_DANGER("t23_venc", "Helix OUTPUT REQBUFS DMABUF failed: %s\n", strerror(errno));
        t23_enc_close(e);
        return EXIT_FAILURE;
    }

    memset(&req, 0, sizeof(req));
    req.count = T23_ENC_CAP_BUFS;
    req.type = t_cap;
    req.memory = V4L2_MEMORY_MMAP;
    if (xioctl(e->fd, VIDIOC_REQBUFS, &req) < 0 || req.count < 1) {
        HAL_DANGER("t23_venc", "Helix CAPTURE REQBUFS failed: %s\n", strerror(errno));
        t23_enc_close(e);
        return EXIT_FAILURE;
    }
    e->cap_cnt = req.count > T23_ENC_CAP_BUFS ? T23_ENC_CAP_BUFS : req.count;

    for (unsigned int i = 0; i < e->cap_cnt; i++) {
        struct v4l2_buffer b;
        memset(&b, 0, sizeof(b));
        b.type = t_cap;
        b.memory = V4L2_MEMORY_MMAP;
        b.index = i;
        if (xioctl(e->fd, VIDIOC_QUERYBUF, &b) < 0) {
            t23_enc_close(e);
            return EXIT_FAILURE;
        }
        e->cap_len[i] = b.length;
        e->cap_map[i] = mmap(NULL, b.length, PROT_READ | PROT_WRITE,
                             MAP_SHARED, e->fd, (off_t)b.m.offset);
        if (e->cap_map[i] == MAP_FAILED) {
            e->cap_map[i] = NULL;
            t23_enc_close(e);
            return EXIT_FAILURE;
        }
        if (xioctl(e->fd, VIDIOC_QBUF, &b) < 0) {
            t23_enc_close(e);
            return EXIT_FAILURE;
        }
    }

    if (xioctl(e->fd, VIDIOC_STREAMON, &t_out) < 0 ||
        xioctl(e->fd, VIDIOC_STREAMON, &t_cap) < 0) {
        HAL_DANGER("t23_venc", "Helix STREAMON failed: %s\n", strerror(errno));
        t23_enc_close(e);
        return EXIT_FAILURE;
    }

    e->codec = config->codec;
    e->streaming = 1;
    e->created = 1;
    e->seq = 0;
    e->idr_req = 0;
    return EXIT_SUCCESS;
}

static void t23_enc_recover(struct t23_venc_chn *e)
{
    enum v4l2_buf_type t_out = V4L2_BUF_TYPE_VIDEO_OUTPUT;
    enum v4l2_buf_type t_cap = V4L2_BUF_TYPE_VIDEO_CAPTURE;

    if (e->fd < 0)
        return;

    xioctl(e->fd, VIDIOC_STREAMOFF, &t_out);
    xioctl(e->fd, VIDIOC_STREAMOFF, &t_cap);

    for (unsigned int i = 0; i < e->cap_cnt && i < T23_ENC_CAP_BUFS; i++) {
        struct v4l2_buffer b;
        memset(&b, 0, sizeof(b));
        b.type = t_cap;
        b.memory = V4L2_MEMORY_MMAP;
        b.index = i;
        if (xioctl(e->fd, VIDIOC_QBUF, &b) < 0) {
            HAL_DANGER("t23_venc", "Helix CAPTURE re-QBUF failed: %s\n", strerror(errno));
            e->streaming = 0;
            return;
        }
    }

    if (xioctl(e->fd, VIDIOC_STREAMON, &t_out) < 0 ||
        xioctl(e->fd, VIDIOC_STREAMON, &t_cap) < 0) {
        HAL_DANGER("t23_venc", "Helix STREAMON recovery failed: %s\n", strerror(errno));
        e->streaming = 0;
    }
}

static int t23_enc_encode_dmabuf(struct t23_venc_chn *e, unsigned int slot,
                                 int dmabuf_fd, const void **out_ptr,
                                 size_t *out_len, struct v4l2_buffer *out_bc)
{
    struct v4l2_buffer bo;
    struct v4l2_buffer bc;
    struct pollfd pfd;
    int prc;

    memset(&bo, 0, sizeof(bo));
    bo.type = V4L2_BUF_TYPE_VIDEO_OUTPUT;
    bo.memory = V4L2_MEMORY_DMABUF;
    bo.index = slot;
    bo.m.fd = dmabuf_fd;
    bo.bytesused = e->in_sizeimage;
    bo.length = 0;

    if (xioctl(e->fd, VIDIOC_QBUF, &bo) < 0) {
        HAL_DANGER("t23_venc", "Helix OUTPUT QBUF failed: %s\n", strerror(errno));
        return -1;
    }

    pfd.fd = e->fd;
    pfd.events = POLLIN;
    pfd.revents = 0;
    do {
        prc = poll(&pfd, 1, 2000);
    } while (prc < 0 && errno == EINTR);
    if (prc <= 0) {
        HAL_DANGER("t23_venc", "Helix poll timeout/err: rc=%d errno=%d\n", prc, errno);
        t23_enc_recover(e);
        return -1;
    }

    memset(&bc, 0, sizeof(bc));
    bc.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    bc.memory = V4L2_MEMORY_MMAP;
    if (xioctl(e->fd, VIDIOC_DQBUF, &bc) < 0) {
        HAL_DANGER("t23_venc", "Helix CAPTURE DQBUF failed: %s\n", strerror(errno));
        t23_enc_recover(e);
        return -1;
    }

    memset(&bo, 0, sizeof(bo));
    bo.type = V4L2_BUF_TYPE_VIDEO_OUTPUT;
    bo.memory = V4L2_MEMORY_DMABUF;
    if (xioctl(e->fd, VIDIOC_DQBUF, &bo) < 0) {
        HAL_DANGER("t23_venc", "Helix OUTPUT DQBUF failed: %s\n", strerror(errno));
        t23_enc_recover(e);
        return -1;
    }

    *out_ptr = e->cap_map[bc.index];
    *out_len = bc.bytesused;
    *out_bc = bc;
    return 0;
}

static void t23_emit_packet(char index, const void *out, size_t outlen,
                            uint64_t ts_us, uint32_t seq)
{
    hal_vidstream outStrm;
    hal_vidpack outPack[1];

    if (!t23_vid_cb || !outlen)
        return;

    memset(outPack, 0, sizeof(outPack));
    outStrm.count = 1;
    outStrm.seq = seq;
    outPack[0].data = (unsigned char *)out;
    outPack[0].length = (unsigned int)outlen;

    if (t23_state[(int)index].payload == HAL_VIDCODEC_H264) {
        const unsigned char *d = out;
        unsigned int n = 0, sc, scan = 0;

        while (n < 8 && (sc = t23_nal_start(d, scan, (unsigned int)outlen)) < outlen) {
            unsigned int scLen = (d[sc + 2] == 1) ? 3 : 4;
            if (sc + scLen >= outlen)
                break;
            outPack[0].nalu[n].offset = sc;
            outPack[0].nalu[n].type = d[sc + scLen] & 0x1F;
            n++;
            scan = sc + scLen;
        }
        if (n) {
            outPack[0].naluCnt = (int)n;
            for (unsigned int k = 0; k < n; k++) {
                unsigned int end = (k + 1 < n) ?
                    outPack[0].nalu[k + 1].offset : (unsigned int)outlen;
                outPack[0].nalu[k].length = end - outPack[0].nalu[k].offset;
            }
        }
    }

    if (!outPack[0].naluCnt) {
        outPack[0].naluCnt = 1;
        outPack[0].nalu[0].offset = 0;
        outPack[0].nalu[0].length = (unsigned int)outlen;
        outPack[0].nalu[0].type = 1;
    }

    outPack[0].offset = 0;
    outPack[0].timestamp = ts_us;
    outStrm.pack = outPack;
    (*t23_vid_cb)(index, &outStrm);
}

static void t23_drain_node(struct t23_cap_node *cap, int node_idx, int polled_fd)
{
    struct v4l2_buffer b;
    uint64_t ts_us;

    pthread_mutex_lock(&t23_mtx);
    if (!cap->streaming || cap->fd < 0 || cap->fd != polled_fd) {
        pthread_mutex_unlock(&t23_mtx);
        return;
    }

    memset(&b, 0, sizeof(b));
    b.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    b.memory = V4L2_MEMORY_MMAP;
    if (xioctl(cap->fd, VIDIOC_DQBUF, &b) < 0) {
        pthread_mutex_unlock(&t23_mtx);
        return;
    }

    ts_us = (uint64_t)b.timestamp.tv_sec * 1000000ULL + (uint64_t)b.timestamp.tv_usec;

    if (b.index < cap->count) {
        for (int i = 0; i < T23_VENC_CHN_NUM; i++) {
            const void *out = NULL;
            size_t outlen = 0;
            struct v4l2_buffer bc;

            if (!t23_state[i].enable || !t23_state[i].mainLoop)
                continue;
            if (!t23_chn[i].created || !t23_chn[i].bound || !t23_chn[i].streaming)
                continue;
            if (t23_chn[i].sub_idx != node_idx)
                continue;

            if (t23_chn[i].codec == HAL_VIDCODEC_H264 &&
                __atomic_exchange_n(&t23_chn[i].idr_req, 0, __ATOMIC_ACQ_REL))
                t23_s_ctrl(t23_chn[i].fd, V4L2_CID_MPEG_VIDEO_FORCE_KEY_FRAME, 1);

            if (t23_enc_encode_dmabuf(&t23_chn[i], b.index, cap->dmabuf_fd[b.index],
                                      &out, &outlen, &bc) == 0) {
                t23_emit_packet((char)i, out, outlen, ts_us, t23_chn[i].seq++);
                if (xioctl(t23_chn[i].fd, VIDIOC_QBUF, &bc) < 0)
                    t23_enc_recover(&t23_chn[i]);
            }
        }

        pthread_mutex_lock(&t23_snap_mtx);
        if (t23_snap_req && !t23_snap_done && t23_snap_node == node_idx) {
            int q = (t23_snap_qual >= 1 && t23_snap_qual <= 100) ? t23_snap_qual : 80;
            if (t23_snap_enc.fd >= 0 &&
                (t23_snap_enc.width != cap->width ||
                 t23_snap_enc.height != cap->height ||
                 !t23_snap_enc.streaming)) {
                t23_enc_close(&t23_snap_enc);
            }
            if (t23_snap_enc.fd < 0) {
                hal_vidconfig scfg = {0};
                scfg.width = (unsigned short)cap->width;
                scfg.height = (unsigned short)cap->height;
                scfg.codec = HAL_VIDCODEC_JPG;
                scfg.mode = HAL_VIDMODE_QP;
                scfg.maxQual = (unsigned char)q;
                if (t23_enc_open_m2m(&t23_snap_enc, cap->width, cap->height, &scfg))
                    t23_snap_err = EXIT_FAILURE;
            } else {
                t23_s_ctrl(t23_snap_enc.fd, V4L2_CID_JPEG_COMPRESSION_QUALITY, q);
            }
            if (t23_snap_enc.fd >= 0 && t23_snap_enc.streaming) {
                const void *jptr = NULL;
                size_t jlen = 0;
                struct v4l2_buffer bc;
                if (t23_enc_encode_dmabuf(&t23_snap_enc, b.index, cap->dmabuf_fd[b.index],
                                          &jptr, &jlen, &bc) == 0) {
                    if (jlen > 0) {
                        unsigned char *nb = malloc(jlen);
                        if (nb) {
                            memcpy(nb, jptr, jlen);
                            free(t23_snap_buf);
                            t23_snap_buf = nb;
                            t23_snap_len = jlen;
                            t23_snap_err = 0;
                        } else {
                            t23_snap_err = EXIT_FAILURE;
                        }
                    } else {
                        t23_snap_err = EXIT_FAILURE;
                    }
                    if (xioctl(t23_snap_enc.fd, VIDIOC_QBUF, &bc) < 0)
                        t23_enc_recover(&t23_snap_enc);
                } else {
                    t23_snap_err = EXIT_FAILURE;
                }
            }
            t23_snap_done = 1;
            pthread_cond_broadcast(&t23_snap_cond);
        }
        pthread_mutex_unlock(&t23_snap_mtx);
    }

    if (cap->streaming && cap->fd >= 0)
        xioctl(cap->fd, VIDIOC_QBUF, &b);
    pthread_mutex_unlock(&t23_mtx);
}

void t23_hal_deinit(void)
{
    if (t23_isp_sd_fd >= 0) {
        close(t23_isp_sd_fd);
        t23_isp_sd_fd = -1;
    }
}

int t23_hal_init(void)
{
    for (int i = 0; i < T23_VENC_CHN_NUM; i++) {
        memset(&t23_chn[i], 0, sizeof(t23_chn[i]));
        t23_chn[i].fd = -1;
        t23_chn[i].sub_idx = -1;
        t23_state[i].fileDesc = -1;
    }
    t23_snap_enc.fd = -1;
    t23_snap_enc.sub_idx = -1;
    t23_snap_node = -1;
    t23_snap_qual = 80;
    for (int s = 0; s < T23_SUB_NODES; s++) {
        t23_has_sub[s] = false;
        t23_snap_sub_ref[s] = 0;
    }
    return EXIT_SUCCESS;
}

void t23_audio_deinit(void)
{
    if (t23_pcm) {
        snd_pcm_drop(t23_pcm);
        snd_pcm_close(t23_pcm);
        t23_pcm = NULL;
    }
    free(t23_pcm_buf);
    t23_pcm_buf = NULL;
}

int t23_audio_init(int samplerate)
{
    const char *device = getenv("DIVINUS_ALSA_DEVICE");
    snd_pcm_hw_params_t *hw = NULL;
    unsigned int req_rate = samplerate > 0 ? (unsigned int)samplerate : 16000;
    unsigned int rate = req_rate;
    unsigned int periods = 32;
    snd_pcm_uframes_t period, buf_size;
    int err;

    t23_audio_deinit();

    if (!device || !device[0])
        device = "hw:0,0";

    period = req_rate / 25;
    if (period < 160) period = 160;
    if (period > 1024) period = 1024;
    t23_pcm_period = period;

    t23_pcm_buf = malloc(period * sizeof(short));
    if (!t23_pcm_buf)
        return EXIT_FAILURE;

    err = snd_pcm_open(&t23_pcm, device, SND_PCM_STREAM_CAPTURE, 0);
    if (err < 0) {
        HAL_DANGER("t23_aud", "snd_pcm_open(%s) failed: %s\n", device, snd_strerror(err));
        goto fail;
    }

    snd_pcm_hw_params_alloca(&hw);
    if ((err = snd_pcm_hw_params_any(t23_pcm, hw)) < 0 ||
        (err = snd_pcm_hw_params_set_access(t23_pcm, hw, SND_PCM_ACCESS_RW_INTERLEAVED)) < 0 ||
        (err = snd_pcm_hw_params_set_format(t23_pcm, hw, SND_PCM_FORMAT_S16_LE)) < 0 ||
        (err = snd_pcm_hw_params_set_channels(t23_pcm, hw, 1)) < 0 ||
        (err = snd_pcm_hw_params_set_rate_near(t23_pcm, hw, &rate, 0)) < 0) {
        HAL_DANGER("t23_aud", "ALSA hw_params failed: %s\n", snd_strerror(err));
        goto fail;
    }
    if (rate != req_rate) {
        HAL_DANGER("t23_aud", "ALSA capture rate mismatch on %s: requested %u Hz, got %u Hz\n",
                   device, req_rate, rate);
        goto fail;
    }
    if ((err = snd_pcm_hw_params_set_periods_integer(t23_pcm, hw)) < 0 ||
        (err = snd_pcm_hw_params_set_period_size_near(t23_pcm, hw, &t23_pcm_period, 0)) < 0 ||
        (err = snd_pcm_hw_params_set_periods_near(t23_pcm, hw, &periods, 0)) < 0 ||
        (buf_size = t23_pcm_period * periods,
         (err = snd_pcm_hw_params_set_buffer_size_near(t23_pcm, hw, &buf_size)) < 0) ||
        (err = snd_pcm_hw_params(t23_pcm, hw)) < 0 ||
        (err = snd_pcm_prepare(t23_pcm)) < 0 ||
        (err = snd_pcm_start(t23_pcm)) < 0) {
        HAL_DANGER("t23_aud", "ALSA hw_params failed: %s\n", snd_strerror(err));
        goto fail;
    }

    if (t23_pcm_period > period) {
        free(t23_pcm_buf);
        t23_pcm_buf = malloc(t23_pcm_period * sizeof(short));
        if (!t23_pcm_buf)
            goto fail;
    }

    HAL_INFO("t23_aud", "ALSA capture %s initialized (%u Hz, mono S16_LE, period %lu)\n",
             device, rate, (unsigned long)t23_pcm_period);
    return EXIT_SUCCESS;

fail:
    t23_audio_deinit();
    return EXIT_FAILURE;
}

void *t23_audio_thread(void)
{
    unsigned int seq = 0;

    while (keepRunning && audioOn) {
        int wr;
        snd_pcm_sframes_t n;

        if (!t23_pcm || !t23_pcm_buf) {
            usleep(20000);
            continue;
        }
        if (snd_pcm_state(t23_pcm) == SND_PCM_STATE_PREPARED)
            snd_pcm_start(t23_pcm);
        wr = snd_pcm_wait(t23_pcm, 200);
        if (wr == 0)
            continue;
        if (wr < 0) {
            snd_pcm_recover(t23_pcm, wr, 1);
            continue;
        }

        n = snd_pcm_readi(t23_pcm, t23_pcm_buf, t23_pcm_period);
        if (n < 0) {
            if (n == -EAGAIN)
                continue;
            if (snd_pcm_recover(t23_pcm, (int)n, 1) < 0)
                usleep(20000);
            continue;
        }
        if (n > 0 && t23_aud_cb) {
            hal_audframe out;
            memset(&out, 0, sizeof(out));
            out.channelCnt = 1;
            out.interleavedOn = 1;
            out.data[0] = (unsigned char *)t23_pcm_buf;
            out.length[0] = (unsigned int)n * sizeof(short);
            out.seq = seq++;
            out.timestamp = millis();
            (*t23_aud_cb)(&out);
        }
    }
    HAL_INFO("t23_aud", "Shutting down audio capture thread...\n");
    return NULL;
}

int t23_channel_bind(char index)
{
    if (index < 0 || index >= T23_VENC_CHN_NUM)
        return EXIT_FAILURE;
    pthread_mutex_lock(&t23_mtx);
    t23_chn[(int)index].bound = 1;
    t23_state[(int)index].fileDesc = t23_chn[(int)index].fd;
    t23_state[(int)index].payload = t23_chn[(int)index].codec;
    pthread_mutex_unlock(&t23_mtx);
    return EXIT_SUCCESS;
}

static void t23_align_sub_dims(unsigned int req_w, unsigned int req_h,
                               unsigned int *out_w, unsigned int *out_h)
{
    unsigned int w = ALIGN_BACK(req_w, 16);
    unsigned int h = ALIGN_BACK(req_h, 8);

    if (w < 64) w = 64;
    if (h < 64) h = 64;
    if (w > t23_sensor_w) w = ALIGN_BACK(t23_sensor_w, 16);
    if (h > t23_sensor_h) h = ALIGN_BACK(t23_sensor_h, 8);
    *out_w = w;
    *out_h = h;
}

static bool t23_sub_in_use_locked(int sub_idx)
{
    if (sub_idx < 0 || sub_idx >= T23_SUB_NODES)
        return false;
    if (t23_snap_sub_ref[sub_idx] > 0)
        return true;
    for (int i = 0; i < T23_VENC_CHN_NUM; i++) {
        if (t23_chn[i].created && t23_chn[i].sub_idx == sub_idx)
            return true;
    }
    return false;
}

static void t23_sub_cleanup_unused_locked(void)
{
    for (int s = 0; s < T23_SUB_NODES; s++) {
        if (t23_cap_sub[s].streaming && !t23_sub_in_use_locked(s))
            t23_cap_close(&t23_cap_sub[s]);
    }
}

static int t23_select_sub_node_locked(unsigned int w, unsigned int h,
                                      bool allow_main_fallback,
                                      const char *tag)
{
    int best = -1;
    unsigned int best_diff = UINT32_MAX;

    for (int s = 0; s < T23_SUB_NODES; s++) {
        if (t23_cap_sub[s].streaming &&
            t23_cap_sub[s].width == w && t23_cap_sub[s].height == h)
            return s;
    }

    for (int s = 0; s < T23_SUB_NODES; s++) {
        if (!t23_has_sub[s] || t23_cap_sub[s].streaming)
            continue;
        if (t23_cap_start(&t23_cap_sub[s], t23_sub_names[s], w, h) == EXIT_SUCCESS)
            return s;
        if (allow_main_fallback && t23_cap_main.streaming) {
            HAL_WARNING(tag, "Failed to start %s (%ux%u), falling back to main node\n",
                        t23_sub_names[s], w, h);
            return -1;
        }
        return -2;
    }

    for (int s = 0; s < T23_SUB_NODES; s++) {
        unsigned int dw, dh, diff;
        if (!t23_cap_sub[s].streaming)
            continue;
        dw = t23_cap_sub[s].width > w ? t23_cap_sub[s].width - w : w - t23_cap_sub[s].width;
        dh = t23_cap_sub[s].height > h ? t23_cap_sub[s].height - h : h - t23_cap_sub[s].height;
        diff = dw + dh;
        if (diff < best_diff) {
            best_diff = diff;
            best = s;
        }
    }

    if (allow_main_fallback && t23_cap_main.streaming) {
        unsigned int dw = t23_cap_main.width > w ? t23_cap_main.width - w : w - t23_cap_main.width;
        unsigned int dh = t23_cap_main.height > h ? t23_cap_main.height - h : h - t23_cap_main.height;
        if (dw + dh < best_diff)
            return -1;
    }

    if (best >= 0) {
        HAL_WARNING(tag, "All MSCA scalers busy for %ux%u, sharing %s (%ux%u)\n",
                    w, h, t23_sub_names[best],
                    t23_cap_sub[best].width, t23_cap_sub[best].height);
        return best;
    }

    if (allow_main_fallback && t23_cap_main.streaming)
        return -1;

    return -2;
}

int t23_channel_create(char index, short width, short height, char framerate, char jpeg)
{
    struct t23_venc_chn *c;

    if (index < 0 || index >= T23_VENC_CHN_NUM)
        return EXIT_FAILURE;

    pthread_mutex_lock(&t23_mtx);
    c = &t23_chn[(int)index];
    c->req_w = width > 0 ? (unsigned int)width : t23_sensor_w;
    c->req_h = height > 0 ? (unsigned int)height : t23_sensor_h;
    c->fps = framerate > 0 ? (unsigned int)framerate : 25;

    if ((c->req_w < t23_sensor_w || c->req_h < t23_sensor_h) &&
        (t23_has_sub[0] || t23_has_sub[1])) {
        c->sub_idx = 0;
        t23_align_sub_dims(c->req_w, c->req_h, &c->width, &c->height);
    } else {
        c->sub_idx = -1;
        if (c->req_w != t23_sensor_w || c->req_h != t23_sensor_h) {
            HAL_WARNING("t23_isp", "Channel %d requested %ux%u, using sensor %ux%u on T23 video0\n",
                        index, c->req_w, c->req_h, t23_sensor_w, t23_sensor_h);
        }
        c->width = t23_sensor_w;
        c->height = t23_sensor_h;
    }
    pthread_mutex_unlock(&t23_mtx);
    (void)jpeg;
    return EXIT_SUCCESS;
}

int t23_channel_grayscale(char enable)
{
    if (t23_isp_sd_fd < 0)
        return EXIT_FAILURE;
    if (t23_s_ctrl(t23_isp_sd_fd, V4L2_CID_COLORFX,
                   enable ? V4L2_COLORFX_BW : V4L2_COLORFX_NONE) < 0)
        return EXIT_FAILURE;
    return EXIT_SUCCESS;
}

int t23_channel_unbind(char index)
{
    if (index < 0 || index >= T23_VENC_CHN_NUM)
        return EXIT_FAILURE;
    pthread_mutex_lock(&t23_mtx);
    t23_chn[(int)index].bound = 0;
    pthread_mutex_unlock(&t23_mtx);
    return EXIT_SUCCESS;
}

int t23_config_load(char *path)
{
    (void)path;
    return EXIT_SUCCESS;
}

int t23_pipeline_create(char mirror, char flip, char framerate, char antiflicker)
{
    (void)framerate;
    (void)antiflicker;

    if (t23_isp_sd_fd < 0)
        t23_isp_sd_fd = t23_open_v4l_by_name("v4l-subdev", "t23-isp-core0",
                                             O_RDWR | O_CLOEXEC, NULL, 0);
    if (t23_isp_sd_fd >= 0) {
        t23_s_ctrl(t23_isp_sd_fd, V4L2_CID_HFLIP, mirror ? 1 : 0);
        t23_s_ctrl(t23_isp_sd_fd, V4L2_CID_VFLIP, flip ? 1 : 0);
    }

    if (t23_cap_start(&t23_cap_main, "T23 video0", t23_sensor_w, t23_sensor_h))
        return EXIT_FAILURE;

    t23_pipe_active = true;
    return EXIT_SUCCESS;
}

void t23_pipeline_destroy(void)
{
    t23_pipe_active = false;
    pthread_mutex_lock(&t23_snap_mtx);
    t23_snap_req = 0;
    t23_snap_err = EXIT_FAILURE;
    t23_snap_done = 1;
    t23_snap_node = -1;
    pthread_cond_broadcast(&t23_snap_cond);
    pthread_mutex_unlock(&t23_snap_mtx);

    pthread_mutex_lock(&t23_mtx);
    t23_enc_close(&t23_snap_enc);
    for (int s = 0; s < T23_SUB_NODES; s++)
        t23_cap_close(&t23_cap_sub[s]);
    t23_cap_close(&t23_cap_main);
    free(t23_snap_buf);
    t23_snap_buf = NULL;
    t23_snap_len = 0;
    pthread_mutex_unlock(&t23_mtx);
}

int t23_video_create(char index, hal_vidconfig *config)
{
    struct t23_venc_chn *c;
    unsigned int w, h;
    bool want_sub;

    if (index < 0 || index >= T23_VENC_CHN_NUM || !config)
        return EXIT_FAILURE;

    pthread_mutex_lock(&t23_mtx);
    c = &t23_chn[(int)index];
    want_sub = (c->sub_idx >= 0);
    if (c->fd >= 0)
        t23_enc_close(c);

    if (want_sub) {
        unsigned int rw = c->req_w ? c->req_w : (unsigned int)config->width;
        unsigned int rh = c->req_h ? c->req_h : (unsigned int)config->height;
        int s;

        t23_align_sub_dims(rw, rh, &w, &h);
        s = t23_select_sub_node_locked(w, h, false, "t23_venc");
        if (s < 0) {
            c->sub_idx = -1;
            t23_sub_cleanup_unused_locked();
            pthread_mutex_unlock(&t23_mtx);
            return EXIT_FAILURE;
        }
        c->sub_idx = s;
        w = t23_cap_sub[s].width;
        h = t23_cap_sub[s].height;
    } else {
        c->sub_idx = -1;
        w = t23_cap_main.streaming ? t23_cap_main.width : t23_sensor_w;
        h = t23_cap_main.streaming ? t23_cap_main.height : t23_sensor_h;
    }
    c->width = w;
    c->height = h;

    if (t23_enc_open_m2m(c, w, h, config)) {
        c->sub_idx = -1;
        t23_sub_cleanup_unused_locked();
        pthread_mutex_unlock(&t23_mtx);
        return EXIT_FAILURE;
    }
    t23_sub_cleanup_unused_locked();
    t23_state[(int)index].payload = config->codec;
    t23_state[(int)index].fileDesc = c->fd;
    pthread_mutex_unlock(&t23_mtx);

    HAL_INFO("t23_venc", "Channel %d encoder ready (%ux%u, codec=%d, bps=%u)\n",
             index, c->width, c->height, config->codec, config->bitrate);
    return EXIT_SUCCESS;
}

int t23_video_destroy(char index)
{
    if (index < 0 || index >= T23_VENC_CHN_NUM)
        return EXIT_FAILURE;

    pthread_mutex_lock(&t23_mtx);
    t23_enc_close(&t23_chn[(int)index]);
    t23_chn[(int)index].sub_idx = -1;
    t23_state[(int)index].enable = 0;
    t23_state[(int)index].fileDesc = -1;
    t23_state[(int)index].payload = HAL_VIDCODEC_UNSPEC;

    t23_sub_cleanup_unused_locked();
    pthread_mutex_unlock(&t23_mtx);
    return EXIT_SUCCESS;
}

int t23_video_destroy_all(void)
{
    for (int i = 0; i < T23_VENC_CHN_NUM; i++) {
        if (t23_state[i].enable || t23_chn[i].created)
            t23_video_destroy((char)i);
    }
    return EXIT_SUCCESS;
}

void t23_video_request_idr(char index)
{
    if (index < 0 || index >= T23_VENC_CHN_NUM)
        return;
    __atomic_store_n(&t23_chn[(int)index].idr_req, 1, __ATOMIC_RELEASE);
}

int t23_video_snapshot_grab(short width, short height, char quality, hal_jpegdata *jpeg)
{
    struct timespec ts;
    int snap_node = -1;
    int q = (quality >= 1 && quality <= 100) ? (int)quality : 80;
    int snap_rc = EXIT_SUCCESS;
    unsigned int req_w, req_h;

    if (!jpeg || !t23_pipe_active)
        return EXIT_FAILURE;

    req_w = width > 0 ? (unsigned int)width : t23_sensor_w;
    req_h = height > 0 ? (unsigned int)height : t23_sensor_h;

    pthread_mutex_lock(&t23_mtx);
    if ((req_w < t23_sensor_w || req_h < t23_sensor_h) &&
        (t23_has_sub[0] || t23_has_sub[1])) {
        unsigned int sw, sh;
        t23_align_sub_dims(req_w, req_h, &sw, &sh);
        if (sw < t23_sensor_w || sh < t23_sensor_h) {
            snap_node = t23_select_sub_node_locked(sw, sh, true, "t23_snap");
            if (snap_node == -2) {
                pthread_mutex_unlock(&t23_mtx);
                return EXIT_FAILURE;
            }
            if (snap_node >= 0)
                t23_snap_sub_ref[snap_node]++;
        }
    }
    pthread_mutex_unlock(&t23_mtx);

    pthread_mutex_lock(&t23_snap_mtx);
    clock_gettime(CLOCK_REALTIME, &ts);
    ts.tv_sec += 3;

    t23_snap_node = snap_node;
    t23_snap_qual = q;
    t23_snap_done = 0;
    t23_snap_err = 0;
    t23_snap_req = 1;

    while (!t23_snap_done && keepRunning && t23_pipe_active) {
        int rc = pthread_cond_timedwait(&t23_snap_cond, &t23_snap_mtx, &ts);
        if (rc == ETIMEDOUT) {
            t23_snap_req = 0;
            snap_rc = EXIT_FAILURE;
            goto out_unlock;
        }
    }
    t23_snap_req = 0;

    if (!t23_snap_done || t23_snap_err || !t23_snap_buf || !t23_snap_len) {
        snap_rc = EXIT_FAILURE;
        goto out_unlock;
    }

    if (!jpeg->data)
        jpeg->length = 0;
    if (!jpeg->data || t23_snap_len > jpeg->length) {
        unsigned char *nb = realloc(jpeg->data, t23_snap_len);
        if (!nb) {
            snap_rc = EXIT_FAILURE;
            goto out_unlock;
        }
        jpeg->data = nb;
        jpeg->length = (unsigned int)t23_snap_len;
    }
    memcpy(jpeg->data, t23_snap_buf, t23_snap_len);
    jpeg->jpegSize = (unsigned int)t23_snap_len;

out_unlock:
    pthread_mutex_unlock(&t23_snap_mtx);

    if (snap_node >= 0) {
        pthread_mutex_lock(&t23_mtx);
        if (t23_snap_sub_ref[snap_node] > 0)
            t23_snap_sub_ref[snap_node]--;
        t23_sub_cleanup_unused_locked();
        pthread_mutex_unlock(&t23_mtx);
    }
    return snap_rc;
}

void *t23_video_thread(void)
{
    while (keepRunning) {
        struct pollfd pfds[1 + T23_SUB_NODES];
        struct t23_cap_node *caps[1 + T23_SUB_NODES];
        int node_ids[1 + T23_SUB_NODES];
        int n = 0;

        pthread_mutex_lock(&t23_mtx);
        if (t23_cap_main.streaming && t23_cap_main.fd >= 0) {
            pfds[n].fd = t23_cap_main.fd;
            pfds[n].events = POLLIN;
            pfds[n].revents = 0;
            caps[n] = &t23_cap_main;
            node_ids[n] = -1;
            n++;
        }
        for (int s = 0; s < T23_SUB_NODES; s++) {
            if (t23_cap_sub[s].streaming && t23_cap_sub[s].fd >= 0) {
                pfds[n].fd = t23_cap_sub[s].fd;
                pfds[n].events = POLLIN;
                pfds[n].revents = 0;
                caps[n] = &t23_cap_sub[s];
                node_ids[n] = s;
                n++;
            }
        }
        pthread_mutex_unlock(&t23_mtx);

        if (!n) {
            usleep(20000);
            continue;
        }

        if (poll(pfds, n, 500) <= 0)
            continue;

        for (int i = 0; i < n; i++) {
            if (!(pfds[i].revents & POLLIN))
                continue;
            t23_drain_node(caps[i], node_ids[i], pfds[i].fd);
        }
    }

    HAL_INFO("t23_venc", "Shutting down video encoding thread...\n");
    return NULL;
}

void t23_system_deinit(void)
{
}

int t23_system_init(void)
{
    if (t23_find_helix(t23_helix_path, sizeof(t23_helix_path)) < 0) {
        HAL_DANGER("t23_sys", "Cannot locate Ingenic Helix V4L2 encoder\n");
        return EXIT_FAILURE;
    }

    for (int s = 0; s < T23_SUB_NODES; s++) {
        char tmp_path[64];
        int fd = t23_open_v4l_by_name("video", t23_sub_names[s], O_RDWR | O_CLOEXEC,
                                      tmp_path, sizeof(tmp_path));
        if (fd >= 0) {
            t23_has_sub[s] = true;
            close(fd);
        }
    }

    t23_probe_mc_sensor(sensor, sizeof(sensor), &t23_sensor_w, &t23_sensor_h);
    HAL_INFO("t23_sys", "Sensor='%s' %ux%u, Helix='%s', MSCA='%s/%s'\n",
             sensor, t23_sensor_w, t23_sensor_h, t23_helix_path,
             t23_has_sub[0] ? "scaled0" : "none",
             t23_has_sub[1] ? "scaled0.1" : "none");
    return EXIT_SUCCESS;
}

int t23_system_identify(void)
{
    char compat[256] = {0};
    FILE *file = fopen("/proc/device-tree/compatible", "r");
    size_t len;

    if (!file)
        return 0;
    len = fread(compat, 1, sizeof(compat) - 1, file);
    fclose(file);

    for (size_t i = 0; i < len; i++) {
        if (!compat[i])
            compat[i] = ' ';
    }
    if (!strstr(compat, "ingenic,t23"))
        return 0;

    plat = HAL_PLATFORM_T23;
    strcpy(family, "ingenic t23");
    if (strstr(compat, "jooan,ja-a10"))
        strcpy(chip, "T23DL");
    else
        strcpy(chip, "T23N");

    t23_probe_mc_sensor(sensor, sizeof(sensor), &t23_sensor_w, &t23_sensor_h);
    chnCount = T23_VENC_CHN_NUM;
    chnState = (hal_chnstate *)t23_state;
    aud_thread = t23_audio_thread;
    vid_thread = t23_video_thread;
    isp_thread = NULL;
    return 1;
}

#else

#include <stdlib.h>

void t23_hal_deinit(void) {}
int t23_hal_init(void) { return EXIT_FAILURE; }

void t23_audio_deinit(void) {}
int t23_audio_init(int samplerate) { (void)samplerate; return EXIT_FAILURE; }
void *t23_audio_thread(void) { return NULL; }

int t23_channel_bind(char index) { (void)index; return EXIT_FAILURE; }
int t23_channel_create(char index, short width, short height, char framerate, char jpeg)
{
    (void)index; (void)width; (void)height; (void)framerate; (void)jpeg;
    return EXIT_FAILURE;
}
int t23_channel_grayscale(char enable) { (void)enable; return EXIT_FAILURE; }
int t23_channel_unbind(char index) { (void)index; return EXIT_FAILURE; }

int t23_config_load(char *path) { (void)path; return EXIT_FAILURE; }

int t23_pipeline_create(char mirror, char flip, char framerate, char antiflicker)
{
    (void)mirror; (void)flip; (void)framerate; (void)antiflicker;
    return EXIT_FAILURE;
}
void t23_pipeline_destroy(void) {}

int t23_video_create(char index, hal_vidconfig *config)
{
    (void)index; (void)config;
    return EXIT_FAILURE;
}
int t23_video_destroy(char index) { (void)index; return EXIT_FAILURE; }
int t23_video_destroy_all(void) { return EXIT_FAILURE; }
void t23_video_request_idr(char index) { (void)index; }
int t23_video_snapshot_grab(short width, short height, char quality, hal_jpegdata *jpeg)
{
    (void)width; (void)height; (void)quality; (void)jpeg;
    return EXIT_FAILURE;
}
void *t23_video_thread(void) { return NULL; }

void t23_system_deinit(void) {}
int t23_system_init(void) { return EXIT_FAILURE; }

int t23_system_identify(void) { return 0; }

#endif
