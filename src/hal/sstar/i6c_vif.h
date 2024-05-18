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
    i6c_common_rect crop;
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
    if (!(vif_lib->handle = dlopen("libmi_vif.so", RTLD_NOW | RTLD_GLOBAL))) {
        fprintf(stderr, "[i6c_vif] Failed to load library!\nError: %s\n", dlerror());
        return EXIT_FAILURE;
    }

    if (!(vif_lib->fnDisableDevice = (int(*)(int device))
        dlsym(vif_lib->handle, "MI_VIF_DisableDev"))) {
        fprintf(stderr, "[i6c_vif] Failed to acquire symbol MI_VIF_DisableDev!\n");
        return EXIT_FAILURE;
    }

    if (!(vif_lib->fnEnableDevice = (int(*)(int device))
        dlsym(vif_lib->handle, "MI_VIF_EnableDev"))) {
        fprintf(stderr, "[i6c_vif] Failed to acquire symbol MI_VIF_EnableDev!\n");
        return EXIT_FAILURE;
    }

    if (!(vif_lib->fnSetDeviceConfig = (int(*)(int device, i6c_vif_dev *config))
        dlsym(vif_lib->handle, "MI_VIF_SetDevAttr"))) {
        fprintf(stderr, "[i6c_vif] Failed to acquire symbol MI_VIF_SetDevAttr!\n");
        return EXIT_FAILURE;
    }

    if (!(vif_lib->fnDisablePort = (int(*)(int device, int port))
        dlsym(vif_lib->handle, "MI_VIF_DisableOutputPort"))) {
        fprintf(stderr, "[i6c_vif] Failed to acquire symbol MI_VIF_DisableOutputPort!\n");
        return EXIT_FAILURE;
    }

    if (!(vif_lib->fnEnablePort = (int(*)(int device, int port))
        dlsym(vif_lib->handle, "MI_VIF_EnableOutputPort"))) {
        fprintf(stderr, "[i6c_vif] Failed to acquire symbol MI_VIF_EnableOutputPort!\n");
        return EXIT_FAILURE;
    }

    if (!(vif_lib->fnSetPortConfig = (int(*)(int device, int port, i6c_vif_port *config))
        dlsym(vif_lib->handle, "MI_VIF_SetOutputPortAttr"))) {
        fprintf(stderr, "[i6c_vif] Failed to acquire symbol MI_VIF_SetOutputPortAttr!\n");
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}

static void i6c_vif_unload(i6c_vif_impl *vif_lib) {
    if (vif_lib->handle)
        dlclose(vif_lib->handle = NULL);
    memset(vif_lib, 0, sizeof(*vif_lib));
}