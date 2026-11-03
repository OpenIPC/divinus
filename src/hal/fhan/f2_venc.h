#pragma once

#include "f2_common.h"

/* Channel creation: which encoders may later be selected, and the maximum frame
 * size — mirrors FH_VENC_CHN_CAP { support_type, max_size, bFrameNum }. */
typedef struct {
    unsigned int types;
    unsigned int width;     /* max_size.u32Width */
    unsigned int height;    /* max_size.u32Height */
    unsigned int bFrameNum;
} f2_venc_cap;

/*
 * Rate control block, also the payload of FH_VENC_SetRCAttr().
 * Layouts (vendor application defaults):
 *   H264/H265 CBR: { mode, 35, bitrate_bps, fps | den << 16, maxQp(50), targetQp, 50, fps | den << 16, 120, 0, 0, 5, 1, 0 }
 *   H264/H265 VBR: { mode, 35, bitrate_bps, fps | den << 16, 120, 0, 0, 5, 1 }
 *   H264 fixed QP: { mode, iQp, pQp, fps | den << 16 }
 */
typedef struct {
    unsigned int mode;
    unsigned int param[21];
} f2_venc_rc;

/* FH_VENC_SetChnAttr() payload, 0xac bytes */
typedef struct {
    unsigned int codec;     /* single f2_venc_codec; 0 destroys the encoder */
    unsigned int profile;   /* H264: 66 baseline, 77 main, 100 high; H265: 1 */
    unsigned int gop;
    unsigned int width;
    unsigned int height;
    unsigned int reserved[16];
    f2_venc_rc rate;
} f2_venc_attr;

/* JPEG snapshot encoder attributes share the same 0xac-byte buffer */
typedef struct {
    unsigned int codec;
    unsigned int quality;   /* 1..99 */
    unsigned int reserved;
    unsigned int rateIndex; /* 0..9, vendor uses 4 */
    unsigned int reserved2[39];
} f2_venc_jpgattr;

/*
 * MJPEG streaming encoder attributes (FH_VENC_CHN_CONFIG, MJPEG variant).
 * Layout from the vendor SDK header (FH_VENC_MJPEG_ATTR) and sample, confirmed
 * at runtime by the driver trace mjpeg_set_chn_cfg(): [INVALID_PARAM]Rotate.
 *   +0x00 codec (must be F2_VENC_CODEC_MJPEG)
 *   +0x04 pic width       +0x08 pic height
 *   +0x0c rotate (0..3)   +0x10 encSpeed (0..9)
 *   +0x54 mode (0 = FH_RC_MJPEG_FIXQP)
 *   +0x58 qp
 *   +0x5c frameNum (u16)     +0x5e frameTime (u16)
 */
typedef struct {
    unsigned int codec;
    unsigned int width;
    unsigned int height;
    unsigned int rotate;        /* 0x0c, 0..3 */
    unsigned int encSpeed;  /* 0x10, 0..9 */
    unsigned int reserved[16];      /* 0x14..0x53 */
    unsigned int mode;       /* 0x54, 0 = MJPEG fixed QP */
    unsigned int qp;
    unsigned short frameNum;
    unsigned short frameTime;
    unsigned int reserved2[40];      /* up to the 0xac buffer size */
} f2_venc_mjpegattr;

/* Per-NALU entry as filled by the V200 encoder (12 bytes): {type, length, addr}.
 * Confirmed at runtime: SPS=7/len 29, PPS=8/len 8, slice=1. */
typedef struct {
    unsigned int type;
    unsigned int length;
    unsigned int addr;
} f2_venc_nalu;

/* Output of FH_VENC_GetStream[_Block]() — V200 layout (libdsp _VENC_HandleStream):
 *   +0x00 type, +0x04, +0x08, +0x0c, +0x10, +0x14, +0x18, +0x1c naluCnt,
 *   +0x20 NALU array (12 bytes each), and a record at +0x110..+0x13c. */
typedef struct {
    unsigned int type;
    unsigned int field04;
    unsigned int field08;
    unsigned int field0c;
    unsigned int field10;
    unsigned int field14;
    unsigned int field18;
    unsigned int naluCnt;
    f2_venc_nalu nalu[40];
    unsigned int tail[48];
} f2_venc_strm;

typedef struct {
    void *handle;

    int (*fnCreateChannel)(unsigned int channel, f2_venc_cap *config);
    int (*fnGetStream)(unsigned int typeMask, f2_venc_strm *stream);
    int (*fnGetStreamBlocking)(unsigned int typeMask, f2_venc_strm *stream);
    int (*fnFreeStream)(unsigned int channel);
    int (*fnRequestIdr)(unsigned int channel);
    int (*fnSetChannelConfig)(unsigned int channel, void *config);
    int (*fnSetRateControl)(unsigned int channel, f2_venc_rc *rc);
    int (*fnStartReceiving)(unsigned int channel);
    int (*fnStopReceiving)(unsigned int channel);
} f2_venc_impl;

static int f2_venc_load(f2_venc_impl *venc_lib) {
    if (!(venc_lib->handle = dlopen("libdsp.so", RTLD_NOW | RTLD_GLOBAL)))
        HAL_ERROR("f2_venc", "Failed to load library!\nError: %s\n", dlerror());

    if (!(venc_lib->fnCreateChannel = (int(*)(unsigned int, f2_venc_cap *))
        hal_symbol_load("f2_venc", venc_lib->handle, "FH_VENC_CreateChn")))
        return EXIT_FAILURE;

    if (!(venc_lib->fnGetStream = (int(*)(unsigned int, f2_venc_strm *))
        hal_symbol_load("f2_venc", venc_lib->handle, "FH_VENC_GetStream")))
        return EXIT_FAILURE;

    if (!(venc_lib->fnGetStreamBlocking = (int(*)(unsigned int, f2_venc_strm *))
        hal_symbol_load("f2_venc", venc_lib->handle, "FH_VENC_GetStream_Block")))
        return EXIT_FAILURE;

    if (!(venc_lib->fnFreeStream = (int(*)(unsigned int))
        hal_symbol_load("f2_venc", venc_lib->handle, "FH_VENC_ReleaseStream")))
        return EXIT_FAILURE;

    if (!(venc_lib->fnRequestIdr = (int(*)(unsigned int))
        hal_symbol_load("f2_venc", venc_lib->handle, "FH_VENC_RequestIDR")))
        return EXIT_FAILURE;

    if (!(venc_lib->fnSetChannelConfig = (int(*)(unsigned int, void *))
        hal_symbol_load("f2_venc", venc_lib->handle, "FH_VENC_SetChnAttr")))
        return EXIT_FAILURE;

    if (!(venc_lib->fnSetRateControl = (int(*)(unsigned int, f2_venc_rc *))
        hal_symbol_load("f2_venc", venc_lib->handle, "FH_VENC_SetRCAttr")))
        return EXIT_FAILURE;

    if (!(venc_lib->fnStartReceiving = (int(*)(unsigned int))
        hal_symbol_load("f2_venc", venc_lib->handle, "FH_VENC_StartRecvPic")))
        return EXIT_FAILURE;

    if (!(venc_lib->fnStopReceiving = (int(*)(unsigned int))
        hal_symbol_load("f2_venc", venc_lib->handle, "FH_VENC_StopRecvPic")))
        return EXIT_FAILURE;

    return EXIT_SUCCESS;
}

static void f2_venc_unload(f2_venc_impl *venc_lib) {
    if (venc_lib->handle) dlclose(venc_lib->handle);
    venc_lib->handle = NULL;
    memset(venc_lib, 0, sizeof(*venc_lib));
}
