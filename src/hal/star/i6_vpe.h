#pragma once

#include "i6_common.h"

typedef enum {
    I6_VPE_MODE_INVALID,
    I6_VPE_MODE_DVR = 0x1,
    I6_VPE_MODE_CAM_TOP = 0x2,
    I6_VPE_MODE_CAM_BOTTOM = 0x4,
    I6_VPE_MODE_CAM = I6_VPE_MODE_CAM_TOP | I6_VPE_MODE_CAM_BOTTOM,
    I6_VPE_MODE_REALTIME_TOP = 0x8,
    I6_VPE_MODE_REALTIME_BOTTOM = 0x10,
    I6_VPE_MODE_REALTIME = I6_VPE_MODE_REALTIME_TOP | I6_VPE_MODE_REALTIME_BOTTOM,
    I6_VPE_MODE_END
} i6_vpe_mode;

typedef enum {
    I6_VPE_SENS_INVALID,
    I6_VPE_SENS_ID0,
    I6_VPE_SENS_ID1,
    I6_VPE_SENS_ID2,
    I6_VPE_SENS_ID3,
    I6_VPE_SENS_END,
} i6_vpe_sens;

typedef struct {
    unsigned int rev;
    unsigned int size;
    unsigned char data[64];
} i6_vpe_iqver;

typedef struct {
    int mode;
    char bypassOn;
    char proj3x3On;
    int proj3x3[9];
    unsigned short userSliceNum;
    unsigned int focalLengthX;
    unsigned int focalLengthY;
    void *configAddr;
    unsigned int configSize;
    int mapType;
    union {
        struct {
            void *xMapAddr, *yMapAddr;
            unsigned int xMapSize, yMapSize;
        } dispInfo;
        struct {
            void *calibPolyBinAddr;
            unsigned int calibPolyBinSize;
        } calibInfo;
    };
    char lensAdjOn;
} i6e_vpe_ildc;

typedef struct {
    char bypassOn;
    char proj3x3On;
    int proj3x3[9];
    unsigned int focalLengthX;
    unsigned int focalLengthY;
    void *configAddr;
    unsigned int configSize;
    union {
        struct {
            void *xMapAddr, *yMapAddr;
            unsigned int xMapSize, yMapSize;
        } dispInfo;
        struct {
            void *calibPolyBinAddr;
            unsigned int calibPolyBinSize;
        } calibInfo;
    };
} i6e_vpe_ldc;

typedef struct {
    i6_common_dim capt;
    i6_common_pixfmt pixFmt;
    i6_common_hdr hdr;
    i6_vpe_sens sensor;
    char noiseRedOn;
    char edgeOn;
    char edgeSmoothOn;
    char contrastOn;
    char invertOn;
    char rotateOn;
    i6_vpe_mode mode;
    i6_vpe_iqver iqparam;
    i6e_vpe_ildc lensInit;
    char lensAdjOn;
    unsigned int chnPort;
} i6e_vpe_chn;

typedef struct {
    i6_common_dim capt;
    i6_common_pixfmt pixFmt;
    i6_common_hdr hdr;
    i6_vpe_sens sensor;
    char noiseRedOn;
    char edgeOn;
    char edgeSmoothOn;
    char contrastOn;
    char invertOn;
    char rotateOn;
    i6_vpe_mode mode;
    i6_vpe_iqver iqparam;
    char lensAdjOn;
    unsigned int chnPort;
} i6_vpe_chn;

typedef struct {
    char reserved[16];
    i6e_vpe_ldc lensAdj;
    i6_common_hdr hdr;
    // Accepts values from 0-7
    int level3DNR;
    char mirror;
    char flip;
    char reserved2;
    char lensAdjOn;
} i6e_vpe_para;

typedef struct {
    char reserved[16];
    i6_common_hdr hdr;
    // Accepts values from 0-7
    int level3DNR;
    char mirror;
    char flip;
    char reserved2;
    char lensAdjOn;
} i6_vpe_para;

typedef struct {
    i6_common_dim output;
    char mirror;
    char flip;
    i6_common_pixfmt pixFmt;
    i6_common_compr compress;
} i6_vpe_port;

typedef struct {
    void *handle;

    int (*fnCreateChannel)(int channel, i6_vpe_chn *config);
    int (*fnDestroyChannel)(int channel);
    int (*fnSetChannelConfig)(int channel, i6_vpe_chn *config);
    int (*fnSetChannelParam)(int channel, i6_vpe_para *config);
    int (*fnStartChannel)(int channel);
    int (*fnStopChannel)(int channel);

    int (*fnDisablePort)(int channel, int port);
    int (*fnEnablePort)(int channel, int port);
    int (*fnSetPortConfig)(int channel, int port, i6_vpe_port *config);
} i6_vpe_impl;

static int i6_vpe_load(i6_vpe_impl *vpe_lib) {
    if (!(vpe_lib->handle = dlopen("libmi_vpe.so", RTLD_LAZY | RTLD_GLOBAL)))
        HAL_ERROR("i6_vpe", "Failed to load library!\nError: %s\n", dlerror());

    if (!(vpe_lib->fnCreateChannel = (int(*)(int channel, i6_vpe_chn *config))
        hal_symbol_load("i6_vpe", vpe_lib->handle, "MI_VPE_CreateChannel")))
        return EXIT_FAILURE;

    if (!(vpe_lib->fnDestroyChannel = (int(*)(int channel))
        hal_symbol_load("i6_vpe", vpe_lib->handle, "MI_VPE_DestroyChannel")))
        return EXIT_FAILURE;

    if (!(vpe_lib->fnSetChannelConfig = (int(*)(int channel, i6_vpe_chn *config))
        hal_symbol_load("i6_vpe", vpe_lib->handle, "MI_VPE_SetChannelAttr")))
        return EXIT_FAILURE;

    if (!(vpe_lib->fnSetChannelParam = (int(*)(int channel, i6_vpe_para *config))
        hal_symbol_load("i6_vpe", vpe_lib->handle, "MI_VPE_SetChannelParam")))
        return EXIT_FAILURE;

    if (!(vpe_lib->fnStartChannel = (int(*)(int channel))
        hal_symbol_load("i6_vpe", vpe_lib->handle, "MI_VPE_StartChannel")))
        return EXIT_FAILURE;

    if (!(vpe_lib->fnStopChannel = (int(*)(int channel))
        hal_symbol_load("i6_vpe", vpe_lib->handle, "MI_VPE_StopChannel")))
        return EXIT_FAILURE;

    if (!(vpe_lib->fnDisablePort = (int(*)(int channel, int port))
        hal_symbol_load("i6_vpe", vpe_lib->handle, "MI_VPE_DisablePort")))
        return EXIT_FAILURE;

    if (!(vpe_lib->fnEnablePort = (int(*)(int channel, int port))
        hal_symbol_load("i6_vpe", vpe_lib->handle, "MI_VPE_EnablePort")))
        return EXIT_FAILURE;

    if (!(vpe_lib->fnSetPortConfig = (int(*)(int channel, int port, i6_vpe_port *config))
        hal_symbol_load("i6_vpe", vpe_lib->handle, "MI_VPE_SetPortMode")))
        return EXIT_FAILURE;

    return EXIT_SUCCESS;
}

static void i6_vpe_unload(i6_vpe_impl *vpe_lib) {
    if (vpe_lib->handle) dlclose(vpe_lib->handle);
    vpe_lib->handle = NULL;
    memset(vpe_lib, 0, sizeof(*vpe_lib));
}