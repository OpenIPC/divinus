#pragma once

#include "ak_common.h"

typedef struct {
    unsigned int rate;
    unsigned int bit;
    unsigned int chnNum;   
} ak_aud_cnf;

typedef struct {
    unsigned char *data;
    unsigned int length;
    unsigned long long timestamp;
    unsigned long sequence;
} ak_aud_frm;

typedef struct {
    void *handle;
    
    int   (*fnDisableDevice)(void *device);
    void* (*fnEnableDevice)(ak_aud_cnf *config);

    int   (*fnSetVolume)(void *device, int level);

    int   (*fnFreeFrame)(void *device, ak_aud_frm *frame);
    int   (*fnGetFrame)(void *device, ak_aud_frm *frame, int millis);
} ak_aud_impl;

static int ak_aud_load(ak_aud_impl *aud_lib) {
    if (!(aud_lib->handle = dlopen("libplat_ai.so", RTLD_LAZY | RTLD_GLOBAL)))
        HAL_ERROR("ak_aud", "Failed to load library!\nError: %s\n", dlerror());

    if (!(aud_lib->fnDisableDevice = (int(*)(void* device))
        hal_symbol_load("ak_aud", aud_lib->handle, "ak_ai_close")))
        return EXIT_FAILURE;

    if (!(aud_lib->fnEnableDevice = (void*(*)(ak_aud_cnf *config))
        hal_symbol_load("ak_aud", aud_lib->handle, "ak_ai_open")))
        return EXIT_FAILURE;

    if (!(aud_lib->fnSetVolume = (int(*)(void *device, int level))
        hal_symbol_load("ak_aud", aud_lib->handle, "ak_ai_set_volume")))
        return EXIT_FAILURE;

    if (!(aud_lib->fnFreeFrame = (int(*)(void *device, ak_aud_frm *frame))
        hal_symbol_load("ak_aud", aud_lib->handle, "ak_ai_release_frame")))
        return EXIT_FAILURE;

    if (!(aud_lib->fnGetFrame = (int(*)(void *device, ak_aud_frm *frame, int millis))
        hal_symbol_load("ak_aud", aud_lib->handle, "ak_ai_get_frame")))
        return EXIT_FAILURE;

    return EXIT_SUCCESS;
}

static void ak_aud_unload(ak_aud_impl *aud_lib) {
    if (aud_lib->handle) dlclose(aud_lib->handle);
    aud_lib->handle = NULL;
    memset(aud_lib, 0, sizeof(*aud_lib));
}