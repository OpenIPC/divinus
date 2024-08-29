#pragma once

#include "cvi_common.h"

typedef struct {
    unsigned int count;
    struct {
        unsigned int blockSize;
        unsigned int blockCnt;
        // Accepts values from 0-2 (none, nocache, cached)
        int rempVirt;
        char heapName[32];
    } comm[16];
} cvi_vb_pool;

typedef struct {
    void *handle;

    int (*fnConfigPool)(cvi_vb_pool *config);
    int (*fnExit)(void);
    int (*fnInit)(void);
} cvi_vb_impl;

static int cvi_vb_load(cvi_vb_impl *vb_lib) {
    if (!(vb_lib->handle = dlopen("libsys.so", RTLD_LAZY | RTLD_GLOBAL)))
        HAL_ERROR("cvi_vb", "Failed to load library!\nError: %s\n", dlerror());

    if (!(vb_lib->fnConfigPool = (int(*)(cvi_vb_pool *config))
        hal_symbol_load("cvi_vb", vb_lib->handle, "CVI_VB_SetConfig")))
        return EXIT_FAILURE;

    if (!(vb_lib->fnExit = (int(*)(void))
        hal_symbol_load("cvi_vb", vb_lib->handle, "CVI_VB_Exit")))
        return EXIT_FAILURE;

    if (!(vb_lib->fnInit = (int(*)(void))
        hal_symbol_load("cvi_vb", vb_lib->handle, "CVI_VB_Init")))
        return EXIT_FAILURE;

    return EXIT_SUCCESS;
}

static void cvi_vb_unload(cvi_vb_impl *vb_lib) {
    if (vb_lib->handle) dlclose(vb_lib->handle);
    vb_lib->handle = NULL;
    memset(vb_lib, 0, sizeof(*vb_lib));
}