#pragma once

#include "m6_common.h"

typedef enum {
    M6_SCL_BIND_INVALID,
    M6_SCL_BIND_ID0 = 0x1,
    M6_SCL_BIND_ID1 = 0x2,
    M6_SCL_BIND_ID2 = 0x4,
    M6_SCL_BIND_ID3 = 0x8,
    M6_SCL_BIND_ID4 = 0x10,
    M6_SCL_BIND_ID5 = 0x20,
    M6_SCL_BIND_END = 0xffff,
} m6_scl_bind;

typedef struct {
    m6_common_rect crop;
    m6_common_dim output;
    char mirror;
    char flip;
    m6_common_pixfmt pixFmt;
    m6_common_compr compress;
} m6_scl_port;

typedef struct {
    void *handle;
    
    int (*fnCreateDevice)(int device, unsigned int *binds);
    int (*fnDestroyDevice)(int device);

    int (*fnAdjustChannelRotation)(int device, int channel, int *rotate);
    int (*fnCreateChannel)(int device, int channel, unsigned int *reserved);
    int (*fnDestroyChannel)(int device, int channel);
    int (*fnStartChannel)(int device, int channel);
    int (*fnStopChannel)(int device, int channel);

    int (*fnDisablePort)(int device, int channel, int port);
    int (*fnEnablePort)(int device, int channel, int port);
    int (*fnSetPortConfig)(int device, int channel, int port, m6_scl_port *config);
} m6_scl_impl;

static int m6_scl_load(m6_scl_impl *scl_lib) {
    if (!(scl_lib->handle = dlopen("libmi_scl.so", RTLD_LAZY | RTLD_GLOBAL)))
        HAL_ERROR("m6_scl", "Failed to load library!\nError: %s\n", dlerror());

    if (!(scl_lib->fnCreateDevice = (int(*)(int device, unsigned int *binds))
        hal_symbol_load("m6_scl", scl_lib->handle, "MI_SCL_CreateDevice")))
        return EXIT_FAILURE;

    if (!(scl_lib->fnDestroyDevice = (int(*)(int device))
        hal_symbol_load("m6_scl", scl_lib->handle, "MI_SCL_DestroyDevice")))
        return EXIT_FAILURE;

    if (!(scl_lib->fnAdjustChannelRotation = (int(*)(int device, int channel, int *rotate))
        hal_symbol_load("m6_scl", scl_lib->handle, "MI_SCL_SetChnParam")))
        return EXIT_FAILURE;

    if (!(scl_lib->fnCreateChannel = (int(*)(int device, int channel, unsigned int *reserved))
        hal_symbol_load("m6_scl", scl_lib->handle, "MI_SCL_CreateChannel")))
        return EXIT_FAILURE;

    if (!(scl_lib->fnDestroyChannel = (int(*)(int device, int channel))
        hal_symbol_load("m6_scl", scl_lib->handle, "MI_SCL_DestroyChannel")))
        return EXIT_FAILURE;

    if (!(scl_lib->fnStartChannel = (int(*)(int device, int channel))
        hal_symbol_load("m6_scl", scl_lib->handle, "MI_SCL_StartChannel")))
        return EXIT_FAILURE;

    if (!(scl_lib->fnStopChannel = (int(*)(int device, int channel))
        hal_symbol_load("m6_scl", scl_lib->handle, "MI_SCL_StopChannel")))
        return EXIT_FAILURE;

    if (!(scl_lib->fnDisablePort = (int(*)(int device, int channel, int port))
        hal_symbol_load("m6_scl", scl_lib->handle, "MI_SCL_DisableOutputPort")))
        return EXIT_FAILURE;

    if (!(scl_lib->fnEnablePort = (int(*)(int device, int channel, int port))
        hal_symbol_load("m6_scl", scl_lib->handle, "MI_SCL_EnableOutputPort")))
        return EXIT_FAILURE;

    if (!(scl_lib->fnSetPortConfig = (int(*)(int device, int channel, int port, m6_scl_port *config))
        hal_symbol_load("m6_scl", scl_lib->handle, "MI_SCL_SetOutputPortParam")))
        return EXIT_FAILURE;

    return EXIT_SUCCESS;
}

static void m6_scl_unload(m6_scl_impl *scl_lib) {
    if (scl_lib->handle) dlclose(scl_lib->handle);
    scl_lib->handle = NULL;
    memset(scl_lib, 0, sizeof(*scl_lib));
}