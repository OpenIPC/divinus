#pragma once

#include "i3_common.h"

typedef struct {
    // Accept industry standards from 8000 to 48000Hz
    int rate;
    // Accepts from 4 to 24 bits
    int bit;
    int stereoOn;
    unsigned int length;
    int leftOrRight;
    int absTimeOn;
} i3_aud_cnf;

typedef struct {
    unsigned char *addr;
    int bit;
    int stereoOn;
    unsigned long long timestamp;
    unsigned int sequence;
    unsigned int length;
    void *reserved;
} i3_aud_frm;

typedef struct {
    void *handle;
    
    int (*fnDisableDevice)(int device);
    int (*fnEnableDevice)(int device);
    int (*fnSetDeviceConfig)(int device, i3_aud_cnf *config);

    int (*fnDisableChannel)(int device, int channel);
    int (*fnEnableChannel)(int device, int channel);

    int (*fnSetMute)(int device, int *muteOn);
    int (*fnSetVolume)(int device, int dbLevel);

    int (*fnFreeFrame)(int device, int channel, i3_aud_frm *frame);
    int (*fnGetFrame)(int device, int channel, i3_aud_frm *frame, int millis);
    int (*fnStartFrame)(int device, int channel);
} i3_aud_impl;

static int i3_aud_load(i3_aud_impl *aud_lib) {
    if (!(aud_lib->handle = dlopen("libmi.so", RTLD_LAZY | RTLD_GLOBAL)))
        HAL_ERROR("i3_aud", "Failed to load library!\nError: %s\n", dlerror());

    if (!(aud_lib->fnDisableDevice = (int(*)(int device))
        hal_symbol_load("i3_aud", aud_lib->handle, "MI_AI_Disable")))
        return EXIT_FAILURE;

    if (!(aud_lib->fnEnableDevice = (int(*)(int device))
        hal_symbol_load("i3_aud", aud_lib->handle, "MI_AI_Enable")))
        return EXIT_FAILURE;

    if (!(aud_lib->fnSetDeviceConfig = (int(*)(int device, i3_aud_cnf *config))
        hal_symbol_load("i3_aud", aud_lib->handle, "MI_AI_SetDevAttr")))
        return EXIT_FAILURE;

    if (!(aud_lib->fnDisableChannel = (int(*)(int device, int channel))
        hal_symbol_load("i3_aud", aud_lib->handle, "MI_AI_DisableChn")))
        return EXIT_FAILURE;

    if (!(aud_lib->fnEnableChannel = (int(*)(int device, int channel))
        hal_symbol_load("i3_aud", aud_lib->handle, "MI_AI_EnableChn")))
        return EXIT_FAILURE;

    if (!(aud_lib->fnSetMute = (int(*)(int device,int *muteOn))
        hal_symbol_load("i3_aud", aud_lib->handle, "MI_AI_SetMute")))
        return EXIT_FAILURE;

    if (!(aud_lib->fnSetVolume = (int(*)(int device, int dbLevel))
        hal_symbol_load("i3_aud", aud_lib->handle, "MI_AI_SetVolume")))
        return EXIT_FAILURE;

    if (!(aud_lib->fnFreeFrame = (int(*)(int device, int channel, i3_aud_frm *frame))
        hal_symbol_load("i3_aud", aud_lib->handle, "MI_AI_ReleaseFrame")))
        return EXIT_FAILURE;

    if (!(aud_lib->fnGetFrame = (int(*)(int device, int channel, i3_aud_frm *frame, int millis))
        hal_symbol_load("i3_aud", aud_lib->handle, "MI_AI_GetFrame")))
        return EXIT_FAILURE;

    if (!(aud_lib->fnStartFrame = (int(*)(int device, int channel))
        hal_symbol_load("i3_aud", aud_lib->handle, "MI_AI_StartFrame")))
        return EXIT_FAILURE;

    return EXIT_SUCCESS;
}

static void i3_aud_unload(i3_aud_impl *aud_lib) {
    if (aud_lib->handle) dlclose(aud_lib->handle);
    aud_lib->handle = NULL;
    memset(aud_lib, 0, sizeof(*aud_lib));
}