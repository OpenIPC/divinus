#pragma once

#include "i6_common.h"

typedef enum {
    I6_VPE_MODE_INVALID,
    I6_VPE_MODE_DVR = 0x1,
    I6_VPE_MODE_CAM_TOP = 0x2,
    I6_VPE_MODE_CAM_BOTTOM = 0x4,
    I6_VPE_MODE_CAM = I6_VPE_MODE_CAM_TOP | I6_VPE_MODE_CAM_BOTTOM,
    I6_VPE_MODE_REALTIME_TOP = 0x8,
    I6_VPE_MODE_REALTIME_BOTTOM = 0x10,
    I6_VPE_MODE_REALTIME = I6_VPE_MODE_REALTIME_TOP | I6_VPE_MODE_REALTIME_BOTTOM,
    I6_VPE_MODE_END
} i6_vpe_mode;

typedef enum {
    I6_VPE_SENS_INVALID,
    I6_VPE_SENS_ID0,
    I6_VPE_SENS_ID1,
    I6_VPE_SENS_ID2,
    I6_VPE_SENS_ID3,
    I6_VPE_SENS_END,
} i6_vpe_sens;

typedef struct {
    unsigned int rev;
    unsigned int size;
    unsigned char data[64];
} i6_vpe_iqver;

typedef struct {
    i6_common_dim capt;
    i6_common_pixfmt pixFmt;
    i6_common_hdr hdr;
    i6_vpe_sens sensor;
    char noiseRedOn;
    char edgeOn;
    char contrastOn;
    char invertOn;
    char rotateOn;
    i6_vpe_mode mode;
    i6_vpe_iqver iqparam;
    char lensAdjOn;
    unsigned int chnPort;
} i6_vpe_chn;

typedef struct {
    char reserved[16];
    i6_common_hdr hdr;
    // Accepts values from 0-7
    int level3DNR;
    char mirror;
    char flip;
    char reserved2;
    char lensAdjOn;
} i6_vpe_para;

typedef struct {
    i6_common_dim output;
    char mirror;
    char flip;
    i6_common_pixfmt pixFmt;
    i6_common_compr compress;
} i6_vpe_port;

typedef struct {
    void *handle;

    int (*fnCreateChannel)(int channel, i6_vpe_chn *config);
    int (*fnDestroyChannel)(int channel);
    int (*fnSetChannelConfig)(int channel, i6_vpe_chn *config);
    int (*fnSetChannelParam)(int channel, i6_vpe_para *config);
    int (*fnStartChannel)(int channel);
    int (*fnStopChannel)(int channel);

    int (*fnDisablePort)(int channel, int port);
    int (*fnEnablePort)(int channel, int port);
    int (*fnSetPortConfig)(int channel, int port, i6_vpe_port *config);
} i6_vpe_impl;

static int i6_vpe_load(i6_vpe_impl *vpe_lib) {
    if (!(vpe_lib->handle = dlopen("libmi_vpe.so", RTLD_NOW | RTLD_GLOBAL))) {
        fprintf(stderr, "[i6_vpe] Failed to load library!\nError: %s\n", dlerror());
        return EXIT_FAILURE;
    }

    if (!(vpe_lib->fnCreateChannel = (int(*)(int channel, i6_vpe_chn *config))
        dlsym(vpe_lib->handle, "MI_VPE_CreateChannel"))) {
        fprintf(stderr, "[i6_vpe] Failed to acquire symbol MI_VPE_CreateChannel!\n");
        return EXIT_FAILURE;
    }

    if (!(vpe_lib->fnDestroyChannel = (int(*)(int channel))
        dlsym(vpe_lib->handle, "MI_VPE_DestroyChannel"))) {
        fprintf(stderr, "[i6_vpe] Failed to acquire symbol MI_VPE_DestroyChannel!\n");
        return EXIT_FAILURE;
    }

    if (!(vpe_lib->fnSetChannelConfig = (int(*)(int channel, i6_vpe_chn *config))
        dlsym(vpe_lib->handle, "MI_VPE_SetChannelAttr"))) {
        fprintf(stderr, "[i6_vpe] Failed to acquire symbol MI_VPE_SetChannelAttr!\n");
        return EXIT_FAILURE;
    }

    if (!(vpe_lib->fnSetChannelParam = (int(*)(int channel, i6_vpe_para *config))
        dlsym(vpe_lib->handle, "MI_VPE_SetChannelParam"))) {
        fprintf(stderr, "[i6_vpe] Failed to acquire symbol MI_VPE_SetChannelParam!\n");
        return EXIT_FAILURE;
    }

    if (!(vpe_lib->fnStartChannel = (int(*)(int channel))
        dlsym(vpe_lib->handle, "MI_VPE_StartChannel"))) {
        fprintf(stderr, "[i6_vpe] Failed to acquire symbol MI_VPE_StartChannel!\n");
        return EXIT_FAILURE;
    }

    if (!(vpe_lib->fnStopChannel = (int(*)(int channel))
        dlsym(vpe_lib->handle, "MI_VPE_StopChannel"))) {
        fprintf(stderr, "[i6_vpe] Failed to acquire symbol MI_VPE_StopChannel!\n");
        return EXIT_FAILURE;
    }

    if (!(vpe_lib->fnDisablePort = (int(*)(int channel, int port))
        dlsym(vpe_lib->handle, "MI_VPE_DisablePort"))) {
        fprintf(stderr, "[i6_vpe] Failed to acquire symbol MI_VPE_DisablePort!\n");
        return EXIT_FAILURE;
    }

    if (!(vpe_lib->fnEnablePort = (int(*)(int channel, int port))
        dlsym(vpe_lib->handle, "MI_VPE_EnablePort"))) {
        fprintf(stderr, "[i6_vpe] Failed to acquire symbol MI_VPE_EnablePort!\n");
        return EXIT_FAILURE;
    }

    if (!(vpe_lib->fnSetPortConfig = (int(*)(int channel, int port, i6_vpe_port *config))
        dlsym(vpe_lib->handle, "MI_VPE_SetPortMode"))) {
        fprintf(stderr, "[i6_vpe] Failed to acquire symbol MI_VPE_SetPortMode!\n");
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}

static void i6_vpe_unload(i6_vpe_impl *vpe_lib) {
    if (vpe_lib->handle)
        dlclose(vpe_lib->handle = NULL);
    memset(vpe_lib, 0, sizeof(*vpe_lib));
}