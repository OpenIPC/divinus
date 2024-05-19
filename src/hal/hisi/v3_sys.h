#pragma once

#include "v3_common.h"

#define V3_SYS_API "1.0"

typedef enum
{
    V3_SYS_MOD_CMPI,
    V3_SYS_MOD_VB,
    V3_SYS_MOD_SYS,
    V3_SYS_MOD_RGN,
    V3_SYS_MOD_CHNL,
    V3_SYS_MOD_VDEC,
    V3_SYS_MOD_GROUP,
    V3_SYS_MOD_VPSS,
    V3_SYS_MOD_VENC,
    V3_SYS_MOD_VDA,
    V3_SYS_MOD_H264E,
    V3_SYS_MOD_JPEGE,
    V3_SYS_MOD_MPEG4E,
    V3_SYS_MOD_H264D,
    V3_SYS_MOD_JPEGD,
    V3_SYS_MOD_VOU,
    V3_SYS_MOD_VIU,
    V3_SYS_MOD_DSU,
    V3_SYS_MOD_VALG,
    V3_SYS_MOD_RC,
    V3_SYS_MOD_AIO,
    V3_SYS_MOD_AI,
    V3_SYS_MOD_AO,
    V3_SYS_MOD_AENC,
    V3_SYS_MOD_ADEC,
    V3_SYS_MOD_AVENC,
    V3_SYS_MOD_PCIV,
    V3_SYS_MOD_PCIVFMW,
    V3_SYS_MOD_ISP,
    V3_SYS_MOD_IVE,
    V3_SYS_MOD_DCCM = 31,
    V3_SYS_MOD_DCCS,
    V3_SYS_MOD_PROC,
    V3_SYS_MOD_LOG,
    V3_SYS_MOD_MST_LOG,
    V3_SYS_MOD_VD,
    V3_SYS_MOD_VCMP = 38,
    V3_SYS_MOD_FB,
    V3_SYS_MOD_HDMI,
    V3_SYS_MOD_VOIE,
    V3_SYS_MOD_TDE,
    V3_SYS_MOD_USR,
    V3_SYS_MOD_VEDU,
    V3_SYS_MOD_VGS,
    V3_SYS_MOD_H265E,
    V3_SYS_MOD_FD,
    V3_SYS_MOD_ODT,
    V3_SYS_MOD_VQA,
    V3_SYS_MOD_LPR,
	V3_SYS_MOD_FISHEYE,
    V3_SYS_MOD_PHOTO,
    V3_SYS_MOD_EXTAO,
    V3_SYS_MOD_END
} v3_sys_mod;

typedef struct {
    v3_sys_mod module;
    int device;
    int channel;
} v3_sys_bind;

typedef struct {
    char version[64];
} v3_sys_ver;

typedef struct {
    void *handle, *handleGoke, *handleDnvqe, *handleVoiceEngine, *handleUpvqe, *handleSecureC;
    
    int (*fnExit)(void);
    int (*fnGetChipId)(unsigned int *chip);
    int (*fnGetVersion)(v3_sys_ver *version);
    int (*fnInit)(void);
    int (*fnSetAlignment)(unsigned int *width);

    int (*fnBind)(v3_sys_bind *source, v3_sys_bind *dest);
    int (*fnUnbind)(v3_sys_bind *source, v3_sys_bind *dest);
} v3_sys_impl;

static int v3_sys_load(v3_sys_impl *sys_lib) {
    if (!(sys_lib->handle = dlopen("libmpi.so", RTLD_LAZY | RTLD_GLOBAL)) &&
        (!(sys_lib->handleSecureC = dlopen("libsecurec.so", RTLD_LAZY | RTLD_GLOBAL)) ||
        !(sys_lib->handleUpvqe = dlopen("libupvqe.so", RTLD_LAZY | RTLD_GLOBAL)) ||
        !(sys_lib->handleVoiceEngine = dlopen("libvoice_engine.so", RTLD_LAZY | RTLD_GLOBAL)) ||
        !(sys_lib->handleDnvqe = dlopen("libdnvqe.so", RTLD_LAZY | RTLD_GLOBAL)) ||
        !(sys_lib->handleGoke = dlopen("libgk_api.so", RTLD_LAZY | RTLD_GLOBAL)) ||
        !(sys_lib->handle = dlopen("libhi_mpi.so", RTLD_LAZY | RTLD_GLOBAL)))) {
        fprintf(stderr, "[v3_sys] Failed to load library!\nError: %s\n", dlerror());
        return EXIT_FAILURE;
    }

    if (!(sys_lib->fnExit = (int(*)(void))
        dlsym(sys_lib->handle, "HI_MPI_SYS_Exit"))) {
        fprintf(stderr, "[v3_sys] Failed to acquire symbol HI_MPI_SYS_Exit!\n");
        return EXIT_FAILURE;
    }

    if (!(sys_lib->fnGetChipId = (int(*)(unsigned int *chip))
        dlsym(sys_lib->handle, "HI_MPI_SYS_GetChipId"))) {
        fprintf(stderr, "[v3_sys] Failed to acquire symbol HI_MPI_SYS_GetChipId!\n");
        return EXIT_FAILURE;
    }

    if (!(sys_lib->fnGetVersion = (int(*)(v3_sys_ver *version))
        dlsym(sys_lib->handle, "HI_MPI_SYS_GetVersion"))) {
        fprintf(stderr, "[v3_sys] Failed to acquire symbol HI_MPI_SYS_GetVersion!\n");
        return EXIT_FAILURE;
    }

    if (!(sys_lib->fnInit = (int(*)(void))
        dlsym(sys_lib->handle, "HI_MPI_SYS_Init"))) {
        fprintf(stderr, "[v3_sys] Failed to acquire symbol HI_MPI_SYS_Init!\n");
        return EXIT_FAILURE;
    }

    if (!(sys_lib->fnSetAlignment = (int(*)(unsigned int *width))
        dlsym(sys_lib->handle, "HI_MPI_SYS_SetConf"))) {
        fprintf(stderr, "[v3_sys] Failed to acquire symbol HI_MPI_SYS_SetConf!\n");
        return EXIT_FAILURE;
    }

    if (!(sys_lib->fnBind = (int(*)(v3_sys_bind *source, v3_sys_bind *dest))
        dlsym(sys_lib->handle, "HI_MPI_SYS_Bind"))) {
        fprintf(stderr, "[v3_sys] Failed to acquire symbol HI_MPI_SYS_Bind!\n");
        return EXIT_FAILURE;
    }

    if (!(sys_lib->fnUnbind = (int(*)(v3_sys_bind *source, v3_sys_bind *dest))
        dlsym(sys_lib->handle, "HI_MPI_SYS_UnBind"))) {
        fprintf(stderr, "[v3_sys] Failed to acquire symbol HI_MPI_SYS_UnBind!\n");
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}

static void v3_sys_unload(v3_sys_impl *sys_lib) {
    if (sys_lib->handle)
        dlclose(sys_lib->handle = NULL);
    if (sys_lib->handleGoke)
        dlclose(sys_lib->handleGoke = NULL);
    if (sys_lib->handleDnvqe)
        dlclose(sys_lib->handleDnvqe = NULL);
    if (sys_lib->handleVoiceEngine)
        dlclose(sys_lib->handleVoiceEngine = NULL);
    if (sys_lib->handleUpvqe)
        dlclose(sys_lib->handleUpvqe = NULL);
    if (sys_lib->handleSecureC)
        dlclose(sys_lib->handleSecureC = NULL);
    memset(sys_lib, 0, sizeof(*sys_lib));
}