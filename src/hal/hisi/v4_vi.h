#pragma once

#include "v4_common.h"

typedef enum {
    V4_VI_INTF_BT656,
    V4_VI_INTF_BT656_PACKED_YUV,
    V4_VI_INTF_BT601,
    V4_VI_INTF_DIGITAL_CAMERA,
    V4_VI_INTF_BT1120_STANDARD,
    V4_VI_INTF_BT1120_INTERLEAVED,
    V4_VI_INTF_MIPI,
    V4_VI_INTF_MIPI_YUV420_NORMAL,
    V4_VI_INTF_MIPI_YUV420_LEGACY,
    V4_VI_INTF_MIPI_YUV422,
    V4_VI_INTF_LVDS,
    V4_VI_INTF_HISPI,
    V4_VI_INTF_SLVS,
    V4_VI_INTF_END
} v4_vi_intf;

typedef enum {
    V4_VI_REPHASE_NONE,
    V4_VI_REPHASE_SKIP1_2,
    V4_VI_REPHASE_SKIP1_3,
    V4_VI_REPHASE_BINNING1_2,
    V4_VI_REPHASE_BINNING1_3,
    V4_VI_REPHASE_END
} v4_vi_rephase;

typedef enum {
    V4_VI_SEQ_VUVU,
    V4_VI_SEQ_UVUV,
    V4_VI_SEQ_UYVY,
    V4_VI_SEQ_VYUY,
    V4_VI_SEQ_YUYV,
    V4_VI_SEQ_YVYU,
    V4_VI_SEQ_END
} v4_vi_seq;

typedef enum {
    V4_VI_WORK_1MULTIPLEX,
    V4_VI_WORK_2MULTIPLEX,
    V4_VI_WORK_4MULTIPLEX
} v4_vi_work;

typedef struct {
    unsigned int num;
    int pipeId[2];
} v4_vi_bind;

typedef struct {
    v4_common_dim size;
    v4_common_pixfmt pixFmt;
    v4_common_hdr dynRange;
    int videoFmt;
    v4_common_compr compress;
    int mirror;
    int flip;
    unsigned int depth;
    int srcFps;
    int dstFps;
} v4_vi_chn;

typedef struct {
    v4_common_pixfmt pixFmt;
    v4_common_prec prec;
    int srcRfrOrChn0;
    v4_common_compr compress;
} v4_vi_nred;

typedef struct {
    // Accepts values from 0-2 (no, front, back)
    int bypass;
    int yuvSkipOn;
    int ispBypassOn;
    v4_common_dim maxSize;
    v4_common_pixfmt pixFmt;
    v4_common_compr compress;
    v4_common_prec prec;
    int nRedOn;
    v4_vi_nred nRed;
    int sharpenOn;
    int srcFps;
    int dstFps;
    int discProPic;
} v4_vi_pipe;

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
} v4_vi_timing;

typedef struct {
    int vsyncPulse;
    int vsyncInv;
    int hsyncPulse;
    int hsyncInv;
    int vsyncValid;
    int vsyncValidInv;
    v4_vi_timing timing;
} v4_vi_sync;

typedef struct {
    v4_vi_intf intf;
    v4_vi_work work;
    unsigned int cmpntMask[2];
    int progressiveOn;
    int adChn[4];
    v4_vi_seq seq;
    v4_vi_sync sync;
    int rgbModeOn;
    int dataRevOn;
    v4_common_dim size;
    v4_common_dim bayerSize;
    v4_vi_rephase hsyncBayerReph;
    v4_vi_rephase vsyncBayerReph;
    v4_common_wdr wdr;
    unsigned int wdrCacheLine;
    int dataRate2X;
} v4_vi_dev;

typedef struct {
    v4_common_wdr mode;
    unsigned int cacheLine;
} v4_vi_wdr;

typedef struct {
    void *handle, *handleGoke;

    int (*fnDisableDevice)(int device);
    int (*fnEnableDevice)(int device);
    int (*fnSetDeviceConfig)(int device, v4_vi_dev *config);

    int (*fnDisableChannel)(int pipe, int channel);
    int (*fnEnableChannel)(int pipe, int channel);
    int (*fnSetChannelConfig)(int pipe, int channel, v4_vi_chn *config);

    int (*fnBindPipe)(int device, v4_vi_bind *config);
    int (*fnCreatePipe)(int pipe, v4_vi_pipe *config);
    int (*fnDestroyPipe)(int pipe);
    int (*fnStartPipe)(int pipe);
    int (*fnStopPipe)(int pipe);
} v4_vi_impl;

static int v4_vi_load(v4_vi_impl *vi_lib) {
    if ( !(vi_lib->handle = dlopen("libmpi.so", RTLD_LAZY | RTLD_GLOBAL)) &&

        (!(vi_lib->handleGoke = dlopen("libgk_api.so", RTLD_LAZY | RTLD_GLOBAL)) ||
         !(vi_lib->handle = dlopen("libhi_mpi.so", RTLD_LAZY | RTLD_GLOBAL))))
        HAL_ERROR("v4_vi", "Failed to load library!\nError: %s\n", dlerror());

    if (!(vi_lib->fnDisableDevice = (int(*)(int device))
        hal_symbol_load("v4_vi", vi_lib->handle, "HI_MPI_VI_DisableDev")))
        return EXIT_FAILURE;

    if (!(vi_lib->fnEnableDevice = (int(*)(int device))
        hal_symbol_load("v4_vi", vi_lib->handle, "HI_MPI_VI_EnableDev")))
        return EXIT_FAILURE;

    if (!(vi_lib->fnSetDeviceConfig = (int(*)(int device, v4_vi_dev *config))
        hal_symbol_load("v4_vi", vi_lib->handle, "HI_MPI_VI_SetDevAttr")))
        return EXIT_FAILURE;

    if (!(vi_lib->fnDisableChannel = (int(*)(int pipe, int channel))
        hal_symbol_load("v4_vi", vi_lib->handle, "HI_MPI_VI_DisableChn")))
        return EXIT_FAILURE;

    if (!(vi_lib->fnEnableChannel = (int(*)(int pipe, int channel))
        hal_symbol_load("v4_vi", vi_lib->handle, "HI_MPI_VI_EnableChn")))
        return EXIT_FAILURE;

    if (!(vi_lib->fnSetChannelConfig = (int(*)(int pipe, int channel, v4_vi_chn *config))
        hal_symbol_load("v4_vi", vi_lib->handle, "HI_MPI_VI_SetChnAttr")))
        return EXIT_FAILURE;

    if (!(vi_lib->fnBindPipe = (int(*)(int device, v4_vi_bind *config))
        hal_symbol_load("v4_vi", vi_lib->handle, "HI_MPI_VI_SetDevBindPipe")))
        return EXIT_FAILURE;

    if (!(vi_lib->fnCreatePipe = (int(*)(int pipe, v4_vi_pipe *config))
        hal_symbol_load("v4_vi", vi_lib->handle, "HI_MPI_VI_CreatePipe")))
        return EXIT_FAILURE;

    if (!(vi_lib->fnDestroyPipe = (int(*)(int pipe))
        hal_symbol_load("v4_vi", vi_lib->handle, "HI_MPI_VI_DestroyPipe")))
        return EXIT_FAILURE;

    if (!(vi_lib->fnStartPipe = (int(*)(int pipe))
        hal_symbol_load("v4_vi", vi_lib->handle, "HI_MPI_VI_StartPipe")))
        return EXIT_FAILURE;

    if (!(vi_lib->fnStopPipe = (int(*)(int pipe))
        hal_symbol_load("v4_vi", vi_lib->handle, "HI_MPI_VI_StopPipe")))
        return EXIT_FAILURE;

    return EXIT_SUCCESS;
}

static void v4_vi_unload(v4_vi_impl *vi_lib) {
    if (vi_lib->handle) dlclose(vi_lib->handle);
    vi_lib->handle = NULL;
    if (vi_lib->handleGoke) dlclose(vi_lib->handleGoke);
    vi_lib->handleGoke = NULL;
    memset(vi_lib, 0, sizeof(*vi_lib));
}