#pragma once

#include "f2_common.h"

#define F2_SYS_API "1.0.0"

typedef struct {
    unsigned int date;
    unsigned int commit;    /* git hash */
    unsigned int package;   /* pkg_id */
    unsigned int reserved;
} f2_sys_ver;

typedef struct {
    void *handle, *handleVmm;

    int (*fnBind)(unsigned int vpssChn, unsigned int vencChn);
    int (*fnUnbind)(unsigned int vencChn);
    int (*fnExit)(void);
    int (*fnGetChipId)(unsigned int *id);
    int (*fnGetVersion)(f2_sys_ver *version);
    int (*fnInit)(void);
} f2_sys_impl;

static int f2_sys_load(f2_sys_impl *sys_lib) {
    /* libdsp needs the allocator from libvmm; it must be resolvable globally */
    if (!(sys_lib->handleVmm = dlopen("libvmm.so", RTLD_NOW | RTLD_GLOBAL)))
        HAL_ERROR("f2_sys", "Failed to load library!\nError: %s\n", dlerror());

    if (!(sys_lib->handle = dlopen("libdsp.so", RTLD_NOW | RTLD_GLOBAL)))
        HAL_ERROR("f2_sys", "Failed to load library!\nError: %s\n", dlerror());

    if (!(sys_lib->fnBind = (int(*)(unsigned int, unsigned int))
        hal_symbol_load("f2_sys", sys_lib->handle, "FH_SYS_BindVpu2Enc")))
        return EXIT_FAILURE;

    if (!(sys_lib->fnUnbind = (int(*)(unsigned int))
        hal_symbol_load("f2_sys", sys_lib->handle, "FH_SYS_UnBindbyDst")))
        return EXIT_FAILURE;

    if (!(sys_lib->fnExit = (int(*)(void))
        hal_symbol_load("f2_sys", sys_lib->handle, "FH_SYS_Exit")))
        return EXIT_FAILURE;

    sys_lib->fnGetChipId = (int(*)(unsigned int*))dlsym(sys_lib->handle, "FH_SYS_GetChipID");

    if (!(sys_lib->fnGetVersion = (int(*)(f2_sys_ver *))
        hal_symbol_load("f2_sys", sys_lib->handle, "FH_SYS_GetVersion")))
        return EXIT_FAILURE;

    if (!(sys_lib->fnInit = (int(*)(void))
        hal_symbol_load("f2_sys", sys_lib->handle, "FH_SYS_Init")))
        return EXIT_FAILURE;

    return EXIT_SUCCESS;
}

static void f2_sys_unload(f2_sys_impl *sys_lib) {
    if (sys_lib->handle) dlclose(sys_lib->handle);
    sys_lib->handle = NULL;
    if (sys_lib->handleVmm) dlclose(sys_lib->handleVmm);
    sys_lib->handleVmm = NULL;
    memset(sys_lib, 0, sizeof(*sys_lib));
}
