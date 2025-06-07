#pragma once

#include "v4_common.h"

typedef enum {
    V4_VB_JPEG_MASK = 0x1,
    V4_VB_ISPINFO_MASK = 0x2,
    V4_VB_MOTIONDATA_MASK = 0x4,
    V4_VB_DNG_MASK = 0x8
} v4_vb_supl;

typedef struct {
    unsigned int count;
    struct {
        unsigned long long blockSize;
        unsigned int blockCnt;
        // Accepts values from 0-2 (none, nocache, cached)
        int rempVirt;
        char heapName[16];
    } comm[16];
} v4_vb_pool;

typedef struct {
    void *handle, *handleGoke;

    int (*fnConfigPool)(v4_vb_pool *config);
    int (*fnConfigSupplement)(v4_vb_supl *value);
    int (*fnExit)(void);
    int (*fnInit)(void);
} v4_vb_impl;

static int v4_vb_load(v4_vb_impl *vb_lib) {
    if ( !(vb_lib->handle = dlopen("libmpi.so", RTLD_LAZY | RTLD_GLOBAL)) &&

        (!(vb_lib->handleGoke = dlopen("libgk_api.so", RTLD_LAZY | RTLD_GLOBAL)) ||
         !(vb_lib->handle = dlopen("libhi_mpi.so", RTLD_LAZY | RTLD_GLOBAL))))
        HAL_ERROR("v4_vb", "Failed to load library!\nError: %s\n", dlerror());

    if (!(vb_lib->fnConfigPool = (int(*)(v4_vb_pool *config))
        hal_symbol_load("v4_vb", vb_lib->handle, "HI_MPI_VB_SetConfig")))
        return EXIT_FAILURE;

    if (!(vb_lib->fnConfigSupplement = (int(*)(v4_vb_supl *value))
        hal_symbol_load("v4_vb", vb_lib->handle, "HI_MPI_VB_SetSupplementConfig")))
        return EXIT_FAILURE;

    if (!(vb_lib->fnExit = (int(*)(void))
        hal_symbol_load("v4_vb", vb_lib->handle, "HI_MPI_VB_Exit")))
        return EXIT_FAILURE;

    if (!(vb_lib->fnInit = (int(*)(void))
        hal_symbol_load("v4_vb", vb_lib->handle, "HI_MPI_VB_Init")))
        return EXIT_FAILURE;

    return EXIT_SUCCESS;
}

static void v4_vb_unload(v4_vb_impl *vb_lib) {
    if (vb_lib->handle) dlclose(vb_lib->handle);
    vb_lib->handle = NULL;
    if (vb_lib->handleGoke) dlclose(vb_lib->handleGoke);
    vb_lib->handleGoke = NULL;
    memset(vb_lib, 0, sizeof(*vb_lib));
}

inline static unsigned int v4_buffer_calculate_vi(
    unsigned int width, unsigned int height, v4_common_pixfmt pixFmt,
    v4_common_compr compr, unsigned int alignWidth)
{
    unsigned int bitWidth;
    unsigned int size = 0, stride = 0;
    unsigned int cmpRatioLine = 1600, cmpRatioFrame = 2000;

    if (!alignWidth)
        alignWidth = 8;
    else if (alignWidth > 1024)
        alignWidth = 1024;
    else
        alignWidth = ALIGN_UP(alignWidth, 8);

    switch (pixFmt) {
        case V4_PIXFMT_RGB_BAYER_8BPP:  bitWidth = 8;  break;
        case V4_PIXFMT_RGB_BAYER_10BPP: bitWidth = 10; break;
        case V4_PIXFMT_RGB_BAYER_12BPP: bitWidth = 12; break;
        case V4_PIXFMT_RGB_BAYER_14BPP: bitWidth = 14; break; 
        case V4_PIXFMT_RGB_BAYER_16BPP: bitWidth = 16; break;
        default: bitWidth = 0; break;
    }

    if (compr == V4_COMPR_NONE) {
        stride = ALIGN_UP(ALIGN_UP(width * bitWidth, 8) / 8,
                     alignWidth);
        size = stride * height;
    } else if (compr == V4_COMPR_LINE) {
        unsigned int temp = ALIGN_UP(
            (16 + width * bitWidth * 1000UL / 
             cmpRatioLine + 8192 + 127) / 128, 2);
        stride = ALIGN_UP(temp * 16, alignWidth);
        size = stride * height;
    } else if (compr == V4_COMPR_FRAME) {
        size = ALIGN_UP(height * width * bitWidth * 1000UL /
                       (cmpRatioFrame * 8), alignWidth);
    }

    return size;
}

inline static unsigned int v4_buffer_calculate_venc(short width, short height, v4_common_pixfmt pixFmt,
    unsigned int alignWidth)
{
    unsigned int bufSize = CEILING_2_POWER(width, alignWidth) *
        CEILING_2_POWER(height, alignWidth) *
        (pixFmt == V4_PIXFMT_YVU422SP ? 2 : 1.5);
    unsigned int headSize = 16 * height;
    if (pixFmt == V4_PIXFMT_YVU422SP || pixFmt >= V4_PIXFMT_RGB_BAYER_8BPP)
        headSize *= 2;
    else if (pixFmt == V4_PIXFMT_YVU420SP)
        headSize *= 3;
        headSize >>= 1;
    return bufSize + headSize;
}