#pragma once

#include "v1_common.h"

extern int (*fnISP_AlgRegisterDrc)(int);

typedef enum {
    V1_ISP_WIN_NONE,
    V1_ISP_WIN_HORIZ,
    V1_ISP_WIN_VERT,
    V1_ISP_WIN_BOTH
} v1_isp_win;

typedef struct {
    int id;
    char libName[20];
} v1_isp_alg;

typedef struct {
    unsigned short width, height, framerate;
    v1_common_bayer bayer;
} v1_isp_img;

typedef struct {
    v1_isp_win mode;
    unsigned short x, width, y, height;
} v1_isp_tim;

typedef struct {
    void *handle, *handleAwb, *handleAe;

    int (*fnExit)(void);
    int (*fnInit)(void);
    int (*fnRun)(void);

    int (*fnSetImageConfig)(v1_isp_img *config);
    int (*fnSetInputTiming)(v1_isp_tim *config);
    int (*fnSetWDRMode)(v1_common_wdr *mode);

    int (*fnRegisterAE)(v1_isp_alg *library);
    int (*fnRegisterAWB)(v1_isp_alg *library);
    int (*fnUnregisterAE)(v1_isp_alg *library);
    int (*fnUnregisterAWB)(v1_isp_alg *library);
} v1_isp_impl;

static int v1_isp_load(v1_isp_impl *isp_lib) {
    if (!(isp_lib->handle = dlopen("libisp.so", RTLD_LAZY | RTLD_GLOBAL)) ||
        !(isp_lib->handleAe = dlopen("lib_hiae.so", RTLD_LAZY | RTLD_GLOBAL)) ||
        !(isp_lib->handleAwb = dlopen("lib_hiawb.so", RTLD_LAZY | RTLD_GLOBAL)))
        HAL_ERROR("v1_isp", "Failed to load library!\nError: %s\n", dlerror());

    if (!(fnISP_AlgRegisterDrc = (int(*)(int))
        hal_symbol_load("v1_isp", isp_lib->handle, "ISP_AlgRegisterDrc")))
        return EXIT_FAILURE;

    if (!(isp_lib->fnExit = (int(*)(void))
        hal_symbol_load("v1_isp", isp_lib->handle, "HI_MPI_ISP_Exit")))
        return EXIT_FAILURE;

    if (!(isp_lib->fnInit = (int(*)(void))
        hal_symbol_load("v1_isp", isp_lib->handle, "HI_MPI_ISP_Init")))
        return EXIT_FAILURE;

    if (!(isp_lib->fnRun = (int(*)(void))
        hal_symbol_load("v1_isp", isp_lib->handle, "HI_MPI_ISP_Run")))
        return EXIT_FAILURE;

    if (!(isp_lib->fnSetImageConfig = (int(*)(v1_isp_img *config))
        hal_symbol_load("v1_isp", isp_lib->handle, "HI_MPI_ISP_SetImageAttr")))
        return EXIT_FAILURE;

    if (!(isp_lib->fnSetInputTiming = (int(*)(v1_isp_tim *config))
        hal_symbol_load("v1_isp", isp_lib->handle, "HI_MPI_ISP_SetInputTiming")))
        return EXIT_FAILURE;

    if (!(isp_lib->fnSetWDRMode = (int(*)(v1_common_wdr *mode))
        hal_symbol_load("v1_isp", isp_lib->handle, "HI_MPI_ISP_SetWdrAttr")))
        return EXIT_FAILURE;

    if (!(isp_lib->fnRegisterAE = (int(*)(v1_isp_alg *library))
        hal_symbol_load("v1_isp", isp_lib->handleAe, "HI_MPI_AE_Register")))
        return EXIT_FAILURE;

    if (!(isp_lib->fnRegisterAWB = (int(*)(v1_isp_alg *library))
        hal_symbol_load("v1_isp", isp_lib->handleAwb, "HI_MPI_AWB_Register")))
        return EXIT_FAILURE;

    if (!(isp_lib->fnUnregisterAE = (int(*)(v1_isp_alg *library))
        hal_symbol_load("v1_isp", isp_lib->handleAe, "HI_MPI_AE_UnRegister")))
        return EXIT_FAILURE;

    if (!(isp_lib->fnUnregisterAWB = (int(*)(v1_isp_alg *library))
        hal_symbol_load("v1_isp", isp_lib->handleAwb, "HI_MPI_AWB_UnRegister")))
        return EXIT_FAILURE;

    return EXIT_SUCCESS;
}

static void v1_isp_unload(v1_isp_impl *isp_lib) {
    if (isp_lib->handleAe) dlclose(isp_lib->handleAe);
    isp_lib->handleAe = NULL;
    if (isp_lib->handleAwb) dlclose(isp_lib->handleAwb);
    isp_lib->handleAwb = NULL;
    if (isp_lib->handle) dlclose(isp_lib->handle);
    isp_lib->handle = NULL;
    memset(isp_lib, 0, sizeof(*isp_lib));
}
