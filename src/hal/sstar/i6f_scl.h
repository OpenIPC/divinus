#pragma once

#include "i6f_common.h"

typedef enum {
    I6F_SCL_BIND_INVALID,
    I6F_SCL_BIND_ID0 = 0x1,
    I6F_SCL_BIND_ID1 = 0x2,
    I6F_SCL_BIND_ID2 = 0x4,
    I6F_SCL_BIND_ID3 = 0x8,
    I6F_SCL_BIND_ID4 = 0x10,
    I6F_SCL_BIND_ID5 = 0x20,
    I6F_SCL_BIND_END = 0xffff,
} i6f_scl_bind;

typedef struct {
    i6f_common_rect crop;
    i6f_common_dim output;
    char mirror;
    char flip;
    i6f_common_pixfmt pixFmt;
    i6f_common_compr compress;
} i6f_scl_port;

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
    int (*fnSetPortConfig)(int device, int channel, int port, i6f_scl_port *config);
} i6f_scl_impl;

static int i6f_scl_load(i6f_scl_impl *scl_lib) {
    if (!(scl_lib->handle = dlopen("libmi_scl.so", RTLD_LAZY | RTLD_GLOBAL))) {
        fprintf(stderr, "[i6f_scl] Failed to load library!\nError: %s\n", dlerror());
        return EXIT_FAILURE;
    }

    if (!(scl_lib->fnCreateDevice = (int(*)(int device, unsigned int *binds))
        dlsym(scl_lib->handle, "MI_SCL_CreateDevice"))) {
        fprintf(stderr, "[i6f_scl] Failed to acquire symbol MI_SCL_CreateDevice!\n");
        return EXIT_FAILURE;
    }

    if (!(scl_lib->fnDestroyDevice = (int(*)(int device))
        dlsym(scl_lib->handle, "MI_SCL_DestroyDevice"))) {
        fprintf(stderr, "[i6f_scl] Failed to acquire symbol MI_SCL_DestroyDevice!\n");
        return EXIT_FAILURE;
    }

    if (!(scl_lib->fnAdjustChannelRotation = (int(*)(int device, int channel, int *rotate))
        dlsym(scl_lib->handle, "MI_SCL_SetChnParam"))) {
        fprintf(stderr, "[i6f_scl] Failed to acquire symbol MI_SCL_SetChnParam!\n");
        return EXIT_FAILURE;
    }

    if (!(scl_lib->fnCreateChannel = (int(*)(int device, int channel, unsigned int *reserved))
        dlsym(scl_lib->handle, "MI_SCL_CreateChannel"))) {
        fprintf(stderr, "[i6f_scl] Failed to acquire symbol MI_SCL_CreateChannel!\n");
        return EXIT_FAILURE;
    }

    if (!(scl_lib->fnDestroyChannel = (int(*)(int device, int channel))
        dlsym(scl_lib->handle, "MI_SCL_DestroyChannel"))) {
        fprintf(stderr, "[i6f_scl] Failed to acquire symbol MI_SCL_DestroyChannel!\n");
        return EXIT_FAILURE;
    }

    if (!(scl_lib->fnStartChannel = (int(*)(int device, int channel))
        dlsym(scl_lib->handle, "MI_SCL_StartChannel"))) {
        fprintf(stderr, "[i6f_scl] Failed to acquire symbol MI_SCL_StartChannel!\n");
        return EXIT_FAILURE;
    }

    if (!(scl_lib->fnStopChannel = (int(*)(int device, int channel))
        dlsym(scl_lib->handle, "MI_SCL_StopChannel"))) {
        fprintf(stderr, "[i6f_scl] Failed to acquire symbol MI_SCL_StopChannel!\n");
        return EXIT_FAILURE;
    }

    if (!(scl_lib->fnDisablePort = (int(*)(int device, int channel, int port))
        dlsym(scl_lib->handle, "MI_SCL_DisableOutputPort"))) {
        fprintf(stderr, "[i6f_scl] Failed to acquire symbol MI_SCL_DisableOutputPort!\n");
        return EXIT_FAILURE;
    }

    if (!(scl_lib->fnEnablePort = (int(*)(int device, int channel, int port))
        dlsym(scl_lib->handle, "MI_SCL_EnableOutputPort"))) {
        fprintf(stderr, "[i6f_scl] Failed to acquire symbol MI_SCL_EnableOutputPort!\n");
        return EXIT_FAILURE;
    }

    if (!(scl_lib->fnSetPortConfig = (int(*)(int device, int channel, int port, i6f_scl_port *config))
        dlsym(scl_lib->handle, "MI_SCL_SetOutputPortParam"))) {
        fprintf(stderr, "[i6f_scl] Failed to acquire symbol MI_SCL_SetOutputPortParam!\n");
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}

static void i6f_scl_unload(i6f_scl_impl *scl_lib) {
    if (scl_lib->handle) dlclose(scl_lib->handle);
    scl_lib->handle = NULL;
    memset(scl_lib, 0, sizeof(*scl_lib));
}