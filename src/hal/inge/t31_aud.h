#pragma once

#include "t31_common.h"

#define T31_AUD_CHN_NUM 2

typedef enum {
    T31_AUD_BIT_16
} t31_aud_bit;

typedef enum {
    T31_AUD_SND_MONO = 1,
    T31_AUD_SND_STEREO = 2
} t31_aud_snd;

typedef struct {
    // Accept industry standards from 8000 to 96000Hz
    int rate;
    t31_aud_bit bit;
    t31_aud_snd mode;
    unsigned int frmNum;
    unsigned int packNumPerFrm;
    unsigned int chnNum;
} t31_aud_cnf;

typedef struct {
    t31_aud_bit bit;
    t31_aud_snd mode;
    unsigned int addr;
    unsigned int phy;
    unsigned long long timestamp;
    unsigned int sequence;
    unsigned int length;
} t31_aud_frm;

typedef struct {
    void *handle;
    
    int (*fnDisableDevice)(int device);
    int (*fnEnableDevice)(int device);
    int (*fnSetDeviceConfig)(int device, t31_aud_cnf *config);

    int (*fnDisableChannel)(int device, int channel);
    int (*fnEnableChannel)(int device, int channel);

    int (*fnSetMute)(int device, int channel, int active);
    int (*fnSetVolume)(int device, int channel, int *dbLevel);

    int (*fnFreeFrame)(int device, int channel, t31_aud_frm *frame);
    int (*fnGetFrame)(int device, int channel, t31_aud_frm *frame, int notBlocking);
} t31_aud_impl;

static int t31_aud_load(t31_aud_impl *aud_lib) {
    if (!(aud_lib->handle = dlopen("libimp.so", RTLD_LAZY | RTLD_GLOBAL))) {
        fprintf(stderr, "[t31_aud] Failed to load library!\nError: %s\n", dlerror());
        return EXIT_FAILURE;
    }

    if (!(aud_lib->fnDisableDevice = (int(*)(int device))
        dlsym(aud_lib->handle, "IMP_AI_Disable"))) {
        fprintf(stderr, "[t31_aud] Failed to acquire symbol IMP_AI_Disable!\n");
        return EXIT_FAILURE;
    }

    if (!(aud_lib->fnEnableDevice = (int(*)(int device))
        dlsym(aud_lib->handle, "IMP_AI_Enable"))) {
        fprintf(stderr, "[t31_aud] Failed to acquire symbol IMP_AI_Enable!\n");
        return EXIT_FAILURE;
    }

    if (!(aud_lib->fnSetDeviceConfig = (int(*)(int device, t31_aud_cnf *config))
        dlsym(aud_lib->handle, "IMP_AI_SetPubAttr"))) {
        fprintf(stderr, "[t31_aud] Failed to acquire symbol IMP_AI_SetPubAttr!\n");
        return EXIT_FAILURE;
    }

    if (!(aud_lib->fnDisableChannel = (int(*)(int device, int channel))
        dlsym(aud_lib->handle, "IMP_AI_DisableChn"))) {
        fprintf(stderr, "[t31_aud] Failed to acquire symbol IMP_AI_DisableChn!\n");
        return EXIT_FAILURE;
    }

    if (!(aud_lib->fnEnableChannel = (int(*)(int device, int channel))
        dlsym(aud_lib->handle, "IMP_AI_EnableChn"))) {
        fprintf(stderr, "[t31_aud] Failed to acquire symbol IMP_AI_EnableChn!\n");
        return EXIT_FAILURE;
    }

    if (!(aud_lib->fnSetMute = (int(*)(int device, int channel, int active))
        dlsym(aud_lib->handle, "IMP_AI_SetVolMute"))) {
        fprintf(stderr, "[t31_aud] Failed to acquire symbol IMP_AI_SetVolMute!\n");
        return EXIT_FAILURE;
    }

    if (!(aud_lib->fnSetVolume = (int(*)(int device, int channel, int *dbLevel))
        dlsym(aud_lib->handle, "IMP_AI_SetVol"))) {
        fprintf(stderr, "[t31_aud] Failed to acquire symbol IMP_AI_SetVol!\n");
        return EXIT_FAILURE;
    }

    if (!(aud_lib->fnFreeFrame = (int(*)(int device, int channel, t31_aud_frm *frame))
        dlsym(aud_lib->handle, "IMP_AI_ReleaseFrame"))) {
        fprintf(stderr, "[t31_aud] Failed to acquire symbol IMP_AI_ReleaseFrame!\n");
        return EXIT_FAILURE;
    }

    if (!(aud_lib->fnGetFrame = (int(*)(int device, int channel, t31_aud_frm *frame, int notBlocking))
        dlsym(aud_lib->handle, "IMP_AI_GetFrame"))) {
        fprintf(stderr, "[t31_aud] Failed to acquire symbol IMP_AI_GetFrame!\n");
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}

static void t31_aud_unload(t31_aud_impl *aud_lib) {
    if (aud_lib->handle) dlclose(aud_lib->handle);
    aud_lib->handle = NULL;
    memset(aud_lib, 0, sizeof(*aud_lib));
}