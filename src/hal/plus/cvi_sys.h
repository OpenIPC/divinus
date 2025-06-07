#pragma once

#include "cvi_common.h"

#define CVI_SYS_API "1.0"

typedef enum {
    CVI_SYS_MOD_BASE,
    CVI_SYS_MOD_VB,
    CVI_SYS_MOD_SYS,
    CVI_SYS_MOD_RGN,
    CVI_SYS_MOD_CHNL,
    CVI_SYS_MOD_VDEC,
    CVI_SYS_MOD_VPSS,
    CVI_SYS_MOD_VENC,
    CVI_SYS_MOD_H264E,
    CVI_SYS_MOD_JPEGE,
    CVI_SYS_MOD_MPEG4E,
    CVI_SYS_MOD_H265E,
    CVI_SYS_MOD_JPEGD,
    CVI_SYS_MOD_VO,
    CVI_SYS_MOD_VI,
    CVI_SYS_MOD_DIS,
    CVI_SYS_MOD_RC,
    CVI_SYS_MOD_AIO,
    CVI_SYS_MOD_AI,
    CVI_SYS_MOD_AO,
    CVI_SYS_MOD_AENC,
    CVI_SYS_MOD_ADEC,
    CVI_SYS_MOD_AUD,
    CVI_SYS_MOD_VPU,
    CVI_SYS_MOD_ISP,
    CVI_SYS_MOD_IVE,
    CVI_SYS_MOD_USER,
    CVI_SYS_MOD_PROC,
    CVI_SYS_MOD_LOG,
    CVI_SYS_MOD_H264D,
    CVI_SYS_MOD_GDC,
    CVI_SYS_MOD_PHOTO,
    CVI_SYS_MOD_FB,
    CVI_SYS_MOD_END
} cvi_sys_mod;

typedef enum {
    CVI_SYS_VIMD_VIOFF_VPSSOFF,
    CVI_SYS_VIMD_VIOFF_VPSSON,
    CVI_SYS_VIMD_VION_VPSSOFF,
    CVI_SYS_VIMD_VION_VPSSON,
    CVI_SYS_VIMD_VIOFF_POSTON,
    CVI_SYS_VIMD_VIOFF_POSTOFF,
    CVI_SYS_VIMD_VION_POSTOFF,
    CVI_SYS_VIMD_VION_POSTON,
    CVI_SYS_VIMD_END
} cvi_sys_vimd;

typedef enum {
    CVI_SYS_VPSS_SINGLE,
    CVI_SYS_VPSS_DUAL,
    CVI_SYS_VPSS_END
} cvi_sys_vpss;

typedef struct {
    cvi_sys_mod module;
    int device;
    int channel;
} cvi_sys_bind;

typedef struct {
    cvi_sys_vpss mode;
    int inputIsIsp[2];
    // Only relevant with an ISP input
    int pipeId[2];
} cvi_sys_vpcf;

typedef struct {
    char version[128];
} cvi_sys_ver;

typedef struct {
    void *handle, *handleAtomic;
    
    int (*fnExit)(void);
    int (*fnGetChipId)(unsigned int *chip);
    int (*fnGetVersion)(cvi_sys_ver *version);
    int (*fnInit)(void);

    int (*fnBind)(cvi_sys_bind *source, cvi_sys_bind *dest);
    int (*fnUnbind)(cvi_sys_bind *source, cvi_sys_bind *dest);

    int (*fnGetViVpssMode)(cvi_sys_vimd *mode[CVI_VI_PIPE_NUM]);
    int (*fnSetViVpssMode)(cvi_sys_vimd *mode[CVI_VI_PIPE_NUM]);
    int (*fnGetVpssMode)(cvi_sys_vpcf *config);
    int (*fnSetVpssMode)(cvi_sys_vpcf *config);
} cvi_sys_impl;

static int cvi_sys_load(cvi_sys_impl *sys_lib) {
    if (!(sys_lib->handleAtomic = dlopen("libatomic.so.1", RTLD_LAZY | RTLD_GLOBAL)) ||
        !(sys_lib->handle = dlopen("libsys.so", RTLD_LAZY | RTLD_GLOBAL)))
        HAL_ERROR("cvi_sys", "Failed to load library!\nError: %s\n", dlerror());

    if (!(sys_lib->fnExit = (int(*)(void))
        hal_symbol_load("cvi_sys", sys_lib->handle, "CVI_SYS_Exit")))
        return EXIT_FAILURE;

    if (!(sys_lib->fnGetChipId = (int(*)(unsigned int *chip))
        hal_symbol_load("cvi_sys", sys_lib->handle, "CVI_SYS_GetChipId")))
        return EXIT_FAILURE;

    if (!(sys_lib->fnGetVersion = (int(*)(cvi_sys_ver *version))
        hal_symbol_load("cvi_sys", sys_lib->handle, "CVI_SYS_GetVersion")))
        return EXIT_FAILURE;

    if (!(sys_lib->fnInit = (int(*)(void))
        hal_symbol_load("cvi_sys", sys_lib->handle, "CVI_SYS_Init")))
        return EXIT_FAILURE;

    if (!(sys_lib->fnBind = (int(*)(cvi_sys_bind *source, cvi_sys_bind *dest))
        hal_symbol_load("cvi_sys", sys_lib->handle, "CVI_SYS_Bind")))
        return EXIT_FAILURE;

    if (!(sys_lib->fnUnbind = (int(*)(cvi_sys_bind *source, cvi_sys_bind *dest))
        hal_symbol_load("cvi_sys", sys_lib->handle, "CVI_SYS_UnBind")))
        return EXIT_FAILURE;

    if (!(sys_lib->fnGetViVpssMode = (int(*)(cvi_sys_vimd *mode[CVI_VI_PIPE_NUM]))
        hal_symbol_load("cvi_sys", sys_lib->handle, "CVI_SYS_GetVIVPSSMode")))
        return EXIT_FAILURE;

    if (!(sys_lib->fnSetViVpssMode = (int(*)(cvi_sys_vimd *mode[CVI_VI_PIPE_NUM]))
        hal_symbol_load("cvi_sys", sys_lib->handle, "CVI_SYS_SetVIVPSSMode")))
        return EXIT_FAILURE;

    if (!(sys_lib->fnGetVpssMode = (int(*)(cvi_sys_vpcf *config))
        hal_symbol_load("cvi_sys", sys_lib->handle, "CVI_SYS_SetVPSSModeEx")))
        return EXIT_FAILURE;

    if (!(sys_lib->fnSetVpssMode = (int(*)(cvi_sys_vpcf *config))
        hal_symbol_load("cvi_sys", sys_lib->handle, "CVI_SYS_SetVPSSModeEx")))
        return EXIT_FAILURE;

    return EXIT_SUCCESS;
}

static void cvi_sys_unload(cvi_sys_impl *sys_lib) {
    if (sys_lib->handle) dlclose(sys_lib->handle);
    sys_lib->handle = NULL;
    if (sys_lib->handleAtomic) dlclose(sys_lib->handleAtomic);
    sys_lib->handleAtomic = NULL;
    memset(sys_lib, 0, sizeof(*sys_lib));
}