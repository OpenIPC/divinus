#pragma once

#include "v3_common.h"

extern int (*fnISP_AlgRegisterDehaze)(int);
extern int (*fnISP_AlgRegisterDrc)(int);

typedef struct {
    int id;
    char libName[20];
} v3_isp_alg;

typedef struct {
    v3_common_rect capt;
    float framerate;
    v3_common_bayer bayer;
} v3_isp_dev;

typedef struct {
    void *handle, *handleAwb, *handleAe, *handleDefog, *handleIrAuto;

    int (*fnExit)(int device);
    int (*fnInit)(int device);
    int (*fnMemInit)(int device);
    int (*fnRun)(int device);

    int (*fnSetDeviceConfig)(int device, v3_isp_dev *config);
    int (*fnSetWDRMode)(int device, v3_common_wdr *mode);

    int (*fnRegisterAE)(int device, v3_isp_alg *library);
    int (*fnRegisterAWB)(int device, v3_isp_alg *library);
    int (*fnUnregisterAE)(int device, v3_isp_alg *library);
    int (*fnUnregisterAWB)(int device, v3_isp_alg *library);
} v3_isp_impl;

static int v3_isp_load(v3_isp_impl *isp_lib) {
    if (!(isp_lib->handle = dlopen("libisp.so", RTLD_LAZY | RTLD_GLOBAL)) ||
        !(isp_lib->handleAe = dlopen("lib_hiae.so", RTLD_LAZY | RTLD_GLOBAL)) ||
        !(isp_lib->handleAwb = dlopen("lib_hiawb.so", RTLD_LAZY | RTLD_GLOBAL)) ||
        !(isp_lib->handleDefog = dlopen("lib_hidefog.so", RTLD_LAZY | RTLD_GLOBAL)) ||
        !(isp_lib->handleIrAuto = dlopen("lib_hiirauto.so", RTLD_LAZY | RTLD_GLOBAL))) {
        fprintf(stderr, "[v3_isp] Failed to load library!\nError: %s\n", dlerror());
        return EXIT_FAILURE;
    }

    if (!(fnISP_AlgRegisterDehaze = (int(*)(int))
        dlsym(isp_lib->handleDefog, "ISP_AlgRegisterDehaze"))) {
        fprintf(stderr, "[v4_isp] Failed to acquire symbol ISP_AlgRegisterDehaze!\n");
        return EXIT_FAILURE;
    }

    if (!(fnISP_AlgRegisterDrc = (int(*)(int))
        dlsym(isp_lib->handle, "ISP_AlgRegisterDrc"))) {
        fprintf(stderr, "[v4_isp] Failed to acquire symbol ISP_AlgRegisterDrc!\n");
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

    if (!(isp_lib->fnRegisterAE = (int(*)(int device, v3_isp_alg *library))
        dlsym(isp_lib->handleAe, "HI_MPI_AE_Register"))) {
        fprintf(stderr, "[v3_isp] Failed to acquire symbol HI_MPI_AE_Register!\n");
        return EXIT_FAILURE;
    }

    if (!(isp_lib->fnRegisterAWB = (int(*)(int device, v3_isp_alg *library))
        dlsym(isp_lib->handleAwb, "HI_MPI_AWB_Register"))) {
        fprintf(stderr, "[v3_isp] Failed to acquire symbol HI_MPI_AWB_Register!\n");
        return EXIT_FAILURE;
    }

    if (!(isp_lib->fnUnregisterAE = (int(*)(int device, v3_isp_alg *library))
        dlsym(isp_lib->handleAe, "HI_MPI_AE_UnRegister"))) {
        fprintf(stderr, "[v3_isp] Failed to acquire symbol HI_MPI_AE_UnRegister!\n");
        return EXIT_FAILURE;
    }

    if (!(isp_lib->fnUnregisterAWB = (int(*)(int device, v3_isp_alg *library))
        dlsym(isp_lib->handleAwb, "HI_MPI_AWB_UnRegister"))) {
        fprintf(stderr, "[v3_isp] Failed to acquire symbol HI_MPI_AWB_UnRegister!\n");
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}

static void v3_isp_unload(v3_isp_impl *isp_lib) {
    if (isp_lib->handleAe) dlclose(isp_lib->handleAe);
    isp_lib->handleAe = NULL;
    if (isp_lib->handleAwb) dlclose(isp_lib->handleAwb);
    isp_lib->handleAwb = NULL;
    if (isp_lib->handle) dlclose(isp_lib->handle);
    isp_lib->handle = NULL;
    memset(isp_lib, 0, sizeof(*isp_lib));
}
