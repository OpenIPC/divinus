#pragma once

#include "ak_common.h"

typedef struct {
    int x;
    int y;
    int width;
    int height;
} ak_vi_crop;

typedef struct {
    int width;
    int height;
    int maxWidth;
    int maxHeight;
} ak_vi_res;

typedef struct {
    ak_vi_crop capt;
    ak_vi_res dest[AK_VI_CHN_NUM];
} ak_vi_cnf;

typedef struct {
    void *handle, *handleIspSdk;

    int   (*fnLoadSensorConfig)(char *path);

    int   (*fnDisableDevice)(void *device);
    void* (*fnEnableDevice)(int device);
    int   (*fnStartDevice)(void *device);
    int   (*fnStopDevice)(void *device);

    int   (*fnGetDeviceConfig)(void *device, ak_vi_cnf *config);
    int   (*fnGetDeviceResolution)(void *device, ak_vi_res *res);
    int   (*fnSetDeviceConfig)(void *device, ak_vi_cnf *config);
    int   (*fnSetDeviceFlipMirror)(void *device, int flipOn, int mirrorOn);
    int   (*fnSetDeviceMode)(void *device, int nightOn);
} ak_vi_impl;

static int ak_vi_load(ak_vi_impl *vi_lib) {
    if (!(vi_lib->handleIspSdk = dlopen("libakispsdk.so", RTLD_LAZY | RTLD_GLOBAL)))
        HAL_ERROR("ak_vi", "Failed to load library!\nError: %s\n", dlerror());

    if (!(vi_lib->handle = dlopen("libplat_vi.so", RTLD_LAZY | RTLD_GLOBAL)))
        HAL_ERROR("ak_vi", "Failed to load library!\nError: %s\n", dlerror());

    if (!(vi_lib->fnLoadSensorConfig = (int(*)(char *path))
        hal_symbol_load("ak_vi", vi_lib->handle, "ak_vi_match_sensor")))
        return EXIT_FAILURE;

    if (!(vi_lib->fnDisableDevice = (int(*)(void* device))
        hal_symbol_load("ak_vi", vi_lib->handle, "ak_vi_close")))
        return EXIT_FAILURE;

    if (!(vi_lib->fnEnableDevice = (void*(*)(int device))
        hal_symbol_load("ak_vi", vi_lib->handle, "ak_vi_open")))
        return EXIT_FAILURE;

    if (!(vi_lib->fnStartDevice = (int(*)(void *device))
        hal_symbol_load("ak_vi", vi_lib->handle, "ak_vi_capture_on")))
        return EXIT_FAILURE;

    if (!(vi_lib->fnStopDevice = (int(*)(void *device))
        hal_symbol_load("ak_vi", vi_lib->handle, "ak_vi_capture_off")))
        return EXIT_FAILURE;

    if (!(vi_lib->fnGetDeviceConfig = (int(*)(void* device, ak_vi_cnf *config))
        hal_symbol_load("ak_vi", vi_lib->handle, "ak_vi_get_channel_attr")))
        return EXIT_FAILURE;

    if (!(vi_lib->fnGetDeviceResolution = (int(*)(void* device, ak_vi_res *res))
        hal_symbol_load("ak_vi", vi_lib->handle, "ak_vi_get_sensor_resolution")))
        return EXIT_FAILURE;

    if (!(vi_lib->fnSetDeviceConfig = (int(*)(void* device, ak_vi_cnf *config))
        hal_symbol_load("ak_vi", vi_lib->handle, "ak_vi_set_channel_attr")))
        return EXIT_FAILURE;

    if (!(vi_lib->fnSetDeviceFlipMirror = (int(*)(void *device, int flipOn, int mirrorOn))
        hal_symbol_load("ak_vi", vi_lib->handle, "ak_vi_set_flip_mirror")))
        return EXIT_FAILURE;

    if (!(vi_lib->fnSetDeviceMode = (int(*)(void *device, int nightOn))
        hal_symbol_load("ak_vi", vi_lib->handle, "ak_vi_switch_mode")))
        return EXIT_FAILURE;

    return EXIT_SUCCESS;
}

static void ak_vi_unload(ak_vi_impl *vi_lib) {
    if (vi_lib->handle) dlclose(vi_lib->handle);
    vi_lib->handle = NULL;
    if (vi_lib->handleIspSdk) dlclose(vi_lib->handleIspSdk);
    vi_lib->handleIspSdk = NULL;
    memset(vi_lib, 0, sizeof(*vi_lib));
}