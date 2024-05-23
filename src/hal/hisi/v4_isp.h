#pragma once

#include "v4_common.h"

typedef enum {
    V4_ISP_DIR_NORMAL,
    V4_ISP_DIR_MIRROR,
    V4_ISP_DIR_FLIP,
    V4_ISP_DIR_MIRROR_FLIP,
    V4_ISP_DIR_END
} v4_isp_dir;

typedef struct {
    int id;
    char libName[20];
} v4_isp_alg;

typedef struct {
    v4_common_rect capt;
    v4_common_dim size;
    float framerate;
    v4_common_bayer bayer;
    v4_common_wdr wdr;
    unsigned char mode;
} v4_isp_dev;

typedef struct {
    void *handle, *handleDehaze, *handleDrc, *handleLdci, *handleIrAuto, *handleAwb, 
        *handleGokeAwb, *handleAe, *handleGokeAe,  *handleGoke;

    int (*fnExit)(int pipe);
    int (*fnInit)(int pipe);
    int (*fnMemInit)(int pipe);
    int (*fnRun)(int pipe);

    int (*fnSetDeviceConfig)(int pipe, v4_isp_dev *config);

    int (*fnRegisterAE)(int pipe, v4_isp_alg *library);
    int (*fnRegisterAWB)(int pipe, v4_isp_alg *library);
    int (*fnUnregisterAE)(int pipe, v4_isp_alg *library);
    int (*fnUnregisterAWB)(int pipe, v4_isp_alg *library);
} v4_isp_impl;

static int v4_isp_load(v4_isp_impl *isp_lib) {
    if ((!(isp_lib->handleAe = dlopen("lib_hiae.so", RTLD_LAZY | RTLD_GLOBAL)) ||
         !(isp_lib->handleAwb = dlopen("lib_hiawb.so", RTLD_LAZY | RTLD_GLOBAL)) ||
         !(isp_lib->handleLdci = dlopen("lib_hildci.so", RTLD_LAZY | RTLD_GLOBAL)) ||
         !(isp_lib->handleDehaze = dlopen("lib_hidehaze.so", RTLD_LAZY | RTLD_GLOBAL)) ||
         !(isp_lib->handleDrc = dlopen("lib_hidrc.so", RTLD_LAZY | RTLD_GLOBAL)) ||
         !(isp_lib->handle = dlopen("libisp.so", RTLD_LAZY | RTLD_GLOBAL))) &&

        (!(isp_lib->handleGoke = dlopen("libgk_isp.so", RTLD_LAZY | RTLD_GLOBAL)) ||
         !(isp_lib->handleGokeAe = dlopen("libgk_ae.so", RTLD_LAZY | RTLD_GLOBAL)) ||
         !(isp_lib->handleAe = dlopen("libhi_ae.so", RTLD_LAZY | RTLD_GLOBAL)) ||
         !(isp_lib->handleGokeAwb = dlopen("libgk_awb.so", RTLD_LAZY | RTLD_GLOBAL)) ||
         !(isp_lib->handleAwb = dlopen("libhi_awb.so", RTLD_LAZY | RTLD_GLOBAL)) ||
         !(isp_lib->handleIrAuto = dlopen("libir_auto.so", RTLD_LAZY | RTLD_GLOBAL)) ||
         !(isp_lib->handleLdci = dlopen("libldci.so", RTLD_LAZY | RTLD_GLOBAL)) ||
         !(isp_lib->handleDrc = dlopen("libdrc.so", RTLD_LAZY | RTLD_GLOBAL)) ||
         !(isp_lib->handleDehaze = dlopen("libdehaze.so", RTLD_LAZY | RTLD_GLOBAL)) ||
         !(isp_lib->handle = dlopen("libhi_isp.so", RTLD_LAZY | RTLD_GLOBAL)))) {
        fprintf(stderr, "[v4_isp] Failed to load library!\nError: %s\n", dlerror());
        return EXIT_FAILURE;
    }

    if (!(isp_lib->fnExit = (int(*)(int pipe))
        dlsym(isp_lib->handle, "HI_MPI_ISP_Exit"))) {
        fprintf(stderr, "[v4_isp] Failed to acquire symbol HI_MPI_ISP_Exit!\n");
        return EXIT_FAILURE;
    }

    if (!(isp_lib->fnInit = (int(*)(int pipe))
        dlsym(isp_lib->handle, "HI_MPI_ISP_Init"))) {
        fprintf(stderr, "[v4_isp] Failed to acquire symbol HI_MPI_ISP_Init!\n");
        return EXIT_FAILURE;
    }

    if (!(isp_lib->fnMemInit = (int(*)(int pipe))
        dlsym(isp_lib->handle, "HI_MPI_ISP_MemInit"))) {
        fprintf(stderr, "[v4_isp] Failed to acquire symbol HI_MPI_ISP_MemInit!\n");
        return EXIT_FAILURE;
    }

    if (!(isp_lib->fnRun = (int(*)(int pipe))
        dlsym(isp_lib->handle, "HI_MPI_ISP_Run"))) {
        fprintf(stderr, "[v4_isp] Failed to acquire symbol HI_MPI_ISP_Run!\n");
        return EXIT_FAILURE;
    }

    if (!(isp_lib->fnSetDeviceConfig = (int(*)(int pipe, v4_isp_dev *config))
        dlsym(isp_lib->handle, "HI_MPI_ISP_SetPubAttr"))) {
        fprintf(stderr, "[v4_isp] Failed to acquire symbol HI_MPI_ISP_SetPubAttr!\n");
        return EXIT_FAILURE;
    }

    if (!(isp_lib->fnRegisterAE = (int(*)(int pipe, v4_isp_alg *library))
        dlsym(isp_lib->handleAe, "HI_MPI_AE_Register"))) {
        fprintf(stderr, "[v4_isp] Failed to acquire symbol HI_MPI_AE_Register!\n");
        return EXIT_FAILURE;
    }

    if (!(isp_lib->fnRegisterAWB = (int(*)(int pipe, v4_isp_alg *library))
        dlsym(isp_lib->handleAwb, "HI_MPI_AWB_Register"))) {
        fprintf(stderr, "[v4_isp] Failed to acquire symbol HI_MPI_AWB_Register!\n");
        return EXIT_FAILURE;
    }

    if (!(isp_lib->fnUnregisterAE = (int(*)(int pipe, v4_isp_alg *library))
        dlsym(isp_lib->handleAe, "HI_MPI_AE_UnRegister"))) {
        fprintf(stderr, "[v4_isp] Failed to acquire symbol HI_MPI_AE_UnRegister!\n");
        return EXIT_FAILURE;
    }

    if (!(isp_lib->fnUnregisterAWB = (int(*)(int pipe, v4_isp_alg *library))
        dlsym(isp_lib->handleAwb, "HI_MPI_AWB_UnRegister"))) {
        fprintf(stderr, "[v4_isp] Failed to acquire symbol HI_MPI_AWB_UnRegister!\n");
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}

static void v4_isp_unload(v4_isp_impl *isp_lib) {
    if (isp_lib->handleGoke) dlclose(isp_lib->handleGoke);
    isp_lib->handleGoke = NULL;
    if (isp_lib->handleGokeAe) dlclose(isp_lib->handleGokeAe);
    isp_lib->handleGokeAe = NULL;
    if (isp_lib->handleAe) dlclose(isp_lib->handleAe);
    isp_lib->handleAe = NULL;
    if (isp_lib->handleGokeAwb) dlclose(isp_lib->handleGokeAwb);
    isp_lib->handleGokeAwb = NULL;
    if (isp_lib->handleAwb) dlclose(isp_lib->handleAwb);
    isp_lib->handleAwb = NULL;
    if (isp_lib->handleIrAuto) dlclose(isp_lib->handleIrAuto);
    isp_lib->handleIrAuto = NULL;
    if (isp_lib->handleLdci) dlclose(isp_lib->handleLdci);
    isp_lib->handleLdci = NULL;
    if (isp_lib->handleDrc) dlclose(isp_lib->handleDrc);
    isp_lib->handleDrc = NULL;
    if (isp_lib->handleDehaze) dlclose(isp_lib->handleDehaze);
    isp_lib->handleDehaze = NULL;
    if (isp_lib->handle) dlclose(isp_lib->handle);
    isp_lib->handle = NULL;
    memset(isp_lib, 0, sizeof(*isp_lib));
}