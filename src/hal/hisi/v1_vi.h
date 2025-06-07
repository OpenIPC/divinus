#pragma once

#include "v1_common.h"

typedef enum {
    V1_VI_INTF_BT656,
    V1_VI_INTF_BT601,
    V1_VI_INTF_DIGITAL_CAMERA,
    V1_VI_INTF_BT1120_STANDARD,
    V1_VI_INTF_BT1120_INTERLEAVED,
    V1_VI_INTF_MIPI,
    V1_VI_INTF_LVDS,
    V1_VI_INTF_HISPI,
    V1_VI_INTF_END
} v1_vi_intf;

typedef enum {
    V1_VI_SEQ_VUVU = 0,
    V1_VI_SEQ_UVUV,
    V1_VI_SEQ_UYVY = 0,
    V1_VI_SEQ_VYUY,
    V1_VI_SEQ_YUYV,
    V1_VI_SEQ_YVYU,
    V1_VI_SEQ_END
} v1_vi_seq;

typedef enum {
    V1_VI_WORK_1MULTIPLEX,
    V1_VI_WORK_2MULTIPLEX,
    V1_VI_WORK_4MULTIPLEX
} v1_vi_work;

typedef struct {
    v1_common_rect capt;
    v1_common_dim dest;
    // Accepts values from 0-2 (top, bottom, both)
    int field;
    v1_common_pixfmt pixFmt;
    v1_common_compr compress;
    int mirror;
    int flip;
    int srcFps;
    int dstFps;
} v1_vi_chn;

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
} v1_vi_timing;

typedef struct {
    int vsyncPulse;
    int vsyncInv;
    int hsyncPulse;
    int hsyncInv;
    int vsyncValid;
    int vsyncValidInv;
    v1_vi_timing timing;
} v1_vi_sync;

typedef struct {
    v1_vi_intf intf;
    v1_vi_work work;
    unsigned int cmpntMask[2];
    int progressiveOn;
    int adChn[4];
    v1_vi_seq seq;
    v1_vi_sync sync;
    // Accepts values from 0-2 (bypass, isp, raw)
    int dataPath;
    int rgbModeOn;
    int dataRevOn;
    v1_common_rect rect;
} v1_vi_dev;

typedef struct {
    void *handle;

    int (*fnDisableDevice)(int device);
    int (*fnEnableDevice)(int device);
    int (*fnSetDeviceConfig)(int device, v1_vi_dev *config);

    int (*fnDisableChannel)(int channel);
    int (*fnEnableChannel)(int channel);
    int (*fnSetChannelConfig)(int channel, v1_vi_chn *config);
} v1_vi_impl;

static int v1_vi_load(v1_vi_impl *vi_lib) {
    if (!(vi_lib->handle = dlopen("libmpi.so", RTLD_LAZY | RTLD_GLOBAL)))
        HAL_ERROR("v1_vi", "Failed to load library!\nError: %s\n", dlerror());

    if (!(vi_lib->fnDisableDevice = (int(*)(int device))
        hal_symbol_load("v1_vi", vi_lib->handle, "HI_MPI_VI_DisableDev")))
        return EXIT_FAILURE;

    if (!(vi_lib->fnEnableDevice = (int(*)(int device))
        hal_symbol_load("v1_vi", vi_lib->handle, "HI_MPI_VI_EnableDev")))
        return EXIT_FAILURE;

    if (!(vi_lib->fnSetDeviceConfig = (int(*)(int device, v1_vi_dev *config))
        hal_symbol_load("v1_vi", vi_lib->handle, "HI_MPI_VI_SetDevAttr")))
        return EXIT_FAILURE;

    if (!(vi_lib->fnDisableChannel = (int(*)(int channel))
        hal_symbol_load("v1_vi", vi_lib->handle, "HI_MPI_VI_DisableChn")))
        return EXIT_FAILURE;

    if (!(vi_lib->fnEnableChannel = (int(*)(int channel))
        hal_symbol_load("v1_vi", vi_lib->handle, "HI_MPI_VI_EnableChn")))
        return EXIT_FAILURE;

    if (!(vi_lib->fnSetChannelConfig = (int(*)(int channel, v1_vi_chn *config))
        hal_symbol_load("v1_vi", vi_lib->handle, "HI_MPI_VI_SetChnAttr")))
        return EXIT_FAILURE;

    return EXIT_SUCCESS;
}

static void v1_vi_unload(v1_vi_impl *vi_lib) {
    if (vi_lib->handle) dlclose(vi_lib->handle);
    vi_lib->handle = NULL;
    memset(vi_lib, 0, sizeof(*vi_lib));
}