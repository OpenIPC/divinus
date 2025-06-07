#pragma once

#include "ak_common.h"

typedef enum {
    AK_VENC_CODEC_H264,
    AK_VENC_CODEC_MJPG,
    AK_VENC_CODEC_H265
} ak_venc_codec;

typedef enum {
    AK_VENC_NALU_PSLICE,
    AK_VENC_NALU_ISLICE,
    AK_VENC_NALU_BSLICE,
    AK_VENC_NALU_PISLICE
} ak_venc_nalu;

typedef enum {
    AK_VENC_OUT_RECORD,
    AK_VENC_OUT_MAINSTRM,
    AK_VENC_OUT_SUBSTRM,
    AK_VENC_OUT_SNAP,
    AK_VENC_OUT_END
} ak_venc_out;

typedef enum {
    AK_VENC_PROF_MAIN,
    AK_VENC_PROF_HIGH,
    AK_VENC_PROF_BASE,
    AK_VENC_PROF_HEVC_MAIN,
    AK_VENC_PROF_HEVC_MAINSTILL,
    AK_VENC_PROF_END
} ak_venc_prof;

typedef struct {
    unsigned long width;
    unsigned long height;
    long minQual;
    long maxQual;
    long dstFps;
    long gop;
    long maxBitrate;
    ak_venc_prof profile;
    int subChnOn;
    ak_venc_out output;
    int vbrModeOn;
    ak_venc_codec codec;
} ak_venc_cnf;

typedef struct {
    unsigned char *data;
    unsigned int length;
    unsigned long long timestamp;
    unsigned long sequence;
    ak_venc_nalu naluType;
} ak_venc_strm;

typedef struct {
    void *handle, *handleStrmEnc;

    void* (*fnBindChannel)(void *input, void *output);
    int   (*fnDisableChannel)(void *channel);
    void* (*fnEnableChannel)(ak_venc_cnf *config);
    int   (*fnUnbindChannel)(void *bind);

    int   (*fnFreeStream)(void *bind, ak_venc_strm *stream);
    int   (*fnGetStream)(void *bind, ak_venc_strm *stream);

    int   (*fnRequestIdr)(void *channel);
} ak_venc_impl;

static int ak_venc_load(ak_venc_impl *venc_lib) {
    if (!(venc_lib->handleStrmEnc = dlopen("libakstreamenc.so", RTLD_LAZY | RTLD_GLOBAL)))
        HAL_ERROR("ak_venc", "Failed to load library!\nError: %s\n", dlerror());

    if (!(venc_lib->handle = dlopen("libmpi_venc.so", RTLD_LAZY | RTLD_GLOBAL)))
        HAL_ERROR("ak_venc", "Failed to load library!\nError: %s\n", dlerror());

    if (!(venc_lib->fnBindChannel = (void*(*)(void *input, void *output))
        hal_symbol_load("ak_venc", venc_lib->handle, "ak_venc_request_stream")))
        return EXIT_FAILURE;

    if (!(venc_lib->fnDisableChannel = (int(*)(void* channel))
        hal_symbol_load("ak_venc", venc_lib->handle, "ak_venc_close")))
        return EXIT_FAILURE;

    if (!(venc_lib->fnEnableChannel = (void*(*)(ak_venc_cnf *config))
        hal_symbol_load("ak_venc", venc_lib->handle, "ak_venc_open")))
        return EXIT_FAILURE;

    if (!(venc_lib->fnUnbindChannel = (int(*)(void *bind))
        hal_symbol_load("ak_venc", venc_lib->handle, "ak_venc_cancel_stream")))
        return EXIT_FAILURE;

    if (!(venc_lib->fnFreeStream = (int(*)(void *bind, ak_venc_strm *stream))
        hal_symbol_load("ak_venc", venc_lib->handle, "ak_venc_release_stream")))
        return EXIT_FAILURE;

    if (!(venc_lib->fnGetStream = (int(*)(void *bind, ak_venc_strm *stream))
        hal_symbol_load("ak_venc", venc_lib->handle, "ak_venc_get_stream")))
        return EXIT_FAILURE;

    if (!(venc_lib->fnRequestIdr = (int(*)(void *channel))
        hal_symbol_load("ak_venc", venc_lib->handle, "ak_venc_set_iframe")))
        return EXIT_FAILURE;

    return EXIT_SUCCESS;
}

static void ak_venc_unload(ak_venc_impl *venc_lib) {
    if (venc_lib->handle) dlclose(venc_lib->handle);
    venc_lib->handle = NULL;
    if (venc_lib->handleStrmEnc) dlclose(venc_lib->handleStrmEnc);
    venc_lib->handleStrmEnc = NULL;
    memset(venc_lib, 0, sizeof(*venc_lib));
}