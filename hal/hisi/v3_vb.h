#pragma once

#include "v3_common.h"

typedef enum {
    V3_VB_JPEG_MASK,
    V3_VB_USERINFO_MASK,
    V3_VB_ISPINFO_MASK,
    V3_VB_ISPSTAT_MASK,
    V3_VB_DNG_MASK
} v3_vb_supl;

typedef struct {
    unsigned int count;
    struct {
        unsigned int blockSize;
        unsigned int blockCnt;
        char heapName[16];
        int rempVirtOn;
    } comm[16];
} v3_vb_pool;

typedef struct {
    void *handle;

    int (*fnConfigPool)(v3_vb_pool *config);
    int (*fnConfigSupplement)(v3_vb_supl *value);
    int (*fnExit)(void);
    int (*fnInit)(void);
} v3_vb_impl;

int v3_vb_load(v3_vb_impl *vb_lib) {
    if (!(vb_lib->handle = dlopen("libmpi.so", RTLD_NOW))) {
        fprintf(stderr, "[v3_sys] Failed to load library!\n");
        return EXIT_FAILURE;
    }

    if (!(vb_lib->fnConfigPool = (int(*)(v3_vb_pool *config))
        dlsym(vb_lib->handle, "HI_MPI_VB_SetConf"))) {
        fprintf(stderr, "[v3_sys] Failed to acquire symbol HI_MPI_VB_SetConf!\n");
        return EXIT_FAILURE;
    }

    if (!(vb_lib->fnConfigSupplement = (int(*)(v3_vb_supl *value))
        dlsym(vb_lib->handle, "HI_MPI_VB_SetSupplementConf"))) {
        fprintf(stderr, "[v3_sys] Failed to acquire symbol HI_MPI_VB_SetSupplementConf!\n");
        return EXIT_FAILURE;
    }

    if (!(vb_lib->fnExit = (int(*)(void))
        dlsym(vb_lib->handle, "HI_MPI_VB_Exit"))) {
        fprintf(stderr, "[v3_sys] Failed to acquire symbol HI_MPI_VB_Exit!\n");
        return EXIT_FAILURE;
    }

    if (!(vb_lib->fnInit = (int(*)(void))
        dlsym(vb_lib->handle, "HI_MPI_VB_Init"))) {
        fprintf(stderr, "[v3_sys] Failed to acquire symbol HI_MPI_VB_Init!\n");
        return EXIT_FAILURE;
    }
}

void v3_vb_unload(v3_vb_impl *vb_lib) {
    if (vb_lib->handle)
        dlclose(vb_lib->handle = NULL);
    memset(vb_lib, 0, sizeof(*vb_lib));
}