#pragma once

#include "v3_common.h"

#define V3_SYS_API "1.0"

typedef enum {
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
    void *handle, *handleVoiceEngine, *handleDnvqe, *handleUpvqe;
    
    int (*fnExit)(void);
    int (*fnGetChipId)(unsigned int *chip);
    int (*fnGetVersion)(v3_sys_ver *version);
    
    int (*fnInit)(void);
    int (*fnSetAlignment)(unsigned int *width);

    int (*fnBind)(v3_sys_bind *source, v3_sys_bind *dest);
    int (*fnUnbind)(v3_sys_bind *source, v3_sys_bind *dest);

    int (*fnGetViVpssMode)(unsigned int *onlineOn);
} v3_sys_impl;

static int v3_sys_load(v3_sys_impl *sys_lib) {
    if (!(sys_lib->handleUpvqe = dlopen("libupvqe.so", RTLD_LAZY | RTLD_GLOBAL)) ||
        !(sys_lib->handleDnvqe = dlopen("libdnvqe.so", RTLD_LAZY | RTLD_GLOBAL)) ||
        !(sys_lib->handleVoiceEngine = dlopen("libVoiceEngine.so", RTLD_LAZY | RTLD_GLOBAL)) ||
        !(sys_lib->handle = dlopen("libmpi.so", RTLD_LAZY | RTLD_GLOBAL)))
        HAL_ERROR("v3_sys", "Failed to load library!\nError: %s\n", dlerror());

    if (!(sys_lib->fnExit = (int(*)(void))
        hal_symbol_load("v3_sys", sys_lib->handle, "HI_MPI_SYS_Exit")))
        return EXIT_FAILURE;

    if (!(sys_lib->fnGetChipId = (int(*)(unsigned int *chip))
        hal_symbol_load("v3_sys", sys_lib->handle, "HI_MPI_SYS_GetChipId")))
        return EXIT_FAILURE;

    if (!(sys_lib->fnGetVersion = (int(*)(v3_sys_ver *version))
        hal_symbol_load("v3_sys", sys_lib->handle, "HI_MPI_SYS_GetVersion")))
        return EXIT_FAILURE;

    if (!(sys_lib->fnInit = (int(*)(void))
        hal_symbol_load("v3_sys", sys_lib->handle, "HI_MPI_SYS_Init")))
        return EXIT_FAILURE;

    if (!(sys_lib->fnSetAlignment = (int(*)(unsigned int *width))
        hal_symbol_load("v3_sys", sys_lib->handle, "HI_MPI_SYS_SetConf")))
        return EXIT_FAILURE;

    if (!(sys_lib->fnBind = (int(*)(v3_sys_bind *source, v3_sys_bind *dest))
        hal_symbol_load("v3_sys", sys_lib->handle, "HI_MPI_SYS_Bind")))
        return EXIT_FAILURE;

    if (!(sys_lib->fnUnbind = (int(*)(v3_sys_bind *source, v3_sys_bind *dest))
        hal_symbol_load("v3_sys", sys_lib->handle, "HI_MPI_SYS_UnBind")))
        return EXIT_FAILURE;

    if (!(sys_lib->fnGetViVpssMode = (int(*)(unsigned int *onlineOn))
        hal_symbol_load("v3_sys", sys_lib->handle, "HI_MPI_SYS_GetViVpssMode")))
        return EXIT_FAILURE;

    return EXIT_SUCCESS;
}

static void v3_sys_unload(v3_sys_impl *sys_lib) {
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