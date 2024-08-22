#pragma once

#include "i3_common.h"

typedef enum {
    I3_VI_SRC_VGA,
    I3_VI_SRC_YPBPR,
    I3_VI_SRC_CVBS,
    I3_VI_SRC_CVBS2,
    I3_VI_SRC_CVBS3,
    I3_VI_SRC_SVIDEO,
    I3_VI_SRC_DTV,
    I3_VI_SRC_SC0_VOP,
    I3_VI_SRC_SC1_VOP,
    I3_VI_SRC_SC2_VOP,
    I3_VI_SRC_BT656,
    I3_VI_SRC_BT656_1,
    I3_VI_SRC_CAMERA,
    I3_VI_SRC_HVSP,
    I3_VI_SRC_END,
} i3_vi_src;

typedef struct {
    i3_common_dim dest;
    i3_common_pixfmt pixFmt;
    i3_common_mode mode;
    void *reserved1;
    int reserved2[1];
} i3_vi_chn;

typedef struct {
    int enable;
    unsigned int x;
    unsigned int y;
    unsigned int width;
    unsigned int height;
} i3_vi_crop;

typedef struct {
    i3_vi_src source;
    float srcFps;
    i3_vi_crop crop;
    int reserved[1];
} i3_vi_dev;

typedef struct {
    void *handle;

    int (*fnSetDeviceConfig)(i3_vi_dev *config);

    int (*fnDisableChannel)(int channel);
    int (*fnEnableChannel)(int channel);
    int (*fnSetChannelConfig)(int channel, i3_vi_chn *config);
} i3_vi_impl;

static int i3_vi_load(i3_vi_impl *vi_lib) {
    if (!(vi_lib->handle = dlopen("libmi.so", RTLD_LAZY | RTLD_GLOBAL)))
        HAL_ERROR("i3_vi", "Failed to load library!\nError: %s\n", dlerror());

    if (!(vi_lib->fnSetDeviceConfig = (int(*)(i3_vi_dev *config))
        hal_symbol_load("i3_vi", vi_lib->handle, "MI_VI_SetDevAttr")))
        return EXIT_FAILURE;

    if (!(vi_lib->fnDisableChannel = (int(*)(int channel))
        hal_symbol_load("i3_vi", vi_lib->handle, "MI_VI_DisableChn")))
        return EXIT_FAILURE;

    if (!(vi_lib->fnEnableChannel = (int(*)(int channel))
        hal_symbol_load("i3_vi", vi_lib->handle, "MI_VI_EnableChn")))
        return EXIT_FAILURE;

    if (!(vi_lib->fnSetChannelConfig = (int(*)(int channel, i3_vi_chn *config))
        hal_symbol_load("i3_vi", vi_lib->handle, "MI_VI_SetChnAttr")))
        return EXIT_FAILURE;

    return EXIT_SUCCESS;
}

static void i3_vi_unload(i3_vi_impl *vi_lib) {
    if (vi_lib->handle) dlclose(vi_lib->handle);
    vi_lib->handle = NULL;
    memset(vi_lib, 0, sizeof(*vi_lib));
}