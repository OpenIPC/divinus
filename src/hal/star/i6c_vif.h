#pragma once

#include "i6c_common.h"

typedef enum {
    I6C_VIF_FRATE_FULL,
    I6C_VIF_FRATE_HALF,
    I6C_VIF_FRATE_QUART,
    I6C_VIF_FRATE_OCTANT,
    I6C_VIF_FRATE_3QUARTS,
    I6C_VIF_FRATE_END
} i6c_vif_frate;

typedef enum {
    I6C_VIF_CLK_12MHZ,
    I6C_VIF_CLK_18MHZ,
    I6C_VIF_CLK_27MHZ,
    I6C_VIF_CLK_36MHZ,
    I6C_VIF_CLK_54MHZ,
    I6C_VIF_CLK_108MHZ,
    I6C_VIF_CLK_END
} i6c_vif_clk;

typedef enum {
    I6C_VIF_WORK_1MULTIPLEX,
    I6C_VIF_WORK_2MULTIPLEX,
    I6C_VIF_WORK_4MULTIPLEX,
    I6C_VIF_WORK_END
} i6c_vif_work;

typedef struct {
    i6c_common_pixfmt pixFmt;
    i6c_common_rect crop;
    // Values 0-3 correspond to No, Top, Bottom, Both
    int field;
    char halfHScan;
} i6c_vif_dev;

typedef struct {
    i6c_common_intf intf;
    i6c_vif_work work;
    i6c_common_hdr hdr;
    i6c_common_edge edge;
    i6c_vif_clk clock;
    int interlaceOn;
    unsigned int grpStitch;
} i6c_vif_grp;

typedef struct {
    i6c_common_rect capt;
    i6c_common_dim dest;
    i6c_common_pixfmt pixFmt;
    i6c_vif_frate frate;
    i6c_common_compr compress;
} i6c_vif_port;

typedef struct {
    void *handle;

    int (*fnDisableDevice)(int device);
    int (*fnEnableDevice)(int device);
    int (*fnSetDeviceConfig)(int device, i6c_vif_dev *config);

    int (*fnCreateGroup)(int group, i6c_vif_grp *config);
    int (*fnDestroyGroup)(int group);

    int (*fnDisablePort)(int device, int port);
    int (*fnEnablePort)(int device, int port);
    int (*fnSetPortConfig)(int device, int port, i6c_vif_port *config);
} i6c_vif_impl;

static int i6c_vif_load(i6c_vif_impl *vif_lib) {
    if (!(vif_lib->handle = dlopen("libmi_vif.so", RTLD_LAZY | RTLD_GLOBAL)))
        HAL_ERROR("i6c_vif", "Failed to load library!\nError: %s\n", dlerror());

    if (!(vif_lib->fnDisableDevice = (int(*)(int device))
        hal_symbol_load("i6c_vif", vif_lib->handle, "MI_VIF_DisableDev")))
        return EXIT_FAILURE;

    if (!(vif_lib->fnEnableDevice = (int(*)(int device))
        hal_symbol_load("i6c_vif", vif_lib->handle, "MI_VIF_EnableDev")))
        return EXIT_FAILURE;

    if (!(vif_lib->fnSetDeviceConfig = (int(*)(int device, i6c_vif_dev *config))
        hal_symbol_load("i6c_vif", vif_lib->handle, "MI_VIF_SetDevAttr")))
        return EXIT_FAILURE;

    if (!(vif_lib->fnCreateGroup = (int(*)(int group, i6c_vif_grp *config))
        hal_symbol_load("i6c_vif", vif_lib->handle, "MI_VIF_CreateDevGroup")))
        return EXIT_FAILURE;

    if (!(vif_lib->fnDestroyGroup = (int(*)(int group))
        hal_symbol_load("i6c_vif", vif_lib->handle, "MI_VIF_DestroyDevGroup")))
        return EXIT_FAILURE;

    if (!(vif_lib->fnDisablePort = (int(*)(int device, int port))
        hal_symbol_load("i6c_vif", vif_lib->handle, "MI_VIF_DisableOutputPort")))
        return EXIT_FAILURE;

    if (!(vif_lib->fnEnablePort = (int(*)(int device, int port))
        hal_symbol_load("i6c_vif", vif_lib->handle, "MI_VIF_EnableOutputPort")))
        return EXIT_FAILURE;

    if (!(vif_lib->fnSetPortConfig = (int(*)(int device, int port, i6c_vif_port *config))
        hal_symbol_load("i6c_vif", vif_lib->handle, "MI_VIF_SetOutputPortAttr")))
        return EXIT_FAILURE;

    return EXIT_SUCCESS;
}

static void i6c_vif_unload(i6c_vif_impl *vif_lib) {
    if (vif_lib->handle) dlclose(vif_lib->handle);
    vif_lib->handle = NULL;
    memset(vif_lib, 0, sizeof(*vif_lib));
}