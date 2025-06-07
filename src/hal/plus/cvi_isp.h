#pragma once

#include "cvi_common.h"

typedef enum {
    CVI_ISP_DIR_NORMAL,
    CVI_ISP_DIR_MIRROR,
    CVI_ISP_DIR_FLIP,
    CVI_ISP_DIR_MIRROR_FLIP,
    CVI_ISP_DIR_END
} cvi_isp_dir;

typedef struct {
    int id;
    char libName[20];
} cvi_isp_alg;

typedef struct {
    int sensorId;
    cvi_isp_alg aeLib;
    cvi_isp_alg afLib;
    cvi_isp_alg awbLib;
} cvi_isp_bind;

typedef struct {
    cvi_common_rect capt;
    cvi_common_dim size;
    float framerate;
    cvi_common_bayer bayer;
    cvi_common_wdr wdr;
    unsigned char mode;
} cvi_isp_dev;

typedef struct {
    void *handle, *handleAlgo, *handleAwb, *handleAe;

    int (*fnExit)(int pipe);
    int (*fnInit)(int pipe);
    int (*fnMemInit)(int pipe);
    int (*fnRun)(int pipe);

    int (*fnSetDeviceBind)(int pipe, cvi_isp_bind *bind);
    int (*fnSetDeviceConfig)(int pipe, cvi_isp_dev *config);

    int (*fnResetIntf)(int device, int state);
    int (*fnResetSensor)(int device, int state);
    int (*fnSetIntfConfig)(int pipe, void *config);
    int (*fnSetSensorClock)(int device, int enable);
    int (*fnSetSensorEdge)(int device, int up);
    int (*fnSetSensorMaster)(void *freq);

    int (*fnRegisterAE)(int pipe, cvi_isp_alg *library);
    int (*fnRegisterAWB)(int pipe, cvi_isp_alg *library);
    int (*fnUnregisterAE)(int pipe, cvi_isp_alg *library);
    int (*fnUnregisterAWB)(int pipe, cvi_isp_alg *library);
} cvi_isp_impl;

static int cvi_isp_load(cvi_isp_impl *isp_lib) {
    if (!(isp_lib->handle = dlopen("libisp.so", RTLD_LAZY | RTLD_GLOBAL)) ||
        !(isp_lib->handleAlgo = dlopen("libisp_algo.so", RTLD_LAZY | RTLD_GLOBAL)) ||
        !(isp_lib->handleAe = dlopen("libae.so", RTLD_LAZY | RTLD_GLOBAL)) ||
        !(isp_lib->handleAwb = dlopen("libawb.so", RTLD_LAZY | RTLD_GLOBAL)))
        HAL_ERROR("cvi_isp", "Failed to load library!\nError: %s\n", dlerror());

    if (!(isp_lib->fnExit = (int(*)(int pipe))
        hal_symbol_load("cvi_isp", isp_lib->handle, "CVI_ISP_Exit")))
        return EXIT_FAILURE;

    if (!(isp_lib->fnInit = (int(*)(int pipe))
        hal_symbol_load("cvi_isp", isp_lib->handle, "CVI_ISP_Init")))
        return EXIT_FAILURE;

    if (!(isp_lib->fnMemInit = (int(*)(int pipe))
        hal_symbol_load("cvi_isp", isp_lib->handle, "CVI_ISP_MemInit")))
        return EXIT_FAILURE;

    if (!(isp_lib->fnRun = (int(*)(int pipe))
        hal_symbol_load("cvi_isp", isp_lib->handle, "CVI_ISP_Run")))
        return EXIT_FAILURE;

    if (!(isp_lib->fnSetDeviceBind = (int(*)(int pipe, cvi_isp_bind *bind))
        hal_symbol_load("cvi_isp", isp_lib->handle, "CVI_ISP_SetBindAttr")))
        return EXIT_FAILURE;

    if (!(isp_lib->fnSetDeviceConfig = (int(*)(int pipe, cvi_isp_dev *config))
        hal_symbol_load("cvi_isp", isp_lib->handle, "CVI_ISP_SetPubAttr")))
        return EXIT_FAILURE;

    if (!(isp_lib->fnResetIntf = (int(*)(int device, int state))
        hal_symbol_load("cvi_isp", isp_lib->handle, "CVI_MIPI_SetMipiReset")))
        return EXIT_FAILURE;

    if (!(isp_lib->fnResetSensor = (int(*)(int device, int state))
        hal_symbol_load("cvi_isp", isp_lib->handle, "CVI_MIPI_SetSensorReset")))
        return EXIT_FAILURE;

    if (!(isp_lib->fnSetIntfConfig = (int(*)(int pipe, void *config))
        hal_symbol_load("cvi_isp", isp_lib->handle, "CVI_MIPI_SetMipiAttr")))
        return EXIT_FAILURE;

    if (!(isp_lib->fnSetSensorClock = (int(*)(int device, int enable))
        hal_symbol_load("cvi_isp", isp_lib->handle, "CVI_MIPI_SetSensorClock")))
        return EXIT_FAILURE;

    if (!(isp_lib->fnSetSensorEdge = (int(*)(int device, int up))
        hal_symbol_load("cvi_isp", isp_lib->handle, "CVI_MIPI_SetClkEdge")))
        return EXIT_FAILURE;

    if (!(isp_lib->fnSetSensorMaster = (int(*)(void *freq))
        hal_symbol_load("cvi_isp", isp_lib->handle, "CVI_MIPI_SetSnsMclk")))
        return EXIT_FAILURE;

    if (!(isp_lib->fnRegisterAE = (int(*)(int pipe, cvi_isp_alg *library))
        hal_symbol_load("cvi_isp", isp_lib->handleAe, "CVI_AE_Register")))
        return EXIT_FAILURE;

    if (!(isp_lib->fnRegisterAWB = (int(*)(int pipe, cvi_isp_alg *library))
        hal_symbol_load("cvi_isp", isp_lib->handleAwb, "CVI_AWB_Register")))
        return EXIT_FAILURE;

    if (!(isp_lib->fnUnregisterAE = (int(*)(int pipe, cvi_isp_alg *library))
        hal_symbol_load("cvi_isp", isp_lib->handleAe, "CVI_AE_UnRegister")))
        return EXIT_FAILURE;

    if (!(isp_lib->fnUnregisterAWB = (int(*)(int pipe, cvi_isp_alg *library))
        hal_symbol_load("cvi_isp", isp_lib->handleAwb, "CVI_AWB_UnRegister")))
        return EXIT_FAILURE;

    return EXIT_SUCCESS;
}

static void cvi_isp_unload(cvi_isp_impl *isp_lib) {
    if (isp_lib->handleAe) dlclose(isp_lib->handleAe);
    isp_lib->handleAe = NULL;
    if (isp_lib->handleAwb) dlclose(isp_lib->handleAwb);
    isp_lib->handleAwb = NULL;
    if (isp_lib->handleAlgo) dlclose(isp_lib->handleAlgo);
    isp_lib->handleAlgo = NULL;
    if (isp_lib->handle) dlclose(isp_lib->handle);
    isp_lib->handle = NULL;
    memset(isp_lib, 0, sizeof(*isp_lib));
}
