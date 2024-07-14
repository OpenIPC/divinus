#pragma once

#include "v2_common.h"

#define V2_VPSS_CHN_NUM 11
#define V2_VPSS_GRP_NUM 32

typedef struct {
    int sharpOn;
    int borderEn;
    int mirror;
    int flip;
    int srcFps;
    int dstFps;
    v2_common_bord border;
} v2_vpss_chn;

typedef struct {
    v2_common_dim dest;
    v2_common_pixfmt pixFmt;
    int enhOn;
    int dciOn;
    int nredOn;
    int histEn;
    // Accepts values from 0-2 (auto, off, on)
    int interlMode;
} v2_vpss_grp;

typedef struct {
    int userModeOn;
    v2_common_dim dest;
    int doubleOn;
    v2_common_pixfmt pixFmt;
    v2_common_compr compress;
} v2_vpss_mode;

typedef struct {
    void *handle;

    int (*fnCreateGroup)(int group, v2_vpss_grp *config);
    int (*fnDestroyGroup)(int group);
    int (*fnResetGroup)(int group);
    int (*fnSetGroupConfig)(int channel, v2_vpss_grp *config);
    int (*fnStartGroup)(int group);
    int (*fnStopGroup)(int group);

    int (*fnDisableChannel)(int group, int channel);
    int (*fnEnableChannel)(int group, int channel);
    int (*fnSetChannelConfig)(int group, int channel, v2_vpss_chn *config);
    int (*fnSetChannelMode)(int group, int channel, v2_vpss_mode *config);
} v2_vpss_impl;

static int v2_vpss_load(v2_vpss_impl *vpss_lib) {
    if (!(vpss_lib->handle = dlopen("libmpi.so", RTLD_LAZY | RTLD_GLOBAL)))
        HAL_ERROR("v2_vpss", "Failed to load library!\nError: %s\n", dlerror());

    if (!(vpss_lib->fnCreateGroup = (int(*)(int group, v2_vpss_grp *config))
        hal_symbol_load("v2_vpss", vpss_lib->handle, "HI_MPI_VPSS_CreateGrp")))
        return EXIT_SUCCESS;

    if (!(vpss_lib->fnDestroyGroup = (int(*)(int group))
        hal_symbol_load("v2_vpss", vpss_lib->handle, "HI_MPI_VPSS_DestroyGrp")))
        return EXIT_FAILURE;

    if (!(vpss_lib->fnResetGroup = (int(*)(int group))
        hal_symbol_load("v2_vpss", vpss_lib->handle, "HI_MPI_VPSS_ResetGrp")))
        return EXIT_FAILURE;

    if (!(vpss_lib->fnSetGroupConfig = (int(*)(int group, v2_vpss_grp *config))
        hal_symbol_load("v2_vpss", vpss_lib->handle, "HI_MPI_VPSS_SetGrpAttr")))
        return EXIT_FAILURE;

    if (!(vpss_lib->fnStartGroup = (int(*)(int group))
        hal_symbol_load("v2_vpss", vpss_lib->handle, "HI_MPI_VPSS_StartGrp")))
        return EXIT_FAILURE;

    if (!(vpss_lib->fnStopGroup = (int(*)(int group))
        hal_symbol_load("v2_vpss", vpss_lib->handle, "HI_MPI_VPSS_StopGrp")))
        return EXIT_FAILURE;

    if (!(vpss_lib->fnDisableChannel = (int(*)(int group, int channel))
        hal_symbol_load("v2_vpss", vpss_lib->handle, "HI_MPI_VPSS_DisableChn")))
        return EXIT_FAILURE;

    if (!(vpss_lib->fnEnableChannel = (int(*)(int group, int channel))
        hal_symbol_load("v2_vpss", vpss_lib->handle, "HI_MPI_VPSS_EnableChn")))
        return EXIT_FAILURE;

    if (!(vpss_lib->fnSetChannelConfig = (int(*)(int group, int channel, v2_vpss_chn *config))
        hal_symbol_load("v2_vpss", vpss_lib->handle, "HI_MPI_VPSS_SetChnAttr")))
        return EXIT_FAILURE;

    if (!(vpss_lib->fnSetChannelMode = (int(*)(int group, int channel, v2_vpss_mode *config))
        hal_symbol_load("v2_vpss", vpss_lib->handle, "HI_MPI_VPSS_SetChnMode")))
        return EXIT_FAILURE;
    
    return EXIT_SUCCESS;
}

static void v2_vpss_unload(v2_vpss_impl *vpss_lib) {
    if (vpss_lib->handle) dlclose(vpss_lib->handle);
    vpss_lib->handle = NULL;
    memset(vpss_lib, 0, sizeof(*vpss_lib));
}
