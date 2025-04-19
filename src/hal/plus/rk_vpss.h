#pragma once

#include "rk_common.h"

#define RK_VPSS_CHN_NUM 6
#define RK_VPSS_GRP_NUM 8

typedef enum {
    RK_VPSS_CMODE_USER,
    RK_VPSS_CMODE_AUTO,
    RK_VPSS_CMODE_PASSTHRU,
    RK_VPSS_CMODE_END
} rk_vpss_cmode;

typedef struct {
    rk_vpss_cmode chnMode;
    rk_common_dim dest;
    rk_common_vidfmt videoFmt;
    rk_common_pixfmt pixFmt;
    rk_common_hdr hdr;
    rk_common_compr compress;
    int srcFps;
    int dstFps;
    int mirror;
    int flip;
    // Accepts values from 0-8
    unsigned int depth;
    // Accepts values from 0-2 (none, auto, manual)
    int aspectRatio;
    unsigned int aspectBgCol;
    rk_common_rect aspectRect;
    unsigned int privFrmBufCnt;
} rk_vpss_chn;

typedef struct {
    rk_common_dim dest;
    rk_common_pixfmt pixFmt;
    rk_common_hdr hdr;
    int srcFps;
    int dstFps;
    rk_common_compr compress;
} rk_vpss_grp;

typedef struct {
    void *handle;

    int (*fnCreateGroup)(int group, rk_vpss_grp *config);
    int (*fnDestroyGroup)(int group);
    int (*fnResetGroup)(int group);
    int (*fnSetGroupConfig)(int channel, rk_vpss_grp *config);
    int (*fnSetGroupDevice)(int group, int gpuOrRga);
    int (*fnStartGroup)(int group);
    int (*fnStopGroup)(int group);

    int (*fnDisableChannel)(int group, int channel);
    int (*fnEnableChannel)(int group, int channel);
    int (*fnSetChannelConfig)(int group, int channel, rk_vpss_chn *config);
} rk_vpss_impl;

static int rk_vpss_load(rk_vpss_impl *vpss_lib) {
    if (!(vpss_lib->handle = dlopen("librockit.so", RTLD_LAZY | RTLD_GLOBAL)))
        HAL_ERROR("rk_vpss", "Failed to load library!\nError: %s\n", dlerror());

    if (!(vpss_lib->fnCreateGroup = (int(*)(int group, rk_vpss_grp *config))
        hal_symbol_load("rk_vpss", vpss_lib->handle, "RK_MPI_VPSS_CreateGrp")))
        return EXIT_FAILURE;

    if (!(vpss_lib->fnDestroyGroup = (int(*)(int group))
        hal_symbol_load("rk_vpss", vpss_lib->handle, "RK_MPI_VPSS_DestroyGrp")))
        return EXIT_FAILURE;

    if (!(vpss_lib->fnResetGroup = (int(*)(int group))
        hal_symbol_load("rk_vpss", vpss_lib->handle, "RK_MPI_VPSS_ResetGrp")))
        return EXIT_FAILURE;

    if (!(vpss_lib->fnSetGroupConfig = (int(*)(int group, rk_vpss_grp *config))
        hal_symbol_load("rk_vpss", vpss_lib->handle, "RK_MPI_VPSS_SetGrpAttr")))
        return EXIT_FAILURE;

    if (!(vpss_lib->fnSetGroupDevice = (int(*)(int group, int gpuOrRga))
        hal_symbol_load("rk_vpss", vpss_lib->handle, "RK_MPI_VPSS_SetVProcDev")))
        return EXIT_FAILURE;

    if (!(vpss_lib->fnStartGroup = (int(*)(int group))
        hal_symbol_load("rk_vpss", vpss_lib->handle, "RK_MPI_VPSS_StartGrp")))
        return EXIT_FAILURE;

    if (!(vpss_lib->fnStopGroup = (int(*)(int group))
        hal_symbol_load("rk_vpss", vpss_lib->handle, "RK_MPI_VPSS_StopGrp")))
        return EXIT_FAILURE;

    if (!(vpss_lib->fnDisableChannel = (int(*)(int group, int channel))
        hal_symbol_load("rk_vpss", vpss_lib->handle, "RK_MPI_VPSS_DisableChn")))
        return EXIT_FAILURE;

    if (!(vpss_lib->fnEnableChannel = (int(*)(int group, int channel))
        hal_symbol_load("rk_vpss", vpss_lib->handle, "RK_MPI_VPSS_EnableChn")))
        return EXIT_FAILURE;

    if (!(vpss_lib->fnSetChannelConfig = (int(*)(int group, int channel, rk_vpss_chn *config))
        hal_symbol_load("rk_vpss", vpss_lib->handle, "RK_MPI_VPSS_SetChnAttr")))
        return EXIT_FAILURE;

    return EXIT_SUCCESS;
}

static void rk_vpss_unload(rk_vpss_impl *vpss_lib) {
    if (vpss_lib->handle) dlclose(vpss_lib->handle);
    vpss_lib->handle = NULL;
    memset(vpss_lib, 0, sizeof(*vpss_lib));
}
