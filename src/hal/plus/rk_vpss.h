#pragma once

#include "rk_common.h"

#define RK_VPSS_CHN_NUM 6
#define RK_VPSS_GRP_NUM 8

typedef enum {
    RK_VPSS_NMODE_VIDEO,
    RK_VPSS_NMODE_SNAP,
    RK_VPSS_NMODE_SPATIAL,
    RK_VPSS_NMODE_ENHANCE,
    RK_VPSS_NMODE_END
} rk_vpss_nmode;

typedef struct {
    int chnAutoOn;
    rk_common_dim dest;
    int videoFmt;
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
} rk_vpss_chn;

typedef struct {
    rk_vpss_nmode mode;
    rk_common_compr compress;
    int motionCompOn;
} rk_vpss_nred;

typedef struct {
    rk_common_dim dest;
    rk_common_pixfmt pixFmt;
    rk_common_hdr hdr;
    int srcFps;
    int dstFps;
    int nRedOn;
    rk_vpss_nred nRed;
} rk_vpss_grp;

typedef struct {
    void *handle;

    int (*fnCreateGroup)(int group, rk_vpss_grp *config);
    int (*fnDestroyGroup)(int group);
    int (*fnResetGroup)(int group);
    int (*fnSetGroupConfig)(int channel, rk_vpss_grp *config);
    int (*fnStartGroup)(int group);
    int (*fnStopGroup)(int group);

    int (*fnDisableChannel)(int group, int channel);
    int (*fnEnableChannel)(int group, int channel);
    int (*fnSetChannelConfig)(int group, int channel, rk_vpss_chn *config);
} rk_vpss_impl;

static int rk_vpss_load(rk_vpss_impl *vpss_lib) {
    if (!(vpss_lib->handle = dlopen("libmpi.so", RTLD_LAZY | RTLD_GLOBAL))) {
        fprintf(stderr, "[rk_vpss] Failed to load library!\nError: %s\n", dlerror());
        return EXIT_FAILURE;
    }

    if (!(vpss_lib->fnCreateGroup = (int(*)(int group, rk_vpss_grp *config))
        dlsym(vpss_lib->handle, "RK_MPI_VPSS_CreateGrp"))) {
        fprintf(stderr, "[rk_vpss] Failed to acquire symbol RK_MPI_VPSS_CreateGrp!\n");
        return EXIT_FAILURE;
    }

    if (!(vpss_lib->fnDestroyGroup = (int(*)(int group))
        dlsym(vpss_lib->handle, "RK_MPI_VPSS_DestroyGrp"))) {
        fprintf(stderr, "[rk_vpss] Failed to acquire symbol RK_MPI_VPSS_DestroyGrp!\n");
        return EXIT_FAILURE;
    }

    if (!(vpss_lib->fnResetGroup = (int(*)(int group))
        dlsym(vpss_lib->handle, "RK_MPI_VPSS_ResetGrp"))) {
        fprintf(stderr, "[rk_vpss] Failed to acquire symbol RK_MPI_VPSS_ResetGrp!\n");
        return EXIT_FAILURE;
    }

    if (!(vpss_lib->fnSetGroupConfig = (int(*)(int group, rk_vpss_grp *config))
        dlsym(vpss_lib->handle, "RK_MPI_VPSS_SetGrpAttr"))) {
        fprintf(stderr, "[rk_vpss] Failed to acquire symbol RK_MPI_VPSS_SetGrpAttr!\n");
        return EXIT_FAILURE;
    }

    if (!(vpss_lib->fnStartGroup = (int(*)(int group))
        dlsym(vpss_lib->handle, "RK_MPI_VPSS_StartGrp"))) {
        fprintf(stderr, "[rk_vpss] Failed to acquire symbol RK_MPI_VPSS_StartGrp!\n");
        return EXIT_FAILURE;
    }

    if (!(vpss_lib->fnStopGroup = (int(*)(int group))
        dlsym(vpss_lib->handle, "RK_MPI_VPSS_StopGrp"))) {
        fprintf(stderr, "[rk_vpss] Failed to acquire symbol RK_MPI_VPSS_StopGrp!\n");
        return EXIT_FAILURE;
    }

    if (!(vpss_lib->fnDisableChannel = (int(*)(int group, int channel))
        dlsym(vpss_lib->handle, "RK_MPI_VPSS_DisableChn"))) {
        fprintf(stderr, "[rk_vpss] Failed to acquire symbol RK_MPI_VPSS_DisableChn!\n");
        return EXIT_FAILURE;
    }

    if (!(vpss_lib->fnEnableChannel = (int(*)(int group, int channel))
        dlsym(vpss_lib->handle, "RK_MPI_VPSS_EnableChn"))) {
        fprintf(stderr, "[rk_vpss] Failed to acquire symbol RK_MPI_VPSS_EnableChn!\n");
        return EXIT_FAILURE;
    }

    if (!(vpss_lib->fnSetChannelConfig = (int(*)(int group, int channel, rk_vpss_chn *config))
        dlsym(vpss_lib->handle, "RK_MPI_VPSS_SetChnAttr"))) {
        fprintf(stderr, "[rk_vpss] Failed to acquire symbol RK_MPI_VPSS_SetChnAttr!\n");
        return EXIT_FAILURE;
    }
    
    return EXIT_SUCCESS;
}

static void rk_vpss_unload(rk_vpss_impl *vpss_lib) {
    if (vpss_lib->handle) dlclose(vpss_lib->handle);
    vpss_lib->handle = NULL;
    memset(vpss_lib, 0, sizeof(*vpss_lib));
}
