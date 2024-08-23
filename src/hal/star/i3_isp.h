#pragma once

#include "i3_common.h"

typedef struct {
    void *handle;

    int (*fnLoadChannelConfig)(int channel, char *path, unsigned int key);
    int (*fnSetColorToGray)(int channel, char *enable);
} i3_isp_impl;

static int i3_isp_load(i3_isp_impl *isp_lib) {
    if (!(isp_lib->handle = dlopen("libmi_isp.so", RTLD_LAZY | RTLD_GLOBAL)))
        HAL_ERROR("i3_isp", "Failed to load library!\nError: %s\n", dlerror());

    if (!(isp_lib->fnLoadChannelConfig = (int(*)(int channel, char *path, unsigned int key))
        hal_symbol_load("i3_isp", isp_lib->handle, "MI_ISP_Load_ISPCmdBinFile")))
        return EXIT_FAILURE;

    if (!(isp_lib->fnSetColorToGray = (int(*)(int channel, char *enable))
        hal_symbol_load("i3_isp", isp_lib->handle, "MI_ISP_SetColorToGray")))
        return EXIT_FAILURE;

    return EXIT_SUCCESS;
}

static void i3_isp_unload(i3_isp_impl *isp_lib) {
    if (isp_lib->handle) dlclose(isp_lib->handle);
    isp_lib->handle = NULL;
    memset(isp_lib, 0, sizeof(*isp_lib));
}