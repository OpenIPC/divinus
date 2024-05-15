#pragma once

#include "v3_common.h"

typedef struct {
    v3_common_rect capt;
    float framerate;
    v3_common_bayer bayer;
} v3_isp_dev;

typedef struct {
    void *handle;

    int (*fnExit)(int device);
    int (*fnInit)(int device);
    int (*fnMemInit)(int device);
    int (*fnRun)(int device);

    int (*fnSetDeviceConfig)(int device, v3_isp_dev *config);
    int (*fnSetWDRMode)(int device, v3_common_wdr *mode);
} v3_isp_impl;

int v3_isp_load(v3_isp_impl *isp_lib) {
    if (!(isp_lib->handle = dlopen("libmpi.so", RTLD_NOW))) {
        fprintf(stderr, "[v3_isp] Failed to load library!\n");
        return EXIT_FAILURE;
    }

    if (!(isp_lib->fnExit = (int(*)(int device))
        dlsym(isp_lib->handle, "HI_MPI_ISP_Exit"))) {
        fprintf(stderr, "[v3_isp] Failed to acquire symbol HI_MPI_ISP_Exit!\n");
        return EXIT_FAILURE;
    }

    if (!(isp_lib->fnInit = (int(*)(int device))
        dlsym(isp_lib->handle, "HI_MPI_ISP_Init"))) {
        fprintf(stderr, "[v3_isp] Failed to acquire symbol HI_MPI_ISP_Init!\n");
        return EXIT_FAILURE;
    }

    if (!(isp_lib->fnMemInit = (int(*)(int device))
        dlsym(isp_lib->handle, "HI_MPI_ISP_MemInit"))) {
        fprintf(stderr, "[v3_isp] Failed to acquire symbol HI_MPI_ISP_MemInit!\n");
        return EXIT_FAILURE;
    }

    if (!(isp_lib->fnRun = (int(*)(int device))
        dlsym(isp_lib->handle, "HI_MPI_ISP_Run"))) {
        fprintf(stderr, "[v3_isp] Failed to acquire symbol HI_MPI_ISP_Run!\n");
        return EXIT_FAILURE;
    }

    if (!(isp_lib->fnSetDeviceConfig = (int(*)(int device, v3_isp_dev *config))
        dlsym(isp_lib->handle, "HI_MPI_ISP_SetPubAttr"))) {
        fprintf(stderr, "[v3_isp] Failed to acquire symbol HI_MPI_ISP_SetPubAttr!\n");
        return EXIT_FAILURE;
    }

    if (!(isp_lib->fnSetWDRMode = (int(*)(int device, v3_common_wdr *mode))
        dlsym(isp_lib->handle, "HI_MPI_ISP_SetWDRMode"))) {
        fprintf(stderr, "[v3_isp] Failed to acquire symbol HI_MPI_ISP_SetWDRMode!\n");
        return EXIT_FAILURE;
    }
}

void v3_isp_unload(v3_isp_impl *isp_lib) {
    if (isp_lib->handle)
        dlclose(isp_lib->handle = NULL);
    memset(isp_lib, 0, sizeof(*isp_lib));
}