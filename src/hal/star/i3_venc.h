#pragma once

#include "i3_common.h"

#define I3_VENC_CHN_NUM 3

typedef enum {
    I3_VENC_CODEC_H264,
    I3_VENC_CODEC_H265,
    I3_VENC_CODEC_MJPEG,
    I3_VENC_CODEC_JPEG,
    I3_VENC_CODEC_END
} i3_venc_codec;

typedef enum {
    I3_VENC_NALU_H26X_PSLICE,
    I3_VENC_NALU_H26X_BSLICE,
    I3_VENC_NALU_H26X_ISLICE,
    I3_VENC_NALU_H26X_IPSLICE,
    I3_VENC_NALU_H26X_END
} i3_venc_nalu_h26x;

typedef enum {
    I3_VENC_RATE_H26X_OFF,
    I3_VENC_RATE_H26X_CBR,
    I3_VENC_RATE_H26X_VBR,
    I3_VENC_RATE_H26X_QP,
    I3_VENC_RATE_H26X_END
} i3_venc_rate_h26x;

typedef struct {
    // Accepts values from 0 to 2 (BP, MP, HP)
    // N.B. H.265 only supports 0 (BP)
    unsigned int profile;
    int reserved[1];
} i3_venc_attr_h26x;

typedef struct {
    unsigned int quality;
    int snapOn;
} i3_venc_attr_mjpg;

typedef struct {
    unsigned int quality;
    int reserved[1];
} i3_venc_attr_jpg;

typedef struct {
    i3_venc_codec codec;
    union {
        i3_venc_attr_h26x h264;
        i3_venc_attr_h26x h265;
        i3_venc_attr_mjpg mjpg;
        i3_venc_attr_jpg jpg;
    };
    i3_common_dim capt;
    i3_common_pixfmt pixFmt;
    unsigned int dstFps;
    i3_common_mode mode;
    int reserved[1];
} i3_venc_attrib;

typedef struct {
    unsigned int gop;
    unsigned int statTime;
    unsigned int bitrate;
    unsigned int avgLvl;
    unsigned int maxQual;
    unsigned int minQual;
    int ipQpDelta;
    int reserved[4];
} i3_venc_rate_h26xcbr;

typedef struct {
    unsigned int gop;
    unsigned int statTime;
    unsigned int maxBitrate;
    unsigned int maxIQual;
    unsigned int minIQual;
    unsigned int maxPQual;
    unsigned int minPQual;
    int ipQpDelta;
    int chgPos;
} i3_venc_rate_h26xvbr;

typedef struct {
    unsigned int gop;
    unsigned int interQual;
    unsigned int predQual;
} i3_venc_rate_h26xqp;

typedef struct {
    i3_venc_rate_h26x mode;
    union {
        i3_venc_rate_h26xcbr h26xCbr;
        i3_venc_rate_h26xvbr h26xVbr;
        i3_venc_rate_h26xqp h26xQp;
    };
    unsigned int rowQpDelta;
    void *extend;
    unsigned int rcVersion;
} i3_venc_rate;

typedef struct {
    i3_venc_attrib attrib;
    i3_venc_rate rate;
} i3_venc_chn;

typedef struct {
    unsigned int quality;
    unsigned char qtLuma[64];
    unsigned char qtChroma[64];
    int reserved[1];
} i3_venc_jpg;

typedef struct {
    unsigned char *data;
    unsigned int length;
    unsigned long long timestamp;
    i3_venc_nalu_h26x naluType;
    unsigned int offset;
} i3_venc_pack;

typedef struct {
    i3_venc_pack *packet;
    unsigned int data1;
    unsigned int data2;
    unsigned int count;
    int toDispose;
    i3_venc_codec codec;
} i3_venc_strm;

typedef struct {
    void *handle;

    int (*fnCreateChannel)(int channel, i3_venc_chn *config);
    int (*fnDestroyChannel)(int channel);
    int (*fnGetChannelConfig)(int channel, i3_venc_chn *config);
    int (*fnSetChannelConfig)(int channel, i3_venc_chn *config);

    int (*fnGetDescriptor)(int channel);

    int (*fnGetJpegParam)(int channel, i3_venc_jpg *param);
    int (*fnSetJpegParam)(int channel, i3_venc_jpg *param);

    int (*fnFreeStream)(int channel, i3_venc_strm *stream);
    int (*fnGetStream)(int channel, i3_venc_strm *stream, unsigned int timeout);

    int (*fnRequestIdr)(int channel, char instant);
    int (*fnStartReceiving)(int channel);
    int (*fnStopReceiving)(int channel);
} i3_venc_impl;

static int i3_venc_load(i3_venc_impl *venc_lib) {
    if (!(venc_lib->handle = dlopen("libmi.so", RTLD_LAZY | RTLD_GLOBAL)))
        HAL_ERROR("i3_venc", "Failed to load library!\nError: %s\n", dlerror());

    if (!(venc_lib->fnCreateChannel = (int(*)(int channel, i3_venc_chn *config))
        hal_symbol_load("i3_venc", venc_lib->handle, "MI_VENC_CreateChn")))
        return EXIT_FAILURE;

    if (!(venc_lib->fnDestroyChannel = (int(*)(int channel))
        hal_symbol_load("i3_venc", venc_lib->handle, "MI_VENC_DestroyChn")))
        return EXIT_FAILURE;

    if (!(venc_lib->fnGetChannelConfig = (int(*)(int channel, i3_venc_chn *config))
        hal_symbol_load("i3_venc", venc_lib->handle, "MI_VENC_GetChnAttr")))
        return EXIT_FAILURE;

    if (!(venc_lib->fnSetChannelConfig = (int(*)(int channel, i3_venc_chn *config))
        hal_symbol_load("i3_venc", venc_lib->handle, "MI_VENC_SetChnAttr")))
        return EXIT_FAILURE;

    if (!(venc_lib->fnGetDescriptor = (int(*)(int channel))
        hal_symbol_load("i3_venc", venc_lib->handle, "MI_VENC_GetFd")))
        return EXIT_FAILURE;

    if (!(venc_lib->fnGetJpegParam = (int(*)(int channel, i3_venc_jpg *param))
        hal_symbol_load("i3_venc", venc_lib->handle, "MI_VENC_GetJpegParam")))
        return EXIT_FAILURE;

    if (!(venc_lib->fnSetJpegParam = (int(*)(int channel, i3_venc_jpg *param))
        hal_symbol_load("i3_venc", venc_lib->handle, "MI_VENC_SetJpegParam")))
        return EXIT_FAILURE;

    if (!(venc_lib->fnFreeStream = (int(*)(int channel, i3_venc_strm *stream))
        hal_symbol_load("i3_venc", venc_lib->handle, "MI_VENC_ReleaseStream")))
        return EXIT_FAILURE;

    if (!(venc_lib->fnGetStream = (int(*)(int channel, i3_venc_strm *stream, unsigned int timeout))
        hal_symbol_load("i3_venc", venc_lib->handle, "MI_VENC_GetStreamTimeout")))
        return EXIT_FAILURE;

    if (!(venc_lib->fnRequestIdr = (int(*)(int channel, char instant))
        hal_symbol_load("i3_venc", venc_lib->handle, "MI_VENC_RequestIDR")))
        return EXIT_FAILURE;

    if (!(venc_lib->fnStartReceiving = (int(*)(int channel))
        hal_symbol_load("i3_venc", venc_lib->handle, "MI_VENC_StartRecvPic")))
        return EXIT_FAILURE;

    if (!(venc_lib->fnStopReceiving = (int(*)(int channel))
        hal_symbol_load("i3_venc", venc_lib->handle, "MI_VENC_StopRecvPic")))
        return EXIT_FAILURE;

    return EXIT_SUCCESS;
}

static void i3_venc_unload(i3_venc_impl *venc_lib) {
    if (venc_lib->handle) dlclose(venc_lib->handle);
    venc_lib->handle = NULL;
    memset(venc_lib, 0, sizeof(*venc_lib));
}