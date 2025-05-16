#pragma once

#include "i6_common.h"

typedef struct {
    unsigned int minShutterUs;
    unsigned int maxShutterUs;
    unsigned int minApertX10;
    unsigned int maxApertX10;
    unsigned int minSensorGain;
    unsigned int minIspGain;
    unsigned int maxSensorGain;
    unsigned int maxIspGain;
} i6_isp_exp;

typedef struct {
    int params[13];
} i6_isp_p3a;

typedef struct {
    void *handle, *handleCus3a, *handleIspAlgo;

    int (*fnDisableUserspace3A)(int channel);
    int (*fnEnableUserspace3A)(int channel, i6_isp_p3a *params);
    int (*fnLoadChannelConfig)(int channel, char *path, unsigned int key);
    int (*fnSetColorToGray)(int channel, char *enable);
    int (*fnGetExposureLimit)(int channel, i6_isp_exp *config);
    int (*fnSetExposureLimit)(int channel, i6_isp_exp *config);
} i6_isp_impl;

static int i6_isp_load(i6_isp_impl *isp_lib) {
    isp_lib->handleIspAlgo = dlopen("libispalgo.so", RTLD_LAZY | RTLD_GLOBAL);

    isp_lib->handleCus3a = dlopen("libcus3a.so", RTLD_LAZY | RTLD_GLOBAL);

    if (!(isp_lib->handle = dlopen("libmi_isp.so", RTLD_LAZY | RTLD_GLOBAL)))
        HAL_ERROR("i6_isp", "Failed to load library!\nError: %s\n", dlerror());

    if (!(isp_lib->fnDisableUserspace3A = (int(*)(int channel))
        hal_symbol_load("i6_isp", isp_lib->handle, "MI_ISP_DisableUserspace3A")))
        return EXIT_FAILURE;

    if (!(isp_lib->fnEnableUserspace3A = (int(*)(int channel, i6_isp_p3a *params))
        hal_symbol_load("i6_isp", isp_lib->handle, "MI_ISP_CUS3A_Enable")))
        return EXIT_FAILURE;

    if (!(isp_lib->fnLoadChannelConfig = (int(*)(int channel, char *path, unsigned int key))
        hal_symbol_load("i6_isp", isp_lib->handle, "MI_ISP_API_CmdLoadBinFile")))
        return EXIT_FAILURE;

    if (!(isp_lib->fnSetColorToGray = (int(*)(int channel, char *enable))
        hal_symbol_load("i6_isp", isp_lib->handle, "MI_ISP_IQ_SetColorToGray")))
        return EXIT_FAILURE;

    if (!(isp_lib->fnGetExposureLimit = (int(*)(int channel, i6_isp_exp *config))
        hal_symbol_load("i6_isp", isp_lib->handle, "MI_ISP_AE_GetExposureLimit")))
        return EXIT_FAILURE;

    if (!(isp_lib->fnSetExposureLimit = (int(*)(int channel, i6_isp_exp *config))
        hal_symbol_load("i6_isp", isp_lib->handle, "MI_ISP_AE_SetExposureLimit")))
        return EXIT_FAILURE;

    return EXIT_SUCCESS;
}

static void i6_isp_unload(i6_isp_impl *isp_lib) {
    if (isp_lib->handle) dlclose(isp_lib->handle);
    isp_lib->handle = NULL;
    if (isp_lib->handleCus3a) dlclose(isp_lib->handleCus3a);
    isp_lib->handleCus3a = NULL;
    if (isp_lib->handleIspAlgo) dlclose(isp_lib->handleIspAlgo);
    isp_lib->handleIspAlgo = NULL;
    memset(isp_lib, 0, sizeof(*isp_lib));
}