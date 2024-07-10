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
        !(isp_lib->handleIrAuto = dlopen("lib_hiirauto.so", RTLD_LAZY | RTLD_GLOBAL)))
        HAL_ERROR("v3_isp", "Failed to load library!\nError: %s\n", dlerror());

    if (!(fnISP_AlgRegisterDehaze = (int(*)(int))
        hal_symbol_load("v3_isp", isp_lib->handleDefog, "ISP_AlgRegisterDehaze")))
        return EXIT_FAILURE;

    if (!(fnISP_AlgRegisterDrc = (int(*)(int))
        hal_symbol_load("v3_isp", isp_lib->handle, "ISP_AlgRegisterDrc")))
        return EXIT_FAILURE;

    if (!(isp_lib->fnExit = (int(*)(int device))
        hal_symbol_load("v3_isp", isp_lib->handle, "HI_MPI_ISP_Exit")))
        return EXIT_FAILURE;

    if (!(isp_lib->fnInit = (int(*)(int device))
        hal_symbol_load("v3_isp", isp_lib->handle, "HI_MPI_ISP_Init")))
        return EXIT_FAILURE;

    if (!(isp_lib->fnMemInit = (int(*)(int device))
        hal_symbol_load("v3_isp", isp_lib->handle, "HI_MPI_ISP_MemInit")))
        return EXIT_FAILURE;

    if (!(isp_lib->fnRun = (int(*)(int device))
        hal_symbol_load("v3_isp", isp_lib->handle, "HI_MPI_ISP_Run")))
        return EXIT_FAILURE;

    if (!(isp_lib->fnSetDeviceConfig = (int(*)(int device, v3_isp_dev *config))
        hal_symbol_load("v3_isp", isp_lib->handle, "HI_MPI_ISP_SetPubAttr")))
        return EXIT_FAILURE;

    if (!(isp_lib->fnSetWDRMode = (int(*)(int device, v3_common_wdr *mode))
        hal_symbol_load("v3_isp", isp_lib->handle, "HI_MPI_ISP_SetWDRMode")))
        return EXIT_FAILURE;

    if (!(isp_lib->fnRegisterAE = (int(*)(int device, v3_isp_alg *library))
        hal_symbol_load("v3_isp", isp_lib->handleAe, "HI_MPI_AE_Register")))
        return EXIT_FAILURE;

    if (!(isp_lib->fnRegisterAWB = (int(*)(int device, v3_isp_alg *library))
        hal_symbol_load("v3_isp", isp_lib->handleAwb, "HI_MPI_AWB_Register")))
        return EXIT_FAILURE;

    if (!(isp_lib->fnUnregisterAE = (int(*)(int device, v3_isp_alg *library))
        hal_symbol_load("v3_isp", isp_lib->handleAe, "HI_MPI_AE_UnRegister")))
        return EXIT_FAILURE;

    if (!(isp_lib->fnUnregisterAWB = (int(*)(int device, v3_isp_alg *library))
        hal_symbol_load("v3_isp", isp_lib->handleAwb, "HI_MPI_AWB_UnRegister")))
        return EXIT_FAILURE;

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
