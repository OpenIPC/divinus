#pragma once

#include "i6f_common.h"

typedef struct {
    unsigned int rev;
    unsigned int size;
    unsigned char data[64];
} i6f_isp_iqver;

typedef struct {
    unsigned int sensorId;
    i6f_isp_iqver iqVer;
    unsigned int sync3A;
} i6f_isp_chn;

typedef struct {
    i6f_common_hdr hdr;
    // Accepts values from 0-7
    int level3DNR;
    char mirror;
    char flip;
    // Represents 90-degree arcs
    char rotate;
} i6f_isp_para;

typedef struct {
    i6f_common_rect crop;
    i6f_common_pixfmt pixFmt;
} i6f_isp_port;

typedef struct {
    void *handle;
    
    int (*fnCreateDevice)(int device, unsigned int *combo);
    int (*fnDestroyDevice)(int device);

    int (*fnCreateChannel)(int device, int channel, i6f_isp_chn *config);
    int (*fnDestroyChannel)(int device, int channel);
    int (*fnLoadChannelConfig)(int device, int channel, char *path, unsigned int key);
    int (*fnSetChannelParam)(int device, int channel, i6f_isp_para *config);
    int (*fnStartChannel)(int device, int channel);
    int (*fnStopChannel)(int device, int channel);

    int (*fnDisablePort)(int device, int channel, int port);
    int (*fnEnablePort)(int device, int channel, int port);
    int (*fnSetPortConfig)(int device, int channel, int port, i6f_isp_port *config);

    int (*fnSetColorToGray)(int device, int channel, char *enable);
} i6f_isp_impl;

int i6f_isp_load(i6f_isp_impl *isp_lib) {
    if (!(isp_lib->handle = dlopen("libmi_isp.so", RTLD_NOW))) {
        fprintf(stderr, "[i6f_isp] Failed to load library!\n");
        return EXIT_FAILURE;
    }

    if (!(isp_lib->fnCreateDevice = (int(*)(int device, unsigned int *combo))
        dlsym(isp_lib->handle, "MI_ISP_CreateDevice"))) {
        fprintf(stderr, "[i6f_isp] Failed to acquire symbol MI_ISP_CreateDevice!\n");
        return EXIT_FAILURE;
    }

    if (!(isp_lib->fnDestroyDevice = (int(*)(int device))
        dlsym(isp_lib->handle, "MI_ISP_DestoryDevice"))) {
        fprintf(stderr, "[i6f_isp] Failed to acquire symbol MI_ISP_DestoryDevice!\n");
        return EXIT_FAILURE;
    }

    if (!(isp_lib->fnCreateChannel = (int(*)(int device, int channel, i6f_isp_chn *config))
        dlsym(isp_lib->handle, "MI_ISP_CreateChannel"))) {
        fprintf(stderr, "[i6f_isp] Failed to acquire symbol MI_ISP_CreateChannel!\n");
        return EXIT_FAILURE;
    }

    if (!(isp_lib->fnDestroyChannel = (int(*)(int device, int channel))
        dlsym(isp_lib->handle, "MI_ISP_DestroyChannel"))) {
        fprintf(stderr, "[i6f_isp] Failed to acquire symbol MI_ISP_DestroyChannel!\n");
        return EXIT_FAILURE;
    }

    if (!(isp_lib->fnLoadChannelConfig = (int(*)(int device, int channel, char *path, unsigned int key))
        dlsym(isp_lib->handle, "MI_ISP_ALGO_API_CmdLoadBinFile"))) {
        fprintf(stderr, "[i6f_isp] Failed to acquire symbol MI_ISP_ALGO_API_CmdLoadBinFile!\n");
        return EXIT_FAILURE;
    }

    if (!(isp_lib->fnSetChannelParam = (int(*)(int device, int channel, i6f_isp_para *config))
        dlsym(isp_lib->handle, "MI_ISP_SetChnParam"))) {
        fprintf(stderr, "[i6f_isp] Failed to acquire symbol MI_ISP_SetChnParam!\n");
        return EXIT_FAILURE;
    }

    if (!(isp_lib->fnStartChannel = (int(*)(int device, int channel))
        dlsym(isp_lib->handle, "MI_ISP_StartChannel"))) {
        fprintf(stderr, "[i6f_isp] Failed to acquire symbol MI_ISP_StartChannel!\n");
        return EXIT_FAILURE;
    }

    if (!(isp_lib->fnStopChannel = (int(*)(int device, int channel))
        dlsym(isp_lib->handle, "MI_ISP_StopChannel"))) {
        fprintf(stderr, "[i6f_isp] Failed to acquire symbol MI_ISP_StopChannel!\n");
        return EXIT_FAILURE;
    }

    if (!(isp_lib->fnDisablePort = (int(*)(int device, int channel, int port))
        dlsym(isp_lib->handle, "MI_ISP_DisableOutputPort"))) {
        fprintf(stderr, "[i6f_isp] Failed to acquire symbol MI_ISP_DisableOutputPort!\n");
        return EXIT_FAILURE;
    }

    if (!(isp_lib->fnEnablePort = (int(*)(int device, int channel, int port))
        dlsym(isp_lib->handle, "MI_ISP_EnableOutputPort"))) {
        fprintf(stderr, "[i6f_isp] Failed to acquire symbol MI_ISP_EnableOutputPort!\n");
        return EXIT_FAILURE;
    }

    if (!(isp_lib->fnSetPortConfig = (int(*)(int device, int channel, int port, i6f_isp_port *config))
        dlsym(isp_lib->handle, "MI_ISP_SetOutputPortParam"))) {
        fprintf(stderr, "[i6f_isp] Failed to acquire symbol MI_ISP_SetOutputPortParam!\n");
        return EXIT_FAILURE;
    }

    if (!(isp_lib->fnSetColorToGray = (int(*)(int device, int channel, char *enable))
        dlsym(isp_lib->handle, "MI_ISP_IQ_SetColorToGray"))) {
        fprintf(stderr, "[i6f_isp] Failed to acquire symbol MI_ISP_IQ_SetColorToGray!\n");
        return EXIT_FAILURE;
    }
    
    return EXIT_SUCCESS;
}

void i6f_isp_unload(i6f_isp_impl *isp_lib) {
    if (isp_lib->handle)
        dlclose(isp_lib->handle = NULL);
    memset(isp_lib, 0, sizeof(*isp_lib));
}