#pragma once

#include "rk_common.h"

typedef enum {
	RK_VI_INTF_BT656,
	RK_VI_INTF_BT656_PACKED_YUV,
	RK_VI_INTF_BT601,
	RK_VI_INTF_DIGITAL_CAMERA,
	RK_VI_INTF_BT1120_STANDARD,
	RK_VI_INTF_BT1120_INTERLEAVED,
	RK_VI_INTF_MIPI,
	RK_VI_INTF_MIPI_YURK20_NORMAL,
	RK_VI_INTF_MIPI_YURK20_LEGACY,
	RK_VI_INTF_MIPI_YURK22,
	RK_VI_INTF_LVDS,
	RK_VI_INTF_HISPI,
	RK_VI_INTF_SLVS,
    RK_VI_INTF_END
} rk_vi_intf;

typedef enum {
    RK_VI_REPHASE_NONE,
    RK_VI_REPHASE_SKIP1_2,
    RK_VI_REPHASE_SKIP1_3,
    RK_VI_REPHASE_BINNING1_2,
    RK_VI_REPHASE_BINNING1_3,
    RK_VI_REPHASE_END
} rk_vi_rephase;

typedef enum {
    RK_VI_SEQ_VUVU,
    RK_VI_SEQ_UVUV,
    RK_VI_SEQ_UYVY,
    RK_VI_SEQ_VYUY,
    RK_VI_SEQ_YUYV,
    RK_VI_SEQ_YVYU,
    RK_VI_SEQ_END
} rk_vi_seq;

typedef enum {
    RK_VI_WORK_1MULTIPLEX,
    RK_VI_WORK_2MULTIPLEX,
    RK_VI_WORK_4MULTIPLEX
} rk_vi_work;

typedef struct {
	unsigned int num;
	int pipeId[2];
} rk_vi_bind;

typedef struct {
    rk_common_dim size;
    rk_common_pixfmt pixFmt;
    rk_common_hdr dynRange;
    int videoFmt;
    rk_common_compr compress;
    int mirror;
    int flip;
    unsigned int depth;
    int srcFps;
    int dstFps;
} rk_vi_chn;

typedef struct {
    rk_common_pixfmt pixFmt;
    rk_common_prec prec;
    int srcRfrOrChn0;
    rk_common_compr compress;
} rk_vi_nred;

typedef struct {
    // Accepts values from 0-2 (no, front, back)
    int bypass;
    int yuvSkipOn;
    int ispBypassOn;
    rk_common_dim maxSize;
    rk_common_pixfmt pixFmt;
    rk_common_compr compress;
    rk_common_prec prec;
    int nRedOn;
    rk_vi_nred nRed;
    int sharpenOn;
    int srcFps;
    int dstFps;
    int discProPic;
} rk_vi_pipe;

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
} rk_vi_timing;

typedef struct {
    int vsyncPulse;
    int vsyncInv;
    int hsyncPulse;
    int hsyncInv;
    int vsyncValid;
    int vsyncValidInv;
    rk_vi_timing timing;
} rk_vi_sync;

typedef struct {
    rk_vi_intf intf;
    rk_vi_work work;
    unsigned int cmpntMask[2];
    int progressiveOn;
    int adChn[4];
    rk_vi_seq seq;
    rk_vi_sync sync;
    int rgbModeOn;
    int dataRevOn;
    rk_common_dim size;
    rk_common_dim bayerSize;
    rk_vi_rephase hsyncBayerReph;
    rk_vi_rephase vsyncBayerReph;
    rk_common_wdr wdr;
    unsigned int wdrCacheLine;
    int dataRate2X;
} rk_vi_dev;

typedef struct {
    rk_common_wdr mode;
    unsigned int cacheLine;
} rk_vi_wdr;

typedef struct {
    void *handle;

    int (*fnDisableDevice)(int device);
    int (*fnEnableDevice)(int device);
    int (*fnSetDeviceConfig)(int device, rk_vi_dev *config);

    int (*fnDisableChannel)(int pipe, int channel);
    int (*fnEnableChannel)(int pipe, int channel);
    int (*fnSetChannelConfig)(int pipe, int channel, rk_vi_chn *config);

    int (*fnBindPipe)(int device, rk_vi_bind *config);
    int (*fnCreatePipe)(int pipe, rk_vi_pipe *config);
    int (*fnDestroyPipe)(int pipe);
    int (*fnStartPipe)(int pipe);
    int (*fnStopPipe)(int pipe);
} rk_vi_impl;

static int rk_vi_load(rk_vi_impl *vi_lib) {
    if (!(vi_lib->handle = dlopen("librockit.so", RTLD_LAZY | RTLD_GLOBAL)))
        HAL_ERROR("rk_vi", "Failed to load library!\nError: %s\n", dlerror());

    if (!(vi_lib->fnDisableDevice = (int(*)(int device))
        hal_symbol_load("rk_vi", vi_lib->handle, "RK_MPI_VI_DisableDev")))
        return EXIT_FAILURE;

    if (!(vi_lib->fnEnableDevice = (int(*)(int device))
        hal_symbol_load("rk_vi", vi_lib->handle, "RK_MPI_VI_EnableDev")))
        return EXIT_FAILURE;

    if (!(vi_lib->fnSetDeviceConfig = (int(*)(int device, rk_vi_dev *config))
        hal_symbol_load("rk_vi", vi_lib->handle, "RK_MPI_VI_SetDevAttr")))
        return EXIT_FAILURE;

    if (!(vi_lib->fnDisableChannel = (int(*)(int pipe, int channel))
        hal_symbol_load("rk_vi", vi_lib->handle, "RK_MPI_VI_DisableChn")))
        return EXIT_FAILURE;

    if (!(vi_lib->fnEnableChannel = (int(*)(int pipe, int channel))
        hal_symbol_load("rk_vi", vi_lib->handle, "RK_MPI_VI_EnableChn")))
        return EXIT_FAILURE;

    if (!(vi_lib->fnSetChannelConfig = (int(*)(int pipe, int channel, rk_vi_chn *config))
        hal_symbol_load("rk_vi", vi_lib->handle, "RK_MPI_VI_SetChnAttr")))
        return EXIT_FAILURE;


    if (!(vi_lib->fnBindPipe = (int(*)(int device, rk_vi_bind *config))
        hal_symbol_load("rk_vi", vi_lib->handle, "RK_MPI_VI_SetDevBindPipe")))
        return EXIT_FAILURE;

    if (!(vi_lib->fnCreatePipe = (int(*)(int pipe, rk_vi_pipe *config))
        hal_symbol_load("rk_vi", vi_lib->handle, "RK_MPI_VI_CreatePipe")))
        return EXIT_FAILURE;

    if (!(vi_lib->fnDestroyPipe = (int(*)(int pipe))
        hal_symbol_load("rk_vi", vi_lib->handle, "RK_MPI_VI_DestroyPipe")))
        return EXIT_FAILURE;

    if (!(vi_lib->fnStartPipe = (int(*)(int pipe))
        hal_symbol_load("rk_vi", vi_lib->handle, "RK_MPI_VI_StartPipe")))
        return EXIT_FAILURE;

    if (!(vi_lib->fnStopPipe = (int(*)(int pipe))
        hal_symbol_load("rk_vi", vi_lib->handle, "RK_MPI_VI_StopPipe")))
        return EXIT_FAILURE;

    return EXIT_SUCCESS;
}

static void rk_vi_unload(rk_vi_impl *vi_lib) {
    if (vi_lib->handle) dlclose(vi_lib->handle);
    vi_lib->handle = NULL;
    memset(vi_lib, 0, sizeof(*vi_lib));
}