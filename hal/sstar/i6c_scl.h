#pragma once
#include "i6c_common.h"

typedef enum {
    I6C_SCL_BIND_INVALID,
    I6C_SCL_BIND_ID0 = 0x1,
    I6C_SCL_BIND_ID1 = 0x2,
    I6C_SCL_BIND_ID2 = 0x4,
    I6C_SCL_BIND_ID3 = 0x8,
    I6C_SCL_BIND_ID4 = 0x10,
    I6C_SCL_BIND_ID5 = 0x20,
    I6C_SCL_BIND_ID6 = 0x40,
    I6C_SCL_BIND_ID7 = 0x80,
    I6C_SCL_BIND_ID8 = 0x100,
    I6C_SCL_BIND_END = 0xffff,
} i6c_scl_bind;

typedef struct {
    i6c_common_rect crop;
    i6c_common_dim output;
    char mirror;
    char flip;
    i6c_common_pixfmt pixFmt;
    i6c_common_compr compress;
} i6c_scl_port;

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
    int (*fnSetPortConfig)(int device, int channel, int port, i6c_scl_port *config);
} i6c_scl_impl;

int i6c_scl_load(i6c_scl_impl *scl_lib) {
    if (!(scl_lib->handle = dlopen("libmi_scl.so", RTLD_NOW))) {
        fprintf(stderr, "[i6c_scl] Failed to load library!\n");
        return EXIT_FAILURE;
    }

    if (!(scl_lib->fnCreateDevice = (int(*)(int device, unsigned int *binds))
        dlsym(scl_lib->handle, "MI_SCL_CreateDevice"))) {
        fprintf(stderr, "[i6c_scl] Failed to acquire symbol MI_SCL_CreateDevice!\n");
        return EXIT_FAILURE;
    }

    if (!(scl_lib->fnDestroyDevice = (int(*)(int device))
        dlsym(scl_lib->handle, "MI_SCL_DestroyDevice"))) {
        fprintf(stderr, "[i6c_scl] Failed to acquire symbol MI_SCL_DestroyDevice!\n");
        return EXIT_FAILURE;
    }

    if (!(scl_lib->fnAdjustChannelRotation = (int(*)(int device, int channel, int *rotate))
        dlsym(scl_lib->handle, "MI_SCL_SetChnParam"))) {
        fprintf(stderr, "[i6c_scl] Failed to acquire symbol MI_SCL_SetChnParam!\n");
        return EXIT_FAILURE;
    }

    if (!(scl_lib->fnCreateChannel = (int(*)(int device, int channel, unsigned int *reserved))
        dlsym(scl_lib->handle, "MI_SCL_CreateChannel"))) {
        fprintf(stderr, "[i6c_scl] Failed to acquire symbol MI_SCL_CreateChannel!\n");
        return EXIT_FAILURE;
    }

    if (!(scl_lib->fnDestroyChannel = (int(*)(int device, int channel))
        dlsym(scl_lib->handle, "MI_SCL_DestroyChannel"))) {
        fprintf(stderr, "[i6c_scl] Failed to acquire symbol MI_SCL_DestroyChannel!\n");
        return EXIT_FAILURE;
    }

    if (!(scl_lib->fnStartChannel = (int(*)(int device, int channel))
        dlsym(scl_lib->handle, "MI_SCL_StartChannel"))) {
        fprintf(stderr, "[i6c_scl] Failed to acquire symbol MI_SCL_StartChannel!\n");
        return EXIT_FAILURE;
    }

    if (!(scl_lib->fnStopChannel = (int(*)(int device, int channel))
        dlsym(scl_lib->handle, "MI_SCL_StopChannel"))) {
        fprintf(stderr, "[i6c_scl] Failed to acquire symbol MI_SCL_StopChannel!\n");
        return EXIT_FAILURE;
    }

    if (!(scl_lib->fnDisablePort = (int(*)(int device, int channel, int port))
        dlsym(scl_lib->handle, "MI_SCL_DisableOutputPort"))) {
        fprintf(stderr, "[i6c_scl] Failed to acquire symbol MI_SCL_DisableOutputPort!\n");
        return EXIT_FAILURE;
    }

    if (!(scl_lib->fnEnablePort = (int(*)(int device, int channel, int port))
        dlsym(scl_lib->handle, "MI_SCL_EnableOutputPort"))) {
        fprintf(stderr, "[i6c_scl] Failed to acquire symbol MI_SCL_EnableOutputPort!\n");
        return EXIT_FAILURE;
    }

    if (!(scl_lib->fnSetPortConfig = (int(*)(int device, int channel, int port, i6c_scl_port *config))
        dlsym(scl_lib->handle, "MI_SCL_SetOutputPortParam"))) {
        fprintf(stderr, "[i6c_scl] Failed to acquire symbol MI_SCL_SetOutputPortParam!\n");
        return EXIT_FAILURE;
    }
}

void i6c_scl_unload(i6c_scl_impl *scl_lib) {
    if (scl_lib->handle)
        dlclose(scl_lib->handle = NULL);
    memset(scl_lib, 0, sizeof(*scl_lib));
}