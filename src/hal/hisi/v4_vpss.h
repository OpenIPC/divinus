#pragma once

#include "v4_common.h"

#define V4_VPSS_CHN_NUM 6
#define V4_VPSS_GRP_NUM 8

typedef enum {
    V4_VPSS_NMODE_VIDEO,
    V4_VPSS_NMODE_SNAP,
    V4_VPSS_NMODE_SPATIAL,
    V4_VPSS_NMODE_ENHANCE,
    V4_VPSS_NMODE_END
} v4_vpss_nmode;

typedef struct {
    int chnAutoOn;
    v4_common_dim dest;
    int videoFmt;
    v4_common_pixfmt pixFmt;
    v4_common_hdr hdr;
    v4_common_compr compress;
    int srcFps;
    int dstFps;
    int mirror;
    int flip;
    // Accepts values from 0-8
    unsigned int depth;
    // Accepts values from 0-2 (none, auto, manual)
    int aspectRatio;
    unsigned int aspectBgCol;
    v4_common_rect aspectRect;
} v4_vpss_chn;

typedef struct {
    v4_vpss_nmode mode;
    v4_common_compr compress;
    int motionCompOn;
} v4_vpss_nred;

typedef struct {
    v4_common_dim dest;
    v4_common_pixfmt pixFmt;
    v4_common_hdr hdr;
    int srcFps;
    int dstFps;
    int nRedOn;
    v4_vpss_nred nRed;
} v4_vpss_grp;

typedef struct {
    void *handle, *handleGoke;

    int (*fnCreateGroup)(int group, v4_vpss_grp *config);
    int (*fnDestroyGroup)(int group);
    int (*fnResetGroup)(int group);
    int (*fnSetGroupConfig)(int channel, v4_vpss_grp *config);
    int (*fnStartGroup)(int group);
    int (*fnStopGroup)(int group);

    int (*fnDisableChannel)(int group, int channel);
    int (*fnEnableChannel)(int group, int channel);
    int (*fnSetChannelConfig)(int group, int channel, v4_vpss_chn *config);
} v4_vpss_impl;

static int v4_vpss_load(v4_vpss_impl *vpss_lib) {
    if ( !(vpss_lib->handle = dlopen("libmpi.so", RTLD_LAZY | RTLD_GLOBAL)) &&

        (!(vpss_lib->handleGoke = dlopen("libgk_api.so", RTLD_LAZY | RTLD_GLOBAL)) ||
         !(vpss_lib->handle = dlopen("libhi_mpi.so", RTLD_LAZY | RTLD_GLOBAL))))
        HAL_ERROR("v4_vpss", "Failed to load library!\nError: %s\n", dlerror());

    if (!(vpss_lib->fnCreateGroup = (int(*)(int group, v4_vpss_grp *config))
        hal_symbol_load("v4_vpss", vpss_lib->handle, "HI_MPI_VPSS_CreateGrp")))
        return EXIT_FAILURE;

    if (!(vpss_lib->fnDestroyGroup = (int(*)(int group))
        hal_symbol_load("v4_vpss", vpss_lib->handle, "HI_MPI_VPSS_DestroyGrp")))
        return EXIT_FAILURE;

    if (!(vpss_lib->fnResetGroup = (int(*)(int group))
        hal_symbol_load("v4_vpss", vpss_lib->handle, "HI_MPI_VPSS_ResetGrp")))
        return EXIT_FAILURE;

    if (!(vpss_lib->fnSetGroupConfig = (int(*)(int group, v4_vpss_grp *config))
        hal_symbol_load("v4_vpss", vpss_lib->handle, "HI_MPI_VPSS_SetGrpAttr")))
        return EXIT_FAILURE;

    if (!(vpss_lib->fnStartGroup = (int(*)(int group))
        hal_symbol_load("v4_vpss", vpss_lib->handle, "HI_MPI_VPSS_StartGrp")))
        return EXIT_FAILURE;

    if (!(vpss_lib->fnStopGroup = (int(*)(int group))
        hal_symbol_load("v4_vpss", vpss_lib->handle, "HI_MPI_VPSS_StopGrp")))
        return EXIT_FAILURE;

    if (!(vpss_lib->fnDisableChannel = (int(*)(int group, int channel))
        hal_symbol_load("v4_vpss", vpss_lib->handle, "HI_MPI_VPSS_DisableChn")))
        return EXIT_FAILURE;

    if (!(vpss_lib->fnEnableChannel = (int(*)(int group, int channel))
        hal_symbol_load("v4_vpss", vpss_lib->handle, "HI_MPI_VPSS_EnableChn")))
        return EXIT_FAILURE;

    if (!(vpss_lib->fnSetChannelConfig = (int(*)(int group, int channel, v4_vpss_chn *config))
        hal_symbol_load("v4_vpss", vpss_lib->handle, "HI_MPI_VPSS_SetChnAttr")))
        return EXIT_FAILURE;
    
    return EXIT_SUCCESS;
}

static void v4_vpss_unload(v4_vpss_impl *vpss_lib) {
    if (vpss_lib->handle) dlclose(vpss_lib->handle);
    vpss_lib->handle = NULL;
    if (vpss_lib->handleGoke) dlclose(vpss_lib->handleGoke);
    vpss_lib->handleGoke = NULL;
    memset(vpss_lib, 0, sizeof(*vpss_lib));
}
