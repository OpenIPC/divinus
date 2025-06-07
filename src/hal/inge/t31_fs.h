#pragma once

#include "t31_common.h"

typedef struct {
    int enable;
    int left;
    int top;
    int width;
    int height;
} t31_fs_crop;

typedef struct {
    int enable;
    int width;
    int height;
} t31_fs_scale;

typedef struct {
    t31_common_dim dest;
    t31_common_pixfmt pixFmt;
    t31_fs_crop crop;
    t31_fs_scale scale;
    int fpsNum;
    int fpsDen;
    int bufCount;
    int phyOrExtChn;
    t31_fs_crop frame;
} t31_fs_chn;

typedef struct {
    int index;
    int poolId;
    unsigned int width;
    unsigned int height;
    t31_common_pixfmt pixFmt;
    unsigned int size;
    unsigned int phyAddr;
    unsigned int virtAddr;
    long long timestamp;
    int rotateFlag;
    unsigned int priv[0];
} t31_fs_frame;

typedef struct {
    void *handle;
    
    int (*fnCreateChannel)(int channel, t31_fs_chn *config);
    int (*fnDestroyChannel)(int channel);
    int (*fnDisableChannel)(int channel);
    int (*fnEnableChannel)(int channel);
    int (*fnSetChannelRotate)(int channel, char rotateMode, int width, int height);
    int (*fnSetChannelSource)(int channel, int source);

    int (*fnSnapshot)(int channel, t31_common_pixfmt pixFmt, int width, int height, 
        void *data, t31_fs_frame *info);
} t31_fs_impl;

static int t31_fs_load(t31_fs_impl *fs_lib) {
    if (!(fs_lib->handle = dlopen("libimp.so", RTLD_LAZY | RTLD_GLOBAL)))
        HAL_ERROR("t31_fs", "Failed to load library!\nError: %s\n", dlerror());

    if (!(fs_lib->fnCreateChannel = (int(*)(int channel, t31_fs_chn *config))
        hal_symbol_load("t31_fs", fs_lib->handle, "IMP_FrameSource_CreateChn")))
        return EXIT_FAILURE;

    if (!(fs_lib->fnDestroyChannel = (int(*)(int channel))
        hal_symbol_load("t31_fs", fs_lib->handle, "IMP_FrameSource_DestroyChn")))
        return EXIT_FAILURE;

    if (!(fs_lib->fnDisableChannel = (int(*)(int channel))
        hal_symbol_load("t31_fs", fs_lib->handle, "IMP_FrameSource_DisableChn")))
        return EXIT_FAILURE;

    if (!(fs_lib->fnEnableChannel = (int(*)(int channel))
        hal_symbol_load("t31_fs", fs_lib->handle, "IMP_FrameSource_EnableChn")))
        return EXIT_FAILURE;

    if (!(fs_lib->fnSetChannelRotate = (int(*)(int channel, char rotateMode, int width, int height))
        hal_symbol_load("t31_fs", fs_lib->handle, "IMP_FrameSource_SetChnRotate")))
        return EXIT_FAILURE;

    if (!(fs_lib->fnSetChannelSource = (int(*)(int channel, int source))
        hal_symbol_load("t31_fs", fs_lib->handle, "IMP_FrameSource_SetSource")))
        return EXIT_FAILURE;

    if (!(fs_lib->fnSnapshot = (int(*)(int channel, t31_common_pixfmt pixFmt, int width, int height, 
        void *data, t31_fs_frame *info))
        hal_symbol_load("t31_fs", fs_lib->handle, "IMP_FrameSource_SnapFrame")))
        return EXIT_FAILURE;

    return EXIT_SUCCESS;
}

static void t31_fs_unload(t31_fs_impl *fs_lib) {
    if (fs_lib->handle) dlclose(fs_lib->handle);
    fs_lib->handle = NULL;
    memset(fs_lib, 0, sizeof(*fs_lib));
}