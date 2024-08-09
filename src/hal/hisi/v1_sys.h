#pragma once

#include "v1_common.h"

#define V1_SYS_API "1.0"

typedef enum {
    V1_SYS_MOD_CMPI,
    V1_SYS_MOD_VB,
    V1_SYS_MOD_SYS,
    V1_SYS_MOD_VALG,
    V1_SYS_MOD_CHNL,
    V1_SYS_MOD_VDEC,
    V1_SYS_MOD_GROUP,
    V1_SYS_MOD_VENC,
    V1_SYS_MOD_VPSS,
    V1_SYS_MOD_VDA,
    V1_SYS_MOD_H264E,
    V1_SYS_MOD_JPEGE,
    V1_SYS_MOD_MPEG4E,
    V1_SYS_MOD_H264D,
    V1_SYS_MOD_JPEGD,
    V1_SYS_MOD_VOU,
    V1_SYS_MOD_VIU,
    V1_SYS_MOD_DSU,
    V1_SYS_MOD_RGN,
    V1_SYS_MOD_RC,
    V1_SYS_MOD_SIO,
    V1_SYS_MOD_AI,
    V1_SYS_MOD_AO,
    V1_SYS_MOD_AENC,
    V1_SYS_MOD_ADEC,
    V1_SYS_MOD_AVENC,
    V1_SYS_MOD_PCIV,
    V1_SYS_MOD_PCIVFMW,
    V1_SYS_MOD_ISP,
    V1_SYS_MOD_IVE,
    V1_SYS_MOD_DCCM = 31,
    V1_SYS_MOD_DCCS,
    V1_SYS_MOD_PROC,
    V1_SYS_MOD_LOG,
    V1_SYS_MOD_MST_LOG,
    V1_SYS_MOD_VD,
    V1_SYS_MOD_VCMP = 38,
    V1_SYS_MOD_FB,
    V1_SYS_MOD_HDMI,
    V1_SYS_MOD_VOIE,
    V1_SYS_MOD_TDE,
    V1_SYS_MOD_USR,
    V1_SYS_MOD_VEDU,
    V1_SYS_MOD_END
} v1_sys_mod;

typedef struct {
    v1_sys_mod module;
    int device;
    int channel;
} v1_sys_bind;

typedef struct {
    char version[64];
} v1_sys_ver;

typedef struct {
    void *handle, *handleVoiceEngine, *handleDnvqe, *handleUpvqe;
    
    int (*fnExit)(void);
    int (*fnGetChipId)(unsigned int *chip);
    int (*fnGetVersion)(v1_sys_ver *version);
    
    int (*fnInit)(void);
    int (*fnSetAlignment)(unsigned int *width);

    int (*fnBind)(v1_sys_bind *source, v1_sys_bind *dest);
    int (*fnUnbind)(v1_sys_bind *source, v1_sys_bind *dest);

    int (*fnGetViVpssMode)(unsigned int *onlineOn);
} v1_sys_impl;

static int v1_sys_load(v1_sys_impl *sys_lib) {
    if (!(sys_lib->handleUpvqe = dlopen("libupvqe.so", RTLD_LAZY | RTLD_GLOBAL)) ||
        !(sys_lib->handleDnvqe = dlopen("libdnvqe.so", RTLD_LAZY | RTLD_GLOBAL)) ||
        !(sys_lib->handleVoiceEngine = dlopen("libVoiceEngine.so", RTLD_LAZY | RTLD_GLOBAL)) ||
        !(sys_lib->handle = dlopen("libmpi.so", RTLD_LAZY | RTLD_GLOBAL)))
        HAL_ERROR("v1_sys", "Failed to load library!\nError: %s\n", dlerror());

    if (!(sys_lib->fnExit = (int(*)(void))
        hal_symbol_load("v1_sys", sys_lib->handle, "HI_MPI_SYS_Exit")))
        return EXIT_FAILURE;

    if (!(sys_lib->fnGetChipId = (int(*)(unsigned int *chip))
        hal_symbol_load("v1_sys", sys_lib->handle, "HI_MPI_SYS_GetChipId")))
        return EXIT_FAILURE;

    if (!(sys_lib->fnGetVersion = (int(*)(v1_sys_ver *version))
        hal_symbol_load("v1_sys", sys_lib->handle, "HI_MPI_SYS_GetVersion")))
        return EXIT_FAILURE;

    if (!(sys_lib->fnInit = (int(*)(void))
        hal_symbol_load("v1_sys", sys_lib->handle, "HI_MPI_SYS_Init")))
        return EXIT_FAILURE;

    if (!(sys_lib->fnSetAlignment = (int(*)(unsigned int *width))
        hal_symbol_load("v1_sys", sys_lib->handle, "HI_MPI_SYS_SetConf")))
        return EXIT_FAILURE;

    if (!(sys_lib->fnBind = (int(*)(v1_sys_bind *source, v1_sys_bind *dest))
        hal_symbol_load("v1_sys", sys_lib->handle, "HI_MPI_SYS_Bind")))
        return EXIT_FAILURE;

    if (!(sys_lib->fnUnbind = (int(*)(v1_sys_bind *source, v1_sys_bind *dest))
        hal_symbol_load("v1_sys", sys_lib->handle, "HI_MPI_SYS_UnBind")))
        return EXIT_FAILURE;

    return EXIT_SUCCESS;
}

static void v1_sys_unload(v1_sys_impl *sys_lib) {
    if (sys_lib->handleUpvqe) dlclose(sys_lib->handleUpvqe);
    sys_lib->handleUpvqe = NULL;
    if (sys_lib->handleDnvqe) dlclose(sys_lib->handleDnvqe);
    sys_lib->handleDnvqe = NULL;
    if (sys_lib->handleVoiceEngine) dlclose(sys_lib->handleVoiceEngine);
    sys_lib->handleVoiceEngine = NULL;
    if (sys_lib->handle) dlclose(sys_lib->handle);
    sys_lib->handle = NULL;
    memset(sys_lib, 0, sizeof(*sys_lib));
}