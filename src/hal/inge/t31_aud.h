#pragma once

#include "t31_common.h"

#define T31_AUD_CHN_NUM 2

typedef enum {
    T31_AUD_BIT_16 = 16
} t31_aud_bit;

typedef enum {
    T31_AUD_SND_MONO = 1,
    T31_AUD_SND_STEREO = 2
} t31_aud_snd;

typedef struct {
    int usrFrmDepth;
    int rev;
} t31_aud_chn;

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
    int (*fnSetChannelConfig)(int device, int channel, t31_aud_chn *config);

    int (*fnSetGain)(int device, int channel, int level);
    int (*fnSetMute)(int device, int channel, int active);
    int (*fnSetVolume)(int device, int channel, int level);

    int (*fnFreeFrame)(int device, int channel, t31_aud_frm *frame);
    int (*fnGetFrame)(int device, int channel, t31_aud_frm *frame, int notBlocking);
    int (*fnPollFrame)(int device, int channel, int timeout);
} t31_aud_impl;

static int t31_aud_load(t31_aud_impl *aud_lib) {
    if (!(aud_lib->handle = dlopen("libimp.so", RTLD_LAZY | RTLD_GLOBAL)))
        HAL_ERROR("t31_aud", "Failed to load library!\nError: %s\n", dlerror());

    if (!(aud_lib->fnDisableDevice = (int(*)(int device))
        hal_symbol_load("t31_aud", aud_lib->handle, "IMP_AI_Disable")))
        return EXIT_FAILURE;

    if (!(aud_lib->fnEnableDevice = (int(*)(int device))
        hal_symbol_load("t31_aud", aud_lib->handle, "IMP_AI_Enable")))
        return EXIT_FAILURE;

    if (!(aud_lib->fnSetDeviceConfig = (int(*)(int device, t31_aud_cnf *config))
        hal_symbol_load("t31_aud", aud_lib->handle, "IMP_AI_SetPubAttr")))
        return EXIT_FAILURE;

    if (!(aud_lib->fnDisableChannel = (int(*)(int device, int channel))
        hal_symbol_load("t31_aud", aud_lib->handle, "IMP_AI_DisableChn")))
        return EXIT_FAILURE;

    if (!(aud_lib->fnEnableChannel = (int(*)(int device, int channel))
        hal_symbol_load("t31_aud", aud_lib->handle, "IMP_AI_EnableChn")))
        return EXIT_FAILURE;

    if (!(aud_lib->fnSetChannelConfig = (int(*)(int device, int channel, t31_aud_chn *config))
        hal_symbol_load("t31_aud", aud_lib->handle, "IMP_AI_SetChnParam")))
        return EXIT_FAILURE;

    if (!(aud_lib->fnSetGain = (int(*)(int device, int channel, int level))
        hal_symbol_load("t31_aud", aud_lib->handle, "IMP_AI_SetGain")))
        return EXIT_FAILURE;

    if (!(aud_lib->fnSetMute = (int(*)(int device, int channel, int active))
        hal_symbol_load("t31_aud", aud_lib->handle, "IMP_AI_SetVolMute")))
        return EXIT_FAILURE;

    if (!(aud_lib->fnSetVolume = (int(*)(int device, int channel, int level))
        hal_symbol_load("t31_aud", aud_lib->handle, "IMP_AI_SetVol")))
        return EXIT_FAILURE;

    if (!(aud_lib->fnFreeFrame = (int(*)(int device, int channel, t31_aud_frm *frame))
        hal_symbol_load("t31_aud", aud_lib->handle, "IMP_AI_ReleaseFrame")))
        return EXIT_FAILURE;

    if (!(aud_lib->fnGetFrame = (int(*)(int device, int channel, t31_aud_frm *frame, int notBlocking))
        hal_symbol_load("t31_aud", aud_lib->handle, "IMP_AI_GetFrame")))
        return EXIT_FAILURE;

    if (!(aud_lib->fnPollFrame = (int(*)(int device, int channel, int timeout))
        hal_symbol_load("t31_aud", aud_lib->handle, "IMP_AI_PollingFrame")))
        return EXIT_FAILURE;

    return EXIT_SUCCESS;
}

static void t31_aud_unload(t31_aud_impl *aud_lib) {
    if (aud_lib->handle) dlclose(aud_lib->handle);
    aud_lib->handle = NULL;
    memset(aud_lib, 0, sizeof(*aud_lib));
}