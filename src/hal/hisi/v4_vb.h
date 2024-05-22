#pragma once

#include "v4_common.h"

typedef enum {
    V4_VB_JPEG_MASK,
    V4_VB_USERINFO_MASK,
    V4_VB_ISPINFO_MASK,
    V4_VB_ISPSTAT_MASK,
    V4_VB_DNG_MASK
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
         !(vb_lib->handle = dlopen("libhi_mpi.so", RTLD_LAZY | RTLD_GLOBAL)))) {
        fprintf(stderr, "[v4_vb] Failed to load library!\nError: %s\n", dlerror());
        return EXIT_FAILURE;
    }

    if (!(vb_lib->fnConfigPool = (int(*)(v4_vb_pool *config))
        dlsym(vb_lib->handle, "HI_MPI_VB_SetConfig"))) {
        fprintf(stderr, "[v4_vb] Failed to acquire symbol HI_MPI_VB_SetConfig!\n");
        return EXIT_FAILURE;
    }

    if (!(vb_lib->fnConfigSupplement = (int(*)(v4_vb_supl *value))
        dlsym(vb_lib->handle, "HI_MPI_VB_SetSupplementConfig"))) {
        fprintf(stderr, "[v4_vb] Failed to acquire symbol HI_MPI_VB_SetSupplementConfig!\n");
        return EXIT_FAILURE;
    }

    if (!(vb_lib->fnExit = (int(*)(void))
        dlsym(vb_lib->handle, "HI_MPI_VB_Exit"))) {
        fprintf(stderr, "[v4_vb] Failed to acquire symbol HI_MPI_VB_Exit!\n");
        return EXIT_FAILURE;
    }

    if (!(vb_lib->fnInit = (int(*)(void))
        dlsym(vb_lib->handle, "HI_MPI_VB_Init"))) {
        fprintf(stderr, "[v4_vb] Failed to acquire symbol HI_MPI_VB_Init!\n");
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}

static void v4_vb_unload(v4_vb_impl *vb_lib) {
    if (vb_lib->handle) dlclose(vb_lib->handle);
    vb_lib->handle = NULL;
    if (vb_lib->handleGoke) dlclose(vb_lib->handleGoke);
    vb_lib->handleGoke = NULL;
    memset(vb_lib, 0, sizeof(*vb_lib));
}