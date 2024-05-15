#pragma once

#include "v3_common.h"

typedef enum {
    V3_VI_INPUT_VUVU,
    V3_VI_INPUT_UVUV,
    V3_VI_INPUT_UYVY,
    V3_VI_INPUT_VYUY,
    V3_VI_INPUT_YUYV,
    V3_VI_INPUT_YVYU,
    V3_VI_INPUT_END
} v3_vi_input;

typedef enum {
    V3_VI_INTF_BT656,
    V3_VI_INTF_BT601,
    V3_VI_INTF_DIGITAL_CAMERA,
    V3_VI_INTF_BT1120_STANDARD,
    V3_VI_INTF_BT1120_INTERLEAVED,
    V3_VI_INTF_MIPI,
    V3_VI_INTF_LVDS,
    V3_VI_INTF_HISPI,
    V3_VI_INTF_END
} v3_vi_intf;

typedef enum {
    V3_VI_REPHASE_NONE,
    V3_VI_REPHASE_SKIP1_2,
    V3_VI_REPHASE_SKIP1_3,
    V3_VI_REPHASE_BINNING1_2,
    V3_VI_REPHASE_BINNING1_3,
    V3_VI_REPHASE_END
} v3_vi_rephase;

typedef enum {
    V3_VI_WORK_1MULTIPLEX,
    V3_VI_WORK_2MULTIPLEX,
    V3_VI_WORK_4MULTIPLEX
} v3_vi_work;

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
} v3_vi_timing;

typedef struct {
    int vsyncPulse;
    int vsyncInv;
    int hsyncPulse;
    int hsyncInv;
    int vsyncValid;
    int vsyncValidInv;
    v3_vi_timing timing;
} v3_vi_sync;

typedef struct {
    v3_common_rect capt;
    v3_common_dim dest;
    // Values 0-3 correspond to Top, Bottom, Both
    int field;
    v3_common_pixfmt pixFmt;
    v3_common_compr compress;
    int mirror;
    int flip;
    int srcFps;
    int dstFps;
} v3_vi_chn;

typedef struct {
    v3_vi_intf intf;
    v3_vi_work work;
    unsigned int cmpntMask[2];
    int progressiveOn;
    int adChn[4];
    v3_vi_input input;
    v3_vi_sync sync;
    // Accepts values between 0-2
    // (0: bypass ISP, 1: enable ISP, 2: raw)
    int path;
    int rgbModeOn;
    int bitswap;
    v3_common_rect capt;
    v3_vi_rephase hsyncBayerReph;
    v3_vi_rephase vsyncBayerReph;
    v3_common_dim bayerSize;
    int bayerComprOn;
} v3_vi_dev;

typedef struct {
    v3_common_wdr mode;
    int comprOn;
} v3_vi_wdr;

typedef struct {
    void *handle;

    int (*fnDisableDevice)(int device);
    int (*fnEnableDevice)(int device);
    int (*fnSetDeviceConfig)(int device, v3_vi_dev *config);
    int (*fnSetWDRMode)(int device, v3_vi_wdr *mode);

    int (*fnDisableChannel)(int channel);
    int (*fnEnableChannel)(int channel);
    int (*fnSetChannelConfig)(int channel, v3_vi_chn *config);
} v3_vi_impl;

int v3_vi_load(v3_vi_impl *vi_lib) {
    if (!(vi_lib->handle = dlopen("libmpi.so", RTLD_NOW))) {
        fprintf(stderr, "[v3_vi] Failed to load library!\n");
        return EXIT_FAILURE;
    }

    if (!(vi_lib->fnDisableDevice = (int(*)(int device))
        dlsym(vi_lib->handle, "HI_MPI_VI_DisableDev"))) {
        fprintf(stderr, "[v3_vi] Failed to acquire symbol HI_MPI_VI_DisableDev!\n");
        return EXIT_FAILURE;
    }

    if (!(vi_lib->fnEnableDevice = (int(*)(int device))
        dlsym(vi_lib->handle, "HI_MPI_VI_EnableDev"))) {
        fprintf(stderr, "[v3_vi] Failed to acquire symbol HI_MPI_VI_EnableDev!\n");
        return EXIT_FAILURE;
    }

    if (!(vi_lib->fnSetDeviceConfig = (int(*)(int device, v3_vi_dev *config))
        dlsym(vi_lib->handle, "HI_MPI_VI_SetDevAttr"))) {
        fprintf(stderr, "[v3_vi] Failed to acquire symbol HI_MPI_VI_SetDevAttr!\n");
        return EXIT_FAILURE;
    }

    if (!(vi_lib->fnSetWDRMode = (int(*)(int device, v3_vi_wdr *mode))
        dlsym(vi_lib->handle, "HI_MPI_VI_SetWDRMode"))) {
        fprintf(stderr, "[v3_vi] Failed to acquire symbol HI_MPI_VI_SetWDRMode!\n");
        return EXIT_FAILURE;
    }

    if (!(vi_lib->fnDisableChannel = (int(*)(int device))
        dlsym(vi_lib->handle, "HI_MPI_VI_DisableChn"))) {
        fprintf(stderr, "[v3_vi] Failed to acquire symbol HI_MPI_VI_DisableChn!\n");
        return EXIT_FAILURE;
    }

    if (!(vi_lib->fnEnableChannel = (int(*)(int device))
        dlsym(vi_lib->handle, "HI_MPI_VI_EnableChn"))) {
        fprintf(stderr, "[v3_vi] Failed to acquire symbol HI_MPI_VI_EnableChn!\n");
        return EXIT_FAILURE;
    }

    if (!(vi_lib->fnSetChannelConfig = (int(*)(int device, v3_vi_chn *config))
        dlsym(vi_lib->handle, "HI_MPI_VI_SetChnAttr"))) {
        fprintf(stderr, "[v3_vi] Failed to acquire symbol HI_MPI_VI_SetChnAttr!\n");
        return EXIT_FAILURE;
    }
}

void v3_vi_unload(v3_vi_impl *vi_lib) {
    if (vi_lib->handle)
        dlclose(vi_lib->handle = NULL);
    memset(vi_lib, 0, sizeof(*vi_lib));
}