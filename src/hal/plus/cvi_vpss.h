#pragma once

#include "cvi_common.h"

typedef struct {
    cvi_common_dim dest;
    int reserved;
    cvi_common_pixfmt pixFmt;
    int srcFps;
    int dstFps;
    int mirror;
    int flip;
    // Accepts values from 0-8
    unsigned int depth;
    // Accepts values from 0-2 (none, auto, manual)
    int aspectRatio;
    char aspectBgOn;
    unsigned int aspectBgCol;
    cvi_common_rect aspectRect;
} cvi_vpss_chn;

typedef struct {
    cvi_common_dim dest;
    cvi_common_pixfmt pixFmt;
    int srcFps;
    int dstFps;
    // Only used when VPSS mode is set to dual
    unsigned char device;
} cvi_vpss_grp;

typedef struct {
    void *handle;

    int (*fnCreateGroup)(int group, cvi_vpss_grp *config);
    int (*fnDestroyGroup)(int group);
    int (*fnResetGroup)(int group);
    int (*fnSetGroupConfig)(int channel, cvi_vpss_grp *config);
    int (*fnStartGroup)(int group);
    int (*fnStopGroup)(int group);

    int (*fnDisableChannel)(int group, int channel);
    int (*fnEnableChannel)(int group, int channel);
    int (*fnSetChannelConfig)(int group, int channel, cvi_vpss_chn *config);
    int (*fnAttachChannelPool)(int group, int channel, int pool);
    int (*fnDetachChannelPool)(int group, int channel);
} cvi_vpss_impl;

static int cvi_vpss_load(cvi_vpss_impl *vpss_lib) {
    if ( !(vpss_lib->handle = dlopen("libvpu.so", RTLD_LAZY | RTLD_GLOBAL)))
        HAL_ERROR("cvi_vpss", "Failed to load library!\nError: %s\n", dlerror());

    if (!(vpss_lib->fnCreateGroup = (int(*)(int group, cvi_vpss_grp *config))
        hal_symbol_load("cvi_vpss", vpss_lib->handle, "CVI_VPSS_CreateGrp")))
        return EXIT_FAILURE;

    if (!(vpss_lib->fnDestroyGroup = (int(*)(int group))
        hal_symbol_load("cvi_vpss", vpss_lib->handle, "CVI_VPSS_DestroyGrp")))
        return EXIT_FAILURE;

    if (!(vpss_lib->fnResetGroup = (int(*)(int group))
        hal_symbol_load("cvi_vpss", vpss_lib->handle, "CVI_VPSS_ResetGrp")))
        return EXIT_FAILURE;

    if (!(vpss_lib->fnSetGroupConfig = (int(*)(int group, cvi_vpss_grp *config))
        hal_symbol_load("cvi_vpss", vpss_lib->handle, "CVI_VPSS_SetGrpAttr")))
        return EXIT_FAILURE;

    if (!(vpss_lib->fnStartGroup = (int(*)(int group))
        hal_symbol_load("cvi_vpss", vpss_lib->handle, "CVI_VPSS_StartGrp")))
        return EXIT_FAILURE;

    if (!(vpss_lib->fnStopGroup = (int(*)(int group))
        hal_symbol_load("cvi_vpss", vpss_lib->handle, "CVI_VPSS_StopGrp")))
        return EXIT_FAILURE;

    if (!(vpss_lib->fnDisableChannel = (int(*)(int group, int channel))
        hal_symbol_load("cvi_vpss", vpss_lib->handle, "CVI_VPSS_DisableChn")))
        return EXIT_FAILURE;

    if (!(vpss_lib->fnEnableChannel = (int(*)(int group, int channel))
        hal_symbol_load("cvi_vpss", vpss_lib->handle, "CVI_VPSS_EnableChn")))
        return EXIT_FAILURE;

    if (!(vpss_lib->fnSetChannelConfig = (int(*)(int group, int channel, cvi_vpss_chn *config))
        hal_symbol_load("cvi_vpss", vpss_lib->handle, "CVI_VPSS_SetChnAttr")))
        return EXIT_FAILURE;

    if (!(vpss_lib->fnAttachChannelPool = (int(*)(int group, int channel, int pool))
        hal_symbol_load("cvi_vpss", vpss_lib->handle, "CVI_VPSS_AttachVbPool")))
        return EXIT_FAILURE;

    if (!(vpss_lib->fnDetachChannelPool = (int(*)(int group, int channel))
        hal_symbol_load("cvi_vpss", vpss_lib->handle, "CVI_VPSS_DetachVbPool")))
        return EXIT_FAILURE;
    
    return EXIT_SUCCESS;
}

static void cvi_vpss_unload(cvi_vpss_impl *vpss_lib) {
    if (vpss_lib->handle) dlclose(vpss_lib->handle);
    vpss_lib->handle = NULL;
    memset(vpss_lib, 0, sizeof(*vpss_lib));
}
