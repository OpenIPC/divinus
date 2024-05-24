#pragma once

#include "tx_common.h"

typedef struct {
	int enable;
	int left;
	int top;
	int width;
	int height;
} tx_fs_crop;

typedef struct {
	int enable;
	int width;
	int height;
} tx_fs_scale;

typedef struct {
    tx_common_dim dest;
    tx_common_pixfmt pixFmt;
    tx_fs_crop crop;
    tx_fs_scale scale;
    int fpsNum;
    int fpsDen;
    int bufCount;
    int phyOrExtChn;
    tx_fs_crop frame;
} tx_fs_chn;

typedef struct {
    int index;
    int poolId;
    unsigned int width;
    unsigned int height;
    tx_common_pixfmt pixFmt;
    unsigned int size;
    unsigned int phyAddr;
    unsigned int virtAddr;
    long long timestamp;
    int rotateFlag;
    unsigned int priv[0];
} tx_fs_frame;

typedef struct {
    void *handle;
    
    int (*fnCreateChannel)(int channel, tx_fs_chn *config);
    int (*fnDestroyChannel)(int channel);
    int (*fnDisableChannel)(int channel);
    int (*fnEnableChannel)(int channel);
    int (*fnSetChannelRotate)(int channel, char rotateMode, int width, int height);
    int (*fnSetChannelSource)(int channel, int source);

    int (*fnSnapshot)(int channel, tx_common_pixfmt pixFmt, int width, int height, 
        void *data, tx_fs_frame *info);
} tx_fs_impl;

static int tx_fs_load(tx_fs_impl *fs_lib) {
    if (!(fs_lib->handle = dlopen("libimp.so", RTLD_LAZY | RTLD_GLOBAL))) {
        fprintf(stderr, "[tx_fs] Failed to load library!\nError: %s\n", dlerror());
        return EXIT_FAILURE;
    }

    if (!(fs_lib->fnCreateChannel = (int(*)(int channel, tx_fs_chn *config))
        dlsym(fs_lib->handle, "IMP_FrameSource_CreateChn"))) {
        fprintf(stderr, "[tx_fs] Failed to acquire symbol IMP_FrameSource_CreateChn!\n");
        return EXIT_FAILURE;
    }

    if (!(fs_lib->fnDestroyChannel = (int(*)(int channel))
        dlsym(fs_lib->handle, "IMP_FrameSource_DestroyChn"))) {
        fprintf(stderr, "[tx_fs] Failed to acquire symbol IMP_FrameSource_DestroyChn!\n");
        return EXIT_FAILURE;
    }

    if (!(fs_lib->fnDisableChannel = (int(*)(int channel))
        dlsym(fs_lib->handle, "IMP_FrameSource_DisableChn"))) {
        fprintf(stderr, "[tx_fs] Failed to acquire symbol IMP_FrameSource_DisableChn!\n");
        return EXIT_FAILURE;
    }

    if (!(fs_lib->fnEnableChannel = (int(*)(int channel))
        dlsym(fs_lib->handle, "IMP_FrameSource_EnableChn"))) {
        fprintf(stderr, "[tx_fs] Failed to acquire symbol IMP_FrameSource_EnableChn!\n");
        return EXIT_FAILURE;
    }

    if (!(fs_lib->fnSetChannelRotate = (int(*)(int channel, char rotateMode, int width, int height))
        dlsym(fs_lib->handle, "IMP_FrameSource_SetChnRotate"))) {
        fprintf(stderr, "[tx_fs] Failed to acquire symbol IMP_FrameSource_SetChnRotate!\n");
        return EXIT_FAILURE;
    }

    if (!(fs_lib->fnSetChannelSource = (int(*)(int channel, int source))
        dlsym(fs_lib->handle, "IMP_FrameSource_SetSource"))) {
        fprintf(stderr, "[tx_fs] Failed to acquire symbol IMP_FrameSource_SetSource!\n");
        return EXIT_FAILURE;
    }

    if (!(fs_lib->fnSnapshot = (int(*)(int channel, tx_common_pixfmt pixFmt, int width, int height, 
        void *data, tx_fs_frame *info))
        dlsym(fs_lib->handle, "IMP_FrameSource_SnapFrame"))) {
        fprintf(stderr, "[tx_fs] Failed to acquire symbol IMP_FrameSource_SnapFrame!\n");
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}

static void tx_fs_unload(tx_fs_impl *fs_lib) {
    if (fs_lib->handle) dlclose(fs_lib->handle);
    fs_lib->handle = NULL;
    memset(fs_lib, 0, sizeof(*fs_lib));
}