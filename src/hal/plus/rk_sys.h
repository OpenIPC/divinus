#pragma once

#include "rk_common.h"

#define RK_SYS_API "1.0"

typedef enum {
    RK_SYS_MOD_CMPI,
    RK_SYS_MOD_VB,
    RK_SYS_MOD_SYS,
    RK_SYS_MOD_RGN,
    RK_SYS_MOD_VENC,
    RK_SYS_MOD_VDEC,
    RK_SYS_MOD_VPSS,
    RK_SYS_MOD_VGS,
    RK_SYS_MOD_VI,
    RK_SYS_MOD_VO,
    RK_SYS_MOD_AI,
    RK_SYS_MOD_AO,
    RK_SYS_MOD_AENC,
    RK_SYS_MOD_ADEC,
    RK_SYS_MOD_TDE,
    RK_SYS_MOD_ISP,
    RK_SYS_MOD_WBC,
    RK_SYS_MOD_AVS,
    RK_SYS_MOD_RGA,
    RK_SYS_MOD_AF,
    RK_SYS_MOD_IVS,
    RK_SYS_MOD_GPU,
    RK_SYS_MOD_NN,
    RK_SYS_MOD_END
} rk_sys_mod;

typedef struct {
    rk_sys_mod module;
    int device;
    int channel;
} rk_sys_bind;

typedef struct {
    char version[64];
} rk_sys_ver;

typedef struct {
    void *handle;
    
    int (*fnExit)(void);
    int (*fnInit)(void);

    int (*fnBind)(rk_sys_bind *source, rk_sys_bind *dest);
    int (*fnUnbind)(rk_sys_bind *source, rk_sys_bind *dest);
} rk_sys_impl;

static int rk_sys_load(rk_sys_impl *sys_lib) {
    if (!(sys_lib->handle = dlopen("librockit.so", RTLD_LAZY | RTLD_GLOBAL)))
        HAL_ERROR("rk_sys", "Failed to load library!\nError: %s\n", dlerror());

    if (!(sys_lib->fnExit = (int(*)(void))
        hal_symbol_load("rk_sys", sys_lib->handle, "RK_MPI_SYS_Exit")))
        return EXIT_FAILURE;

    if (!(sys_lib->fnInit = (int(*)(void))
        hal_symbol_load("rk_sys", sys_lib->handle, "RK_MPI_SYS_Init")))
        return EXIT_FAILURE;

    if (!(sys_lib->fnBind = (int(*)(rk_sys_bind *source, rk_sys_bind *dest))
        hal_symbol_load("rk_sys", sys_lib->handle, "RK_MPI_SYS_Bind")))
        return EXIT_FAILURE;

    if (!(sys_lib->fnUnbind = (int(*)(rk_sys_bind *source, rk_sys_bind *dest))
        hal_symbol_load("rk_sys", sys_lib->handle, "RK_MPI_SYS_UnBind")))
        return EXIT_FAILURE;

    return EXIT_SUCCESS;
}

static void rk_sys_unload(rk_sys_impl *sys_lib) {
    if (sys_lib->handle) dlclose(sys_lib->handle);
    sys_lib->handle = NULL;
    memset(sys_lib, 0, sizeof(*sys_lib));
}