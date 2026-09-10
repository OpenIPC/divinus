#pragma once

#include "fh_common.h"

#define FH_SYS_API "1.2.0"

typedef struct {
    unsigned int date;      /* 0x20200324 */
    unsigned int commit;    /* git hash */
    unsigned int package;   /* pkg_id, 0x7840d on FH8856 */
    unsigned int reserved;
} fh_sys_ver;

typedef struct {
    void *handle, *handleVmm;

    int (*fnBindVpu2Enc)(unsigned int vpssChn, unsigned int vencChn);
    int (*fnUnbindByDst)(unsigned int vencChn);
    int (*fnExit)(void);
    int (*fnGetVersion)(fh_sys_ver *version);
    int (*fnInit)(void);
} fh_sys_impl;

static int fh_sys_load(fh_sys_impl *sys_lib) {
    /* libdsp needs the allocator from libvmm; it must be resolvable globally */
    if (!(sys_lib->handleVmm = dlopen("libvmm.so", RTLD_NOW | RTLD_GLOBAL)))
        HAL_ERROR("fh_sys", "Failed to load library!\nError: %s\n", dlerror());

    if (!(sys_lib->handle = dlopen("libdsp.so", RTLD_NOW | RTLD_GLOBAL)))
        HAL_ERROR("fh_sys", "Failed to load library!\nError: %s\n", dlerror());

    if (!(sys_lib->fnBindVpu2Enc = (int(*)(unsigned int vpssChn, unsigned int vencChn))
        hal_symbol_load("fh_sys", sys_lib->handle, "FH_SYS_BindVpu2Enc")))
        return EXIT_FAILURE;

    if (!(sys_lib->fnUnbindByDst = (int(*)(unsigned int vencChn))
        hal_symbol_load("fh_sys", sys_lib->handle, "FH_SYS_UnBindbyDst")))
        return EXIT_FAILURE;

    if (!(sys_lib->fnExit = (int(*)(void))
        hal_symbol_load("fh_sys", sys_lib->handle, "FH_SYS_Exit")))
        return EXIT_FAILURE;

    if (!(sys_lib->fnGetVersion = (int(*)(fh_sys_ver *version))
        hal_symbol_load("fh_sys", sys_lib->handle, "FH_SYS_GetVersion")))
        return EXIT_FAILURE;

    if (!(sys_lib->fnInit = (int(*)(void))
        hal_symbol_load("fh_sys", sys_lib->handle, "FH_SYS_Init")))
        return EXIT_FAILURE;

    return EXIT_SUCCESS;
}

static void fh_sys_unload(fh_sys_impl *sys_lib) {
    if (sys_lib->handle) dlclose(sys_lib->handle);
    sys_lib->handle = NULL;
    if (sys_lib->handleVmm) dlclose(sys_lib->handleVmm);
    sys_lib->handleVmm = NULL;
    memset(sys_lib, 0, sizeof(*sys_lib));
}
