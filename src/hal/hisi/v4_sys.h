#pragma once

#include "v4_common.h"

#define V4_SYS_API "1.0"

typedef enum {
    V4_SYS_MOD_CMPI,
    V4_SYS_MOD_VB,
    V4_SYS_MOD_SYS,
    V4_SYS_MOD_RGN,
    V4_SYS_MOD_CHNL,
    V4_SYS_MOD_VDEC,
    V4_SYS_MOD_GROUP,
    V4_SYS_MOD_VPSS,
    V4_SYS_MOD_VENC,
    V4_SYS_MOD_SVP,
    V4_SYS_MOD_H264E,
    V4_SYS_MOD_JPEGE,
    V4_SYS_MOD_MPEG4E,
    V4_SYS_MOD_H265E,
    V4_SYS_MOD_JPEGD,
    V4_SYS_MOD_VOU,
    V4_SYS_MOD_VIU,
    V4_SYS_MOD_DSU,
    V4_SYS_MOD_VALG,
    V4_SYS_MOD_RC,
    V4_SYS_MOD_AIO,
    V4_SYS_MOD_AI,
    V4_SYS_MOD_AO,
    V4_SYS_MOD_AENC,
    V4_SYS_MOD_ADEC,
    V4_SYS_MOD_AVENC,
    V4_SYS_MOD_PCIV,
    V4_SYS_MOD_PCIVFMW,
    V4_SYS_MOD_ISP,
    V4_SYS_MOD_IVE,
    V4_SYS_MOD_USER,
    V4_SYS_MOD_DCCM,
    V4_SYS_MOD_DCCS,
    V4_SYS_MOD_PROC,
    V4_SYS_MOD_LOG,
    V4_SYS_MOD_VFMW,
    V4_SYS_MOD_H264D,
    V4_SYS_MOD_GDC,
    V4_SYS_MOD_PHOTO,
    V4_SYS_MOD_FB,
    V4_SYS_MOD_HDMI,
    V4_SYS_MOD_VOIE,
    V4_SYS_MOD_TDE,
    V4_SYS_MOD_HDR,
    V4_SYS_MOD_PRORES,
    V4_SYS_MOD_VGS,
    V4_SYS_MOD_FD = 47,
    V4_SYS_MOD_ODT,
    V4_SYS_MOD_VQA,
    V4_SYS_MOD_LPR,
	V4_SYS_MOD_SVP_NNIE,
    V4_SYS_MOD_SVP_DSP,
    V4_SYS_MOD_DPU_RECT,
    V4_SYS_MOD_DPU_MATCH,
    V4_SYS_MOD_MOTSNS,
    V4_SYS_MOD_MOTFUS,
    V4_SYS_MOD_GYRODIS,
    V4_SYS_MOD_PM,
    V4_SYS_MOD_SVP_ALG,
    V4_SYS_MOD_IVP,
    V4_SYS_MOD_MCF,
    V4_SYS_MOD_QR,
    V4_SYS_MOD_END
} v4_sys_mod;

typedef enum {
	V4_SYS_OPER_VIOFF_VPSSOFF,
	V4_SYS_OPER_VIOFF_VPSSON,
	V4_SYS_OPER_VION_VPSSOFF,
	V4_SYS_OPER_VION_VPSSON,
	V4_SYS_OPER_VIPARA_VPSSOFF,
	V4_SYS_OPER_VIPARA_VPSSPARA,
	V4_SYS_OPER_END
} v4_sys_oper;

typedef struct {
    v4_sys_mod module;
    int device;
    int channel;
} v4_sys_bind;

typedef struct {
    char version[64];
} v4_sys_ver;

typedef struct {
    void *handle, *handleGoke, *handleVoiceEngine, *handleDnvqe, *handleUpvqe, *handleSecureC;
    
    int (*fnExit)(void);
    int (*fnGetChipId)(unsigned int *chip);
    int (*fnGetVersion)(v4_sys_ver *version);
    
    int (*fnInit)(void);
    int (*fnSetAlignment)(unsigned int *width);

    int (*fnBind)(v4_sys_bind *source, v4_sys_bind *dest);
    int (*fnUnbind)(v4_sys_bind *source, v4_sys_bind *dest);

    int (*fnGetViVpssMode)(v4_sys_oper *mode);
    int (*fnSetViVpssMode)(v4_sys_oper *mode);
} v4_sys_impl;

static int v4_sys_load(v4_sys_impl *sys_lib) {
    if ((!(sys_lib->handleSecureC = dlopen("libsecurec.so", RTLD_LAZY | RTLD_GLOBAL)) ||
         !(sys_lib->handleUpvqe = dlopen("libupvqe.so", RTLD_LAZY | RTLD_GLOBAL)) ||
         !(sys_lib->handleDnvqe = dlopen("libdnvqe.so", RTLD_LAZY | RTLD_GLOBAL))) ||

       ((!(sys_lib->handleVoiceEngine = dlopen("libVoiceEngine.so", RTLD_LAZY | RTLD_GLOBAL)) ||
         !(sys_lib->handle = dlopen("libmpi.so", RTLD_LAZY | RTLD_GLOBAL))) &&

        (!(sys_lib->handleVoiceEngine = dlopen("libvoice_engine.so", RTLD_LAZY | RTLD_GLOBAL)) ||
         !(sys_lib->handleGoke = dlopen("libgk_api.so", RTLD_LAZY | RTLD_GLOBAL)) ||
         !(sys_lib->handle = dlopen("libhi_mpi.so", RTLD_LAZY | RTLD_GLOBAL)))))
        HAL_ERROR("v4_sys", "Failed to load library!\nError: %s\n", dlerror());

    if (!(sys_lib->fnExit = (int(*)(void))
        hal_symbol_load("v4_sys", sys_lib->handle, "HI_MPI_SYS_Exit")))
        return EXIT_FAILURE;

    if (!(sys_lib->fnGetChipId = (int(*)(unsigned int *chip))
        hal_symbol_load("v4_sys", sys_lib->handle, "HI_MPI_SYS_GetChipId")))
        return EXIT_FAILURE;

    if (!(sys_lib->fnGetVersion = (int(*)(v4_sys_ver *version))
        hal_symbol_load("v4_sys", sys_lib->handle, "HI_MPI_SYS_GetVersion")))
        return EXIT_FAILURE;

    if (!(sys_lib->fnInit = (int(*)(void))
        hal_symbol_load("v4_sys", sys_lib->handle, "HI_MPI_SYS_Init")))
        return EXIT_FAILURE;

    if (!(sys_lib->fnSetAlignment = (int(*)(unsigned int *width))
        hal_symbol_load("v4_sys", sys_lib->handle, "HI_MPI_SYS_SetConfig")))
        return EXIT_FAILURE;

    if (!(sys_lib->fnBind = (int(*)(v4_sys_bind *source, v4_sys_bind *dest))
        hal_symbol_load("v4_sys", sys_lib->handle, "HI_MPI_SYS_Bind")))
        return EXIT_FAILURE;

    if (!(sys_lib->fnUnbind = (int(*)(v4_sys_bind *source, v4_sys_bind *dest))
        hal_symbol_load("v4_sys", sys_lib->handle, "HI_MPI_SYS_UnBind")))
        return EXIT_FAILURE;

    if (!(sys_lib->fnGetViVpssMode = (int(*)(v4_sys_oper *mode))
        hal_symbol_load("v4_sys", sys_lib->handle, "HI_MPI_SYS_GetVIVPSSMode")))
        return EXIT_FAILURE;

    if (!(sys_lib->fnSetViVpssMode = (int(*)(v4_sys_oper *mode))
        hal_symbol_load("v4_sys", sys_lib->handle, "HI_MPI_SYS_SetVIVPSSMode")))
        return EXIT_FAILURE;

    return EXIT_SUCCESS;
}

static void v4_sys_unload(v4_sys_impl *sys_lib) {
    if (sys_lib->handleSecureC) dlclose(sys_lib->handleSecureC);
    sys_lib->handleSecureC = NULL;
    if (sys_lib->handleUpvqe) dlclose(sys_lib->handleUpvqe);
    sys_lib->handleUpvqe = NULL;
    if (sys_lib->handleDnvqe) dlclose(sys_lib->handleDnvqe);
    sys_lib->handleDnvqe = NULL;
    if (sys_lib->handleVoiceEngine) dlclose(sys_lib->handleVoiceEngine);
    sys_lib->handleVoiceEngine = NULL;
    if (sys_lib->handleGoke) dlclose(sys_lib->handleGoke);
    sys_lib->handleGoke = NULL;
    if (sys_lib->handle) dlclose(sys_lib->handle);
    sys_lib->handle = NULL;
    memset(sys_lib, 0, sizeof(*sys_lib));
}