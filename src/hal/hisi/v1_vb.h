#pragma once

#include "v1_common.h"

typedef struct {
    unsigned int count;
    struct {
        unsigned int blockSize;
        unsigned int blockCnt;
        char heapName[16];
    } comm[16];
} v1_vb_pool;

typedef struct {
    void *handle;

    int (*fnConfigPool)(v1_vb_pool *config);
    int (*fnExit)(void);
    int (*fnInit)(void);
} v1_vb_impl;

static int v1_vb_load(v1_vb_impl *vb_lib) {
    if (!(vb_lib->handle = dlopen("libmpi.so", RTLD_LAZY | RTLD_GLOBAL)))
        HAL_ERROR("v1_vb", "Failed to load library!\nError: %s\n", dlerror());

    if (!(vb_lib->fnConfigPool = (int(*)(v1_vb_pool *config))
        hal_symbol_load("v1_vb", vb_lib->handle, "HI_MPI_VB_SetConf")))
        return EXIT_FAILURE;

    if (!(vb_lib->fnExit = (int(*)(void))
        hal_symbol_load("v1_vb", vb_lib->handle, "HI_MPI_VB_Exit")))
        return EXIT_FAILURE;

    if (!(vb_lib->fnInit = (int(*)(void))
        hal_symbol_load("v1_vb", vb_lib->handle, "HI_MPI_VB_Init")))
        return EXIT_FAILURE;

    return EXIT_SUCCESS;
}

static void v1_vb_unload(v1_vb_impl *vb_lib) {
    if (vb_lib->handle) dlclose(vb_lib->handle);
    vb_lib->handle = NULL;
    memset(vb_lib, 0, sizeof(*vb_lib));
}

inline static unsigned int v1_buffer_calculate_venc(short width, short height, v1_common_pixfmt pixFmt,
    unsigned int alignWidth)
{
    unsigned int bufSize = CEILING_2_POWER(width, alignWidth) *
        CEILING_2_POWER(height, alignWidth) *
        (pixFmt == V1_PIXFMT_YUV422SP ? 2 : 1.5);
    unsigned int headSize = 16 * height;
    if (pixFmt == V1_PIXFMT_YUV422SP)
        headSize *= 2;
    else if (pixFmt == V1_PIXFMT_YUV420SP)
        headSize *= 3;
        headSize >>= 1;
    return bufSize + headSize;
}