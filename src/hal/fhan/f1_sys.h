#pragma once

#include "f1_common.h"

#define F1_SYS_API "1.2.0"

typedef struct {
    unsigned int date;      /* 0x20200324 */
    unsigned int commit;    /* git hash */
    unsigned int package;   /* pkg_id, 0x7840d on FH8856 */
    unsigned int reserved;
} f1_sys_ver;

typedef struct {
    unsigned int phys;
    unsigned short *virt;
    unsigned int size;
} f1_sys_buf;

typedef struct {
    void *handle, *handleVmm;

    int (*fnBindVpu2Enc)(unsigned int vpssChn, unsigned int vencChn);
    int (*fnUnbindByDst)(unsigned int vencChn);
    int (*fnExit)(void);
    int (*fnGetVersion)(f1_sys_ver *version);
    int (*fnInit)(void);
    int (*fnAlloc)(f1_sys_buf *buf, unsigned int size, unsigned int align, const char *name);
} f1_sys_impl;

static int f1_sys_load(f1_sys_impl *sys_lib) {
    /* libdsp needs the allocator from libvmm; it must be resolvable globally */
    if (!(sys_lib->handleVmm = dlopen("libvmm.so", RTLD_NOW | RTLD_GLOBAL)))
        HAL_ERROR("f1_sys", "Failed to load library!\nError: %s\n", dlerror());

    if (!(sys_lib->handle = dlopen("libdsp.so", RTLD_NOW | RTLD_GLOBAL)))
        HAL_ERROR("f1_sys", "Failed to load library!\nError: %s\n", dlerror());

    sys_lib->fnAlloc = (int(*)(f1_sys_buf *buf, unsigned int size, unsigned int align, const char *name))
        dlsym(sys_lib->handleVmm, "buffer_malloc_withname");

    if (!(sys_lib->fnBindVpu2Enc = (int(*)(unsigned int vpssChn, unsigned int vencChn))
        hal_symbol_load("f1_sys", sys_lib->handle, "FH_SYS_BindVpu2Enc")))
        return EXIT_FAILURE;

    if (!(sys_lib->fnUnbindByDst = (int(*)(unsigned int vencChn))
        hal_symbol_load("f1_sys", sys_lib->handle, "FH_SYS_UnBindbyDst")))
        return EXIT_FAILURE;

    if (!(sys_lib->fnExit = (int(*)(void))
        hal_symbol_load("f1_sys", sys_lib->handle, "FH_SYS_Exit")))
        return EXIT_FAILURE;

    if (!(sys_lib->fnGetVersion = (int(*)(f1_sys_ver *version))
        hal_symbol_load("f1_sys", sys_lib->handle, "FH_SYS_GetVersion")))
        return EXIT_FAILURE;

    if (!(sys_lib->fnInit = (int(*)(void))
        hal_symbol_load("f1_sys", sys_lib->handle, "FH_SYS_Init")))
        return EXIT_FAILURE;

    return EXIT_SUCCESS;
}

static void f1_sys_unload(f1_sys_impl *sys_lib) {
    if (sys_lib->handle) dlclose(sys_lib->handle);
    sys_lib->handle = NULL;
    if (sys_lib->handleVmm) dlclose(sys_lib->handleVmm);
    sys_lib->handleVmm = NULL;
    memset(sys_lib, 0, sizeof(*sys_lib));
}
