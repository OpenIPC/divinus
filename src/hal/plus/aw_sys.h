#pragma once

#include "aw_common.h"

#define AW_SYS_API "1.0"

typedef enum {
    AW_SYS_MOD_CMPI,
    AW_SYS_MOD_VB,
    AW_SYS_MOD_SYS,
    AW_SYS_MOD_RGN,
    AW_SYS_MOD_CHNL,
    AW_SYS_MOD_VDEC,
    AW_SYS_MOD_GROUP,
    AW_SYS_MOD_VPSS,
    AW_SYS_MOD_VENC,
    AW_SYS_MOD_VDA,
    AW_SYS_MOD_H264E,
    AW_SYS_MOD_JPEGE,
    AW_SYS_MOD_MPEG4E,
    AW_SYS_MOD_H264D,
    AW_SYS_MOD_JPEGD,
    AW_SYS_MOD_VOU,
    AW_SYS_MOD_VIU,
    AW_SYS_MOD_DSU,
    AW_SYS_MOD_VALG,
    AW_SYS_MOD_RC,
    AW_SYS_MOD_AIO,
    AW_SYS_MOD_AI,
    AW_SYS_MOD_AO,
    AW_SYS_MOD_AENC,
    AW_SYS_MOD_ADEC,
    AW_SYS_MOD_AVENC,
    AW_SYS_MOD_PCIV,
    AW_SYS_MOD_PCIVFMW,
    AW_SYS_MOD_ISP,
    AW_SYS_MOD_IVE,
    AW_SYS_MOD_UVC,
    AW_SYS_MOD_DCCM,
    AW_SYS_MOD_DCCS,
    AW_SYS_MOD_PROC,
    AW_SYS_MOD_LOG,
    AW_SYS_MOD_MSTLOG,
    AW_SYS_MOD_VD,
    AW_SYS_MOD_VCMP = 38,
    AW_SYS_MOD_FB,
    AW_SYS_MOD_HDMI,
    AW_SYS_MOD_VOIE,
    AW_SYS_MOD_TDE,
    AW_SYS_MOD_USR,
    AW_SYS_MOD_VEDU,
    AW_SYS_MOD_VGS,
    AW_SYS_MOD_H265E,
    AW_SYS_MOD_FD,
    AW_SYS_MOD_ODT,
    AW_SYS_MOD_VQA,
    AW_SYS_MOD_LPR,
    AW_SYS_MOD_COMPCORE = 100,
    AW_SYS_MOD_DEMUX,
    AW_SYS_MOD_MUX,
    AW_SYS_MOD_CLOCK,
    AW_SYS_MOD_CSI,
    AW_SYS_MOD_ISE = 229,
    AW_SYS_MOD_EIS,
    AW_SYS_MOD_TENC,
    AW_SYS_MOD_END
} aw_sys_mod;

typedef struct {
    aw_sys_mod module;
    int device;
    int channel;
} aw_sys_bind;

typedef struct {
    char version[64];
} aw_sys_ver;

typedef struct {
    void *handle;
    
    int (*fnExit)(void);
    int (*fnInit)(void);

    int (*fnBind)(aw_sys_bind *source, aw_sys_bind *dest);
    int (*fnUnbind)(aw_sys_bind *source, aw_sys_bind *dest);
} aw_sys_impl;

static int aw_sys_load(aw_sys_impl *sys_lib) {
    if (!(sys_lib->handle = dlopen("libaw_mpp.so", RTLD_LAZY | RTLD_GLOBAL)))
        HAL_ERROR("aw_sys", "Failed to load library!\nError: %s\n", dlerror());

    if (!(sys_lib->fnExit = (int(*)(void))
        hal_symbol_load("aw_sys", sys_lib->handle, "AW_MPI_SYS_Exit")))
        return EXIT_FAILURE;

    if (!(sys_lib->fnInit = (int(*)(void))
        hal_symbol_load("aw_sys", sys_lib->handle, "AW_MPI_SYS_Init")))
        return EXIT_FAILURE;

    if (!(sys_lib->fnBind = (int(*)(aw_sys_bind *source, aw_sys_bind *dest))
        hal_symbol_load("aw_sys", sys_lib->handle, "AW_MPI_SYS_Bind")))
        return EXIT_FAILURE;

    if (!(sys_lib->fnUnbind = (int(*)(aw_sys_bind *source, aw_sys_bind *dest))
        hal_symbol_load("aw_sys", sys_lib->handle, "AW_MPI_SYS_UnBind")))
        return EXIT_FAILURE;

    return EXIT_SUCCESS;
}

static void aw_sys_unload(aw_sys_impl *sys_lib) {
    if (sys_lib->handle) dlclose(sys_lib->handle);
    sys_lib->handle = NULL;
    memset(sys_lib, 0, sizeof(*sys_lib));
}