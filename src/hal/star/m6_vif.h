#pragma once

#include "m6_common.h"

typedef enum {
    M6_VIF_FRATE_FULL,
    M6_VIF_FRATE_HALF,
    M6_VIF_FRATE_QUART,
    M6_VIF_FRATE_OCTANT,
    M6_VIF_FRATE_3QUARTS,
    M6_VIF_FRATE_END
} m6_vif_frate;

typedef enum {
    M6_VIF_CLK_12MHZ,
    M6_VIF_CLK_18MHZ,
    M6_VIF_CLK_27MHZ,
    M6_VIF_CLK_36MHZ,
    M6_VIF_CLK_54MHZ,
    M6_VIF_CLK_108MHZ,
    M6_VIF_CLK_END
} m6_vif_clk;

typedef enum {
    M6_VIF_WORK_1MULTIPLEX,
    M6_VIF_WORK_2MULTIPLEX,
    M6_VIF_WORK_4MULTIPLEX,
    M6_VIF_WORK_END
} m6_vif_work;

typedef struct {
    m6_common_pixfmt pixFmt;
    m6_common_rect crop;
    // Values 0-3 correspond to No, Top, Bottom, Both
    int field;
    char halfHScan;
} m6_vif_dev;

typedef struct {
    m6_common_intf intf;
    m6_vif_work work;
    m6_common_hdr hdr;
    m6_common_edge edge;
    m6_vif_clk clock;
    int interlaceOn;
    unsigned int grpStitch;
} m6_vif_grp;

typedef struct {
    m6_common_rect capt;
    m6_common_dim dest;
    m6_common_pixfmt pixFmt;
    m6_vif_frate frate;
} m6_vif_port;

typedef struct {
    void *handle;

    int (*fnDisableDevice)(int device);
    int (*fnEnableDevice)(int device);
    int (*fnSetDeviceConfig)(int device, m6_vif_dev *config);

    int (*fnCreateGroup)(int group, m6_vif_grp *config);
    int (*fnDestroyGroup)(int group);

    int (*fnDisablePort)(int device, int port);
    int (*fnEnablePort)(int device, int port);
    int (*fnSetPortConfig)(int device, int port, m6_vif_port *config);
} m6_vif_impl;

static int m6_vif_load(m6_vif_impl *vif_lib) {
    if (!(vif_lib->handle = dlopen("libmi_vif.so", RTLD_LAZY | RTLD_GLOBAL)))
        HAL_ERROR("m6_vif", "Failed to load library!\nError: %s\n", dlerror());

    if (!(vif_lib->fnDisableDevice = (int(*)(int device))
        hal_symbol_load("m6_vif", vif_lib->handle, "MI_VIF_DisableDev")))
        return EXIT_FAILURE;

    if (!(vif_lib->fnEnableDevice = (int(*)(int device))
        hal_symbol_load("m6_vif", vif_lib->handle,  "MI_VIF_EnableDev")))
        return EXIT_FAILURE;

    if (!(vif_lib->fnSetDeviceConfig = (int(*)(int device, m6_vif_dev *config))
        hal_symbol_load("m6_vif", vif_lib->handle,  "MI_VIF_SetDevAttr")))
        return EXIT_FAILURE;

    if (!(vif_lib->fnCreateGroup = (int(*)(int group, m6_vif_grp *config))
        hal_symbol_load("m6_vif", vif_lib->handle,  "MI_VIF_CreateDevGroup")))
        return EXIT_FAILURE;

    if (!(vif_lib->fnDestroyGroup = (int(*)(int group))
        hal_symbol_load("m6_vif", vif_lib->handle,  "MI_VIF_DestroyDevGroup")))
        return EXIT_FAILURE;

    if (!(vif_lib->fnDisablePort = (int(*)(int device, int port))
        hal_symbol_load("m6_vif", vif_lib->handle,  "MI_VIF_DisableOutputPort")))
        return EXIT_FAILURE;

    if (!(vif_lib->fnEnablePort = (int(*)(int device, int port))
        hal_symbol_load("m6_vif", vif_lib->handle,  "MI_VIF_EnableOutputPort")))
        return EXIT_FAILURE;

    if (!(vif_lib->fnSetPortConfig = (int(*)(int device, int port, m6_vif_port *config))
        hal_symbol_load("m6_vif", vif_lib->handle,  "MI_VIF_SetOutputPortAttr")))
        return EXIT_FAILURE;

    return EXIT_SUCCESS;
}

static void m6_vif_unload(m6_vif_impl *vif_lib) {
    if (vif_lib->handle) dlclose(vif_lib->handle);
    vif_lib->handle = NULL;
    memset(vif_lib, 0, sizeof(*vif_lib));
}