#pragma once

#include "rk_common.h"

typedef struct {
    unsigned int count;
    struct {
        unsigned long long blockSize;
        unsigned int blockCnt;
        // Accepts values from 0-2 (none, nocache, cached)
        int rempVirt;
        char heapName[16];
    } comm[16];
} rk_vb_pool;

typedef struct {
    void *handle;

    int (*fnConfigPool)(rk_vb_pool *config);
    int (*fnConfigSupplement)(rk_vb_supl *value);
    int (*fnExit)(void);
    int (*fnInit)(void);
} rk_vb_impl;

static int rk_vb_load(rk_vb_impl *vb_lib) {
    if (!(vb_lib->handle = dlopen("librockit.so", RTLD_LAZY | RTLD_GLOBAL))) {
        fprintf(stderr, "[rk_vb] Failed to load library!\nError: %s\n", dlerror());
        return EXIT_FAILURE;
    }

    if (!(vb_lib->fnConfigPool = (int(*)(rk_vb_pool *config))
        dlsym(vb_lib->handle, "RK_MPI_VB_SetConfig"))) {
        fprintf(stderr, "[rk_vb] Failed to acquire symbol RK_MPI_VB_SetConfig!\n");
        return EXIT_FAILURE;
    }

    if (!(vb_lib->fnExit = (int(*)(void))
        dlsym(vb_lib->handle, "RK_MPI_VB_Exit"))) {
        fprintf(stderr, "[rk_vb] Failed to acquire symbol RK_MPI_VB_Exit!\n");
        return EXIT_FAILURE;
    }

    if (!(vb_lib->fnInit = (int(*)(void))
        dlsym(vb_lib->handle, "RK_MPI_VB_Init"))) {
        fprintf(stderr, "[rk_vb] Failed to acquire symbol RK_MPI_VB_Init!\n");
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}

static void rk_vb_unload(rk_vb_impl *vb_lib) {
    if (vb_lib->handle) dlclose(vb_lib->handle);
    vb_lib->handle = NULL;
    memset(vb_lib, 0, sizeof(*vb_lib));
}

inline static unsigned int rk_buffer_calculate_vi(
    unsigned int width, unsigned int height, rk_common_pixfmt pixFmt,
	rk_common_compr compr, unsigned int alignWidth)
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
	    case RK_PIXFMT_RGB_BAYER_8BPP:  bitWidth = 8;  break;
	    case RK_PIXFMT_RGB_BAYER_10BPP: bitWidth = 10; break;
	    case RK_PIXFMT_RGB_BAYER_12BPP: bitWidth = 12; break;
	    case RK_PIXFMT_RGB_BAYER_14BPP: bitWidth = 14; break; 
	    case RK_PIXFMT_RGB_BAYER_16BPP: bitWidth = 16; break;
	    default: bitWidth = 0; break;
	}

	if (compr == RK_COMPR_NONE) {
		stride = ALIGN_UP(ALIGN_UP(width * bitWidth, 8) / 8,
				     alignWidth);
		size = stride * height;
	} else if (compr == RK_COMPR_LINE) {
		unsigned int temp = ALIGN_UP(
			(16 + width * bitWidth * 1000UL / 
             cmpRatioLine + 8192 + 127) / 128, 2);
		stride = ALIGN_UP(temp * 16, alignWidth);
		size = stride * height;
	} else if (compr == RK_COMPR_FRAME) {
		size = ALIGN_UP(height * width * bitWidth * 1000UL /
					   (cmpRatioFrame * 8), alignWidth);
	}

	return size;
}

inline static unsigned int rk_buffer_calculate_venc(short width, short height, rk_common_pixfmt pixFmt,
    unsigned int alignWidth)
{
    unsigned int bufSize = CEILING_2_POWER(width, alignWidth) *
        CEILING_2_POWER(height, alignWidth) *
        (pixFmt == RK_PIXFMT_YVU422SP ? 2 : 1.5);
    unsigned int headSize = 16 * height;
    if (pixFmt == RK_PIXFMT_YVU422SP || pixFmt >= RK_PIXFMT_RGB_BAYER_8BPP)
        headSize *= 2;
    else if (pixFmt == RK_PIXFMT_YVU420SP)
        headSize *= 3;
        headSize >>= 1;
    return bufSize + headSize;
}