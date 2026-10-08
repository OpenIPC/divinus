#pragma once

#include "fh_common.h"

/* Channel creation: which encoders may later be selected, and the maximum frame size */
typedef struct {
    unsigned int types;     /* OR of fh_venc_type */
    unsigned int width;
    unsigned int height;
} fh_venc_cfg;

/*
 * Rate control block, also the payload of FH_VENC_SetRCAttr().
 * Layouts (vendor application defaults):
 *   H264/H265 CBR: { mode, 35, bitrate_bps, minQp(28), maxQp(50), targetQp, 50, fps | den << 16, 120, 0, 0, 5, 1, 0 }
 *   H264/H265 VBR: { mode, 35, bitrate_bps, fps | den << 16, 120, 0, 0, 5, 1 }
 *   H264 fixed QP: { mode, iQp, pQp, fps | den << 16 }
 */
typedef struct {
    unsigned int mode;      /* fh_venc_rcmode */
    unsigned int param[21];
} fh_venc_rc;

/* FH_VENC_SetChnAttr() payload, 0xac bytes */
typedef struct {
    unsigned int type;      /* single fh_venc_type; 0 destroys the encoder */
    unsigned int profile;   /* H264: 66 baseline, 77 main, 100 high; H265: 1 */
    unsigned int gop;
    unsigned int width;
    unsigned int height;
    unsigned int reserved[16];
    fh_venc_rc rc;
} fh_venc_attr;

/* JPEG snapshot encoder attributes share the same 0xac-byte buffer */
typedef struct {
    unsigned int type;      /* FH_VENC_TYPE_JPEG */
    unsigned int quality;   /* 1..99 */
    unsigned int reserved;
    unsigned int rateIndex; /* 0..9, vendor uses 4 */
    unsigned int pad[39];
} fh_venc_jpgattr;

typedef struct {
    unsigned int type;      /* H264: 7 SPS, 8 PPS, 1 slice (IDR too, check frame type) */
    unsigned int length;
    unsigned int addr;      /* user virtual address, starts with the 4-byte start code */
} fh_venc_nalu;

/* Output of FH_VENC_GetStream[_Block](), 0x174 bytes. The first word is the type mask on input. */
typedef struct {
    unsigned int type;      /* fh_venc_type of the returned frame */
    unsigned int reserved1;
    unsigned int channel;
    unsigned int base;      /* JPEG: data address */
    unsigned int frameType; /* 2 = I frame, 0 = P frame; JPEG: data length */
    unsigned int length;    /* total length of the frame */
    unsigned int ptsLow;    /* microseconds */
    unsigned int ptsHigh;
    unsigned int naluCount;
    fh_venc_nalu nalu[28];
} fh_venc_strm;

typedef struct {
    void *handle;

    int (*fnCreateChannel)(unsigned int channel, fh_venc_cfg *config);
    int (*fnGetStream)(unsigned int typeMask, fh_venc_strm *stream);
    int (*fnGetStreamBlocking)(unsigned int typeMask, fh_venc_strm *stream);
    int (*fnReleaseStream)(unsigned int channel);
    int (*fnRequestIdr)(unsigned int channel);
    int (*fnSetChannelConfig)(unsigned int channel, void *config);
    int (*fnSetRateControl)(unsigned int channel, fh_venc_rc *rc);
    int (*fnStartReceiving)(unsigned int channel);
    int (*fnStopReceiving)(unsigned int channel);
} fh_venc_impl;

static int fh_venc_load(fh_venc_impl *venc_lib) {
    if (!(venc_lib->handle = dlopen("libdsp.so", RTLD_NOW | RTLD_GLOBAL)))
        HAL_ERROR("fh_venc", "Failed to load library!\nError: %s\n", dlerror());

    if (!(venc_lib->fnCreateChannel = (int(*)(unsigned int channel, fh_venc_cfg *config))
        hal_symbol_load("fh_venc", venc_lib->handle, "FH_VENC_CreateChn")))
        return EXIT_FAILURE;

    if (!(venc_lib->fnGetStream = (int(*)(unsigned int typeMask, fh_venc_strm *stream))
        hal_symbol_load("fh_venc", venc_lib->handle, "FH_VENC_GetStream")))
        return EXIT_FAILURE;

    if (!(venc_lib->fnGetStreamBlocking = (int(*)(unsigned int typeMask, fh_venc_strm *stream))
        hal_symbol_load("fh_venc", venc_lib->handle, "FH_VENC_GetStream_Block")))
        return EXIT_FAILURE;

    if (!(venc_lib->fnReleaseStream = (int(*)(unsigned int channel))
        hal_symbol_load("fh_venc", venc_lib->handle, "FH_VENC_ReleaseStream")))
        return EXIT_FAILURE;

    if (!(venc_lib->fnRequestIdr = (int(*)(unsigned int channel))
        hal_symbol_load("fh_venc", venc_lib->handle, "FH_VENC_RequestIDR")))
        return EXIT_FAILURE;

    if (!(venc_lib->fnSetChannelConfig = (int(*)(unsigned int channel, void *config))
        hal_symbol_load("fh_venc", venc_lib->handle, "FH_VENC_SetChnAttr")))
        return EXIT_FAILURE;

    if (!(venc_lib->fnSetRateControl = (int(*)(unsigned int channel, fh_venc_rc *rc))
        hal_symbol_load("fh_venc", venc_lib->handle, "FH_VENC_SetRCAttr")))
        return EXIT_FAILURE;

    if (!(venc_lib->fnStartReceiving = (int(*)(unsigned int channel))
        hal_symbol_load("fh_venc", venc_lib->handle, "FH_VENC_StartRecvPic")))
        return EXIT_FAILURE;

    if (!(venc_lib->fnStopReceiving = (int(*)(unsigned int channel))
        hal_symbol_load("fh_venc", venc_lib->handle, "FH_VENC_StopRecvPic")))
        return EXIT_FAILURE;

    return EXIT_SUCCESS;
}

static void fh_venc_unload(fh_venc_impl *venc_lib) {
    if (venc_lib->handle) dlclose(venc_lib->handle);
    venc_lib->handle = NULL;
    memset(venc_lib, 0, sizeof(*venc_lib));
}
