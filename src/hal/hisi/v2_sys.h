#pragma once

#include "v2_common.h"

#define V2_SYS_API "1.0"

typedef enum {
    V2_SYS_MOD_CMPI,
    V2_SYS_MOD_VB,
    V2_SYS_MOD_SYS,
    V2_SYS_MOD_RGN,
    V2_SYS_MOD_CHNL,
    V2_SYS_MOD_VDEC,
    V2_SYS_MOD_GROUP,
    V2_SYS_MOD_VPSS,
    V2_SYS_MOD_VENC,
    V2_SYS_MOD_VDA,
    V2_SYS_MOD_H264E,
    V2_SYS_MOD_JPEGE,
    V2_SYS_MOD_MPEG4E,
    V2_SYS_MOD_H264D,
    V2_SYS_MOD_JPEGD,
    V2_SYS_MOD_VOU,
    V2_SYS_MOD_VIU,
    V2_SYS_MOD_DSU,
    V2_SYS_MOD_VALG,
    V2_SYS_MOD_RC,
    V2_SYS_MOD_AIO,
    V2_SYS_MOD_AI,
    V2_SYS_MOD_AO,
    V2_SYS_MOD_AENC,
    V2_SYS_MOD_ADEC,
    V2_SYS_MOD_AVENC,
    V2_SYS_MOD_PCIV,
    V2_SYS_MOD_PCIVFMW,
    V2_SYS_MOD_ISP,
    V2_SYS_MOD_IVE,
    V2_SYS_MOD_DCCM = 31,
    V2_SYS_MOD_DCCS,
    V2_SYS_MOD_PROC,
    V2_SYS_MOD_LOG,
    V2_SYS_MOD_MST_LOG,
    V2_SYS_MOD_VD,
    V2_SYS_MOD_VCMP = 38,
    V2_SYS_MOD_FB,
    V2_SYS_MOD_HDMI,
    V2_SYS_MOD_VOIE,
    V2_SYS_MOD_TDE,
    V2_SYS_MOD_USR,
    V2_SYS_MOD_VEDU,
    V2_SYS_MOD_VGS,
    V2_SYS_MOD_H265E,
    V2_SYS_MOD_FD,
    V2_SYS_MOD_ODT,
    V2_SYS_MOD_VQA,
    V2_SYS_MOD_LPR,
    V2_SYS_MOD_FISHEYE,
    V2_SYS_MOD_PHOTO,
    V2_SYS_MOD_EXTAO,
    V2_SYS_MOD_END
} v2_sys_mod;

typedef struct {
    v2_sys_mod module;
    int device;
    int channel;
} v2_sys_bind;

typedef struct {
    char version[64];
} v2_sys_ver;

typedef struct {
    void *handle, *handleVoiceEngine, *handleDnvqe, *handleUpvqe;
    
    int (*fnExit)(void);
    int (*fnGetChipId)(unsigned int *chip);
    int (*fnGetVersion)(v2_sys_ver *version);
    
    int (*fnInit)(void);
    int (*fnSetAlignment)(unsigned int *width);

    int (*fnBind)(v2_sys_bind *source, v2_sys_bind *dest);
    int (*fnUnbind)(v2_sys_bind *source, v2_sys_bind *dest);

    int (*fnGetViVpssMode)(unsigned int *onlineOn);
} v2_sys_impl;

static int v2_sys_load(v2_sys_impl *sys_lib) {
    if (!(sys_lib->handleUpvqe = dlopen("libupvqe.so", RTLD_LAZY | RTLD_GLOBAL)) ||
        !(sys_lib->handleDnvqe = dlopen("libdnvqe.so", RTLD_LAZY | RTLD_GLOBAL)) ||
        !(sys_lib->handleVoiceEngine = dlopen("libVoiceEngine.so", RTLD_LAZY | RTLD_GLOBAL)) ||
        !(sys_lib->handle = dlopen("libmpi.so", RTLD_LAZY | RTLD_GLOBAL)))
        HAL_ERROR("v2_sys", "Failed to load library!\nError: %s\n", dlerror());

    if (!(sys_lib->fnExit = (int(*)(void))
        hal_symbol_load("v2_sys", sys_lib->handle, "HI_MPI_SYS_Exit")))
        return EXIT_FAILURE;

    if (!(sys_lib->fnGetChipId = (int(*)(unsigned int *chip))
        hal_symbol_load("v2_sys", sys_lib->handle, "HI_MPI_SYS_GetChipId")))
        return EXIT_FAILURE;

    if (!(sys_lib->fnGetVersion = (int(*)(v2_sys_ver *version))
        hal_symbol_load("v2_sys", sys_lib->handle, "HI_MPI_SYS_GetVersion")))
        return EXIT_FAILURE;

    if (!(sys_lib->fnInit = (int(*)(void))
        hal_symbol_load("v2_sys", sys_lib->handle, "HI_MPI_SYS_Init")))
        return EXIT_FAILURE;

    if (!(sys_lib->fnSetAlignment = (int(*)(unsigned int *width))
        hal_symbol_load("v2_sys", sys_lib->handle, "HI_MPI_SYS_SetConf")))
        return EXIT_FAILURE;

    if (!(sys_lib->fnBind = (int(*)(v2_sys_bind *source, v2_sys_bind *dest))
        hal_symbol_load("v2_sys", sys_lib->handle, "HI_MPI_SYS_Bind")))
        return EXIT_FAILURE;

    if (!(sys_lib->fnUnbind = (int(*)(v2_sys_bind *source, v2_sys_bind *dest))
        hal_symbol_load("v2_sys", sys_lib->handle, "HI_MPI_SYS_UnBind")))
        return EXIT_FAILURE;

    if (!(sys_lib->fnGetViVpssMode = (int(*)(unsigned int *onlineOn))
        hal_symbol_load("v2_sys", sys_lib->handle, "HI_MPI_SYS_GetViVpssMode")))
        return EXIT_FAILURE;

    return EXIT_SUCCESS;
}

static void v2_sys_unload(v2_sys_impl *sys_lib) {
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