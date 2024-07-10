#pragma once

#include "v3_common.h"

#define V3_VPSS_CHN_NUM 11
#define V3_VPSS_GRP_NUM 32

typedef struct {
    int sharpOn;
    int borderEn;
    int mirror;
    int flip;
    int srcFps;
    int dstFps;
    v3_common_bord border;
} v3_vpss_chn;

typedef struct {
    v3_common_dim dest;
    v3_common_pixfmt pixFmt;
    int enhOn;
    int dciOn;
    int nredOn;
    int histEn;
    // Accepts values from 0-2 (auto, off, on)
    int interlMode;
    int sharpOn;
} v3_vpss_grp;

typedef struct {
    int userModeOn;
    v3_common_dim dest;
    int doubleOn;
    v3_common_pixfmt pixFmt;
    v3_common_compr compress;
} v3_vpss_mode;

typedef struct {
    void *handle;

    int (*fnCreateGroup)(int group, v3_vpss_grp *config);
    int (*fnDestroyGroup)(int group);
    int (*fnResetGroup)(int group);
    int (*fnSetGroupConfig)(int channel, v3_vpss_grp *config);
    int (*fnStartGroup)(int group);
    int (*fnStopGroup)(int group);

    int (*fnDisableChannel)(int group, int channel);
    int (*fnEnableChannel)(int group, int channel);
    int (*fnSetChannelConfig)(int group, int channel, v3_vpss_chn *config);
    int (*fnSetChannelMode)(int group, int channel, v3_vpss_mode *config);
} v3_vpss_impl;

static int v3_vpss_load(v3_vpss_impl *vpss_lib) {
    if (!(vpss_lib->handle = dlopen("libmpi.so", RTLD_LAZY | RTLD_GLOBAL)))
        HAL_ERROR("v3_vpss", "Failed to load library!\nError: %s\n", dlerror());

    if (!(vpss_lib->fnCreateGroup = (int(*)(int group, v3_vpss_grp *config))
        hal_symbol_load("v3_vpss", vpss_lib->handle, "HI_MPI_VPSS_CreateGrp")))
        return EXIT_SUCCESS;

    if (!(vpss_lib->fnDestroyGroup = (int(*)(int group))
        hal_symbol_load("v3_vpss", vpss_lib->handle, "HI_MPI_VPSS_DestroyGrp")))
        return EXIT_FAILURE;

    if (!(vpss_lib->fnResetGroup = (int(*)(int group))
        hal_symbol_load("v3_vpss", vpss_lib->handle, "HI_MPI_VPSS_ResetGrp")))
        return EXIT_FAILURE;

    if (!(vpss_lib->fnSetGroupConfig = (int(*)(int group, v3_vpss_grp *config))
        hal_symbol_load("v3_vpss", vpss_lib->handle, "HI_MPI_VPSS_SetGrpAttr")))
        return EXIT_FAILURE;

    if (!(vpss_lib->fnStartGroup = (int(*)(int group))
        hal_symbol_load("v3_vpss", vpss_lib->handle, "HI_MPI_VPSS_StartGrp")))
        return EXIT_FAILURE;

    if (!(vpss_lib->fnStopGroup = (int(*)(int group))
        hal_symbol_load("v3_vpss", vpss_lib->handle, "HI_MPI_VPSS_StopGrp")))
        return EXIT_FAILURE;

    if (!(vpss_lib->fnDisableChannel = (int(*)(int group, int channel))
        hal_symbol_load("v3_vpss", vpss_lib->handle, "HI_MPI_VPSS_DisableChn")))
        return EXIT_FAILURE;

    if (!(vpss_lib->fnEnableChannel = (int(*)(int group, int channel))
        hal_symbol_load("v3_vpss", vpss_lib->handle, "HI_MPI_VPSS_EnableChn")))
        return EXIT_FAILURE;

    if (!(vpss_lib->fnSetChannelConfig = (int(*)(int group, int channel, v3_vpss_chn *config))
        hal_symbol_load("v3_vpss", vpss_lib->handle, "HI_MPI_VPSS_SetChnAttr")))
        return EXIT_FAILURE;

    if (!(vpss_lib->fnSetChannelMode = (int(*)(int group, int channel, v3_vpss_mode *config))
        hal_symbol_load("v3_vpss", vpss_lib->handle, "HI_MPI_VPSS_SetChnMode")))
        return EXIT_FAILURE;
    
    return EXIT_SUCCESS;
}

static void v3_vpss_unload(v3_vpss_impl *vpss_lib) {
    if (vpss_lib->handle) dlclose(vpss_lib->handle);
    vpss_lib->handle = NULL;
    memset(vpss_lib, 0, sizeof(*vpss_lib));
}
