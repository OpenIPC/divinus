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
} v3_vpss_impl;

static int v3_vpss_load(v3_vpss_impl *vpss_lib) {
    if (!(vpss_lib->handle = dlopen("libmpi.so", RTLD_LAZY | RTLD_GLOBAL))) {
        fprintf(stderr, "[v3_vpss] Failed to load library!\nError: %s\n", dlerror());
        return EXIT_FAILURE;
    }

    if (!(vpss_lib->fnCreateGroup = (int(*)(int group, v3_vpss_grp *config))
        dlsym(vpss_lib->handle, "HI_MPI_VPSS_CreateGrp"))) {
        fprintf(stderr, "[v3_vpss] Failed to acquire symbol HI_MPI_VPSS_CreateGrp!\n");
        return EXIT_FAILURE;
    }

    if (!(vpss_lib->fnDestroyGroup = (int(*)(int group))
        dlsym(vpss_lib->handle, "HI_MPI_VPSS_DestroyGrp"))) {
        fprintf(stderr, "[v3_vpss] Failed to acquire symbol HI_MPI_VPSS_DestroyGrp!\n");
        return EXIT_FAILURE;
    }

    if (!(vpss_lib->fnResetGroup = (int(*)(int group))
        dlsym(vpss_lib->handle, "HI_MPI_VPSS_ResetGrp"))) {
        fprintf(stderr, "[v3_vpss] Failed to acquire symbol HI_MPI_VPSS_ResetGrp!\n");
        return EXIT_FAILURE;
    }

    if (!(vpss_lib->fnSetGroupConfig = (int(*)(int group, v3_vpss_grp *config))
        dlsym(vpss_lib->handle, "HI_MPI_VPSS_SetGrpAttr"))) {
        fprintf(stderr, "[v3_vpss] Failed to acquire symbol HI_MPI_VPSS_SetGrpAttr!\n");
        return EXIT_FAILURE;
    }

    if (!(vpss_lib->fnStartGroup = (int(*)(int group))
        dlsym(vpss_lib->handle, "HI_MPI_VPSS_StartGrp"))) {
        fprintf(stderr, "[v3_vpss] Failed to acquire symbol HI_MPI_VPSS_StartGrp!\n");
        return EXIT_FAILURE;
    }

    if (!(vpss_lib->fnStopGroup = (int(*)(int group))
        dlsym(vpss_lib->handle, "HI_MPI_VPSS_StopGrp"))) {
        fprintf(stderr, "[v3_vpss] Failed to acquire symbol HI_MPI_VPSS_StopGrp!\n");
        return EXIT_FAILURE;
    }

    if (!(vpss_lib->fnDisableChannel = (int(*)(int group, int channel))
        dlsym(vpss_lib->handle, "HI_MPI_VPSS_DisableChn"))) {
        fprintf(stderr, "[v3_vpss] Failed to acquire symbol HI_MPI_VPSS_DisableChn!\n");
        return EXIT_FAILURE;
    }

    if (!(vpss_lib->fnEnableChannel = (int(*)(int group, int channel))
        dlsym(vpss_lib->handle, "HI_MPI_VPSS_EnableChn"))) {
        fprintf(stderr, "[v3_vpss] Failed to acquire symbol HI_MPI_VPSS_EnableChn!\n");
        return EXIT_FAILURE;
    }

    if (!(vpss_lib->fnSetChannelConfig = (int(*)(int group, int channel, v3_vpss_chn *config))
        dlsym(vpss_lib->handle, "HI_MPI_VPSS_SetChnAttr"))) {
        fprintf(stderr, "[v3_vpss] Failed to acquire symbol HI_MPI_VPSS_SetChnAttr!\n");
        return EXIT_FAILURE;
    }
    
    return EXIT_SUCCESS;
}

static void v3_vpss_unload(v3_vpss_impl *vpss_lib) {
    if (vpss_lib->handle) dlclose(vpss_lib->handle);
    vpss_lib->handle = NULL;
    memset(vpss_lib, 0, sizeof(*vpss_lib));
}
