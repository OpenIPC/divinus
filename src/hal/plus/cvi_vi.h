#pragma once

#include "cvi_common.h"

typedef enum {
    CVI_VI_INTF_BT656,
    CVI_VI_INTF_BT601,
    CVI_VI_INTF_DIGITAL_CAMERA,
    CVI_VI_INTF_BT1120_STANDARD,
    CVI_VI_INTF_BT1120_INTERLEAVED,
    CVI_VI_INTF_MIPI,
    CVI_VI_INTF_MIPI_YUV420_NORMAL,
    CVI_VI_INTF_MIPI_YUV420_LEGACY,
    CVI_VI_INTF_MIPI_YUV422,
    CVI_VI_INTF_LVDS,
    CVI_VI_INTF_HISPI,
    CVI_VI_INTF_SLVS,
    CVI_VI_INTF_END
} cvi_vi_intf;

typedef enum {
    CVI_VI_SEQ_VUVU,
    CVI_VI_SEQ_UVUV,
    CVI_VI_SEQ_UYVY,
    CVI_VI_SEQ_VYUY,
    CVI_VI_SEQ_YUYV,
    CVI_VI_SEQ_YVYU,
    CVI_VI_SEQ_END
} cvi_vi_seq;

typedef enum {
    CVI_VI_WORK_1MULTIPLEX,
    CVI_VI_WORK_2MULTIPLEX,
    CVI_VI_WORK_3MULTIPLEX,
    CVI_VI_WORK_4MULTIPLEX
} cvi_vi_work;

typedef struct {
    unsigned int num;
    int pipeId[CVI_VI_PIPE_NUM];
} cvi_vi_bind;

typedef struct {
    cvi_common_dim size;
    cvi_common_pixfmt pixFmt;
    cvi_common_hdr dynRange;
    int reserved;
    cvi_common_compr compress;
    char mirror;
    char flip;
    unsigned int depth;
    int srcFps;
    int dstFps;
    unsigned int bindVbPool;
} cvi_vi_chn;

typedef struct {
    // Accepts values from 0-2 (no, front, back)
    int bypass;
    char yuvSkipOn;
    char ispBypassOn;
    cvi_common_dim maxSize;
    cvi_common_pixfmt pixFmt;
    cvi_common_compr compress;
    cvi_common_prec prec;
    char nRedOn;
    char sharpenOn;
    int srcFps;
    int dstFps;
    char discProPicOn;
    char yuvBypassOn;
} cvi_vi_pipe;

typedef struct {
    unsigned int hsyncFront;
    unsigned int hsyncWidth;
    unsigned int hsyncBack;
    unsigned int vsyncFront;
    unsigned int vsyncWidth;
    unsigned int vsyncBack;
    // Next three are valid on interlace mode
    // and define even-frame timings
    unsigned int vsyncIntrlFront;
    unsigned int vsyncIntrlWidth;
    unsigned int vsyncIntrlBack;
} cvi_vi_timing;

typedef struct {
    int vsyncPulse;
    int vsyncInv;
    int hsyncPulse;
    int hsyncInv;
    int vsyncValid;
    int vsyncValidInv;
    cvi_vi_timing timing;
} cvi_vi_sync;

typedef struct {
    cvi_vi_intf intf;
    cvi_vi_work work;
    int progressiveOn;
    int adChn[4];
    cvi_vi_seq seq;
    cvi_vi_sync sync;
    // Accepts values from 0-2 (yuv, rgb, early yuv)
    int rgbMode;
    cvi_common_dim size;
    cvi_common_wdr wdrMode;
    unsigned int wdrCacheLine;
    char wdrSynthOn;
    cvi_common_bayer bayerMode;
    unsigned int chnNum;
    unsigned int snrFps;
} cvi_vi_dev;

typedef struct {
    cvi_common_wdr mode;
    unsigned int cacheLine;
    char synthWdrOn;
} cvi_vi_wdr;

typedef struct {
    void *handle;

    int (*fnDisableDevice)(int device);
    int (*fnEnableDevice)(int device);
    int (*fnSetDeviceConfig)(int device, cvi_vi_dev *config);
    int (*fnSetDeviceNumber)(int number);

    int (*fnDisableChannel)(int pipe, int channel);
    int (*fnEnableChannel)(int pipe, int channel);
    int (*fnSetChannelConfig)(int pipe, int channel, cvi_vi_chn *config);
    int (*fnAttachChannelPool)(int pipe, int channel, unsigned int pool);
    int (*fnDetachChannelPool)(int pipe, int channel);

    int (*fnBindPipe)(int device, cvi_vi_bind *config);
    int (*fnCreatePipe)(int pipe, cvi_vi_pipe *config);
    int (*fnDestroyPipe)(int pipe);
    int (*fnStartPipe)(int pipe);
    int (*fnStopPipe)(int pipe);
} cvi_vi_impl;

static int cvi_vi_load(cvi_vi_impl *vi_lib) {
    if (!(vi_lib->handle = dlopen("libvpu.so", RTLD_LAZY | RTLD_GLOBAL)))
        HAL_ERROR("cvi_vi", "Failed to load library!\nError: %s\n", dlerror());

    if (!(vi_lib->fnDisableDevice = (int(*)(int device))
        hal_symbol_load("cvi_vi", vi_lib->handle, "CVI_VI_DisableDev")))
        return EXIT_FAILURE;

    if (!(vi_lib->fnEnableDevice = (int(*)(int device))
        hal_symbol_load("cvi_vi", vi_lib->handle, "CVI_VI_EnableDev")))
        return EXIT_FAILURE;

    if (!(vi_lib->fnSetDeviceConfig = (int(*)(int device, cvi_vi_dev *config))
        hal_symbol_load("cvi_vi", vi_lib->handle, "CVI_VI_SetDevAttr")))
        return EXIT_FAILURE;

    if (!(vi_lib->fnSetDeviceNumber = (int(*)(int number))
        hal_symbol_load("cvi_vi", vi_lib->handle, "CVI_VI_SetDevNum")))
        return EXIT_FAILURE;

    if (!(vi_lib->fnDisableChannel = (int(*)(int pipe, int channel))
        hal_symbol_load("cvi_vi", vi_lib->handle, "CVI_VI_DisableChn")))
        return EXIT_FAILURE;

    if (!(vi_lib->fnEnableChannel = (int(*)(int pipe, int channel))
        hal_symbol_load("cvi_vi", vi_lib->handle, "CVI_VI_EnableChn")))
        return EXIT_FAILURE;

    if (!(vi_lib->fnSetChannelConfig = (int(*)(int pipe, int channel, cvi_vi_chn *config))
        hal_symbol_load("cvi_vi", vi_lib->handle, "CVI_VI_SetChnAttr")))
        return EXIT_FAILURE;

    if (!(vi_lib->fnAttachChannelPool = (int(*)(int pipe, int channel, unsigned int pool))
        hal_symbol_load("cvi_vi", vi_lib->handle, "CVI_VI_AttachVbPool")))
        return EXIT_FAILURE;

    if (!(vi_lib->fnDetachChannelPool = (int(*)(int pipe, int channel))
        hal_symbol_load("cvi_vi", vi_lib->handle, "CVI_VI_DetachVbPool")))
        return EXIT_FAILURE;

    if (!(vi_lib->fnBindPipe = (int(*)(int device, cvi_vi_bind *config))
        hal_symbol_load("cvi_vi", vi_lib->handle, "CVI_VI_SetDevBindPipe")))
        return EXIT_FAILURE;

    if (!(vi_lib->fnCreatePipe = (int(*)(int pipe, cvi_vi_pipe *config))
        hal_symbol_load("cvi_vi", vi_lib->handle, "CVI_VI_CreatePipe")))
        return EXIT_FAILURE;

    if (!(vi_lib->fnDestroyPipe = (int(*)(int pipe))
        hal_symbol_load("cvi_vi", vi_lib->handle, "CVI_VI_DestroyPipe")))
        return EXIT_FAILURE;

    if (!(vi_lib->fnStartPipe = (int(*)(int pipe))
        hal_symbol_load("cvi_vi", vi_lib->handle, "CVI_VI_StartPipe")))
        return EXIT_FAILURE;

    if (!(vi_lib->fnStopPipe = (int(*)(int pipe))
        hal_symbol_load("cvi_vi", vi_lib->handle, "CVI_VI_StopPipe")))
        return EXIT_FAILURE;

    return EXIT_SUCCESS;
}

static void cvi_vi_unload(cvi_vi_impl *vi_lib) {
    if (vi_lib->handle) dlclose(vi_lib->handle);
    vi_lib->handle = NULL;
    memset(vi_lib, 0, sizeof(*vi_lib));
}