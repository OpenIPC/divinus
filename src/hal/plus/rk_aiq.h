#pragma once

#include "rk_common.h"

#include <stdbool.h>

typedef void rk_aiq_ctx;

typedef enum {
    RK_AIQ_WBL_AUTO,
    RK_AIQ_WBL_MANUAL,
    RK_AIQ_WBL_DAYLIGHT,
    RK_AIQ_WBL_CLOUDY,
    RK_AIQ_WBL_TUNGSTEN,
    RK_AIQ_WBL_FLUORESCENT
} rk_aiq_wbl;

typedef enum {
    RK_AIQ_WORK_NORMAL,
    RK_AIQ_WORK_ISP_HDR2,
    RK_AIQ_WORK_ISP_HDR3
} rk_aiq_work;

typedef struct {
    int mode;
    struct {
        float redGain, greenGain, blueGain;
        int colorTemp;
    } para;
} rk_aiq_mwb;

typedef struct {
    float min, max;
} rk_aiq_rng;

typedef struct {
    void *handle;

    rk_aiq_ctx *(*fnInit)(const char *sensor, const char *iqDir, void *errCb, void *metaCb);
    int (*fnPreInitBuf)(const char *sensor, const char *device, int count);
    int (*fnPreInitScene)(const char *sensor, const char *main, const char *sub);
    int (*fnDeinit)(rk_aiq_ctx *context);
    const char*(*fnGetSensorFromV4l2)(const char *device);
    int (*fnPrepare)(rk_aiq_ctx *context, int width, int height, rk_aiq_work work);
    int (*fnStart)(rk_aiq_ctx *context);
    int (*fnStop)(rk_aiq_ctx *context);
    int (*fnGetVersion)(char **version);

    int (*fnGetExpMode)(const rk_aiq_ctx *context, int *autoMode);
    int (*fnSetExpMode)(const rk_aiq_ctx *context, int autoMode);
    int (*fnGetExpGainRange)(const rk_aiq_ctx *context, rk_aiq_rng *gain);
    int (*fnGetExpTimeRange)(const rk_aiq_ctx *context, rk_aiq_rng *time);

    int (*fnGetWhiteBalMode)(const rk_aiq_ctx *context, rk_aiq_wbl *whiteBal);
    int (*fnSetWhiteBalMode)(const rk_aiq_ctx *context, rk_aiq_wbl whiteBal);
    int (*fnGetManualWhiteBal)(const rk_aiq_ctx *context, rk_aiq_mwb *config);
    int (*fnSetManualWhiteBal)(const rk_aiq_ctx *context, rk_aiq_mwb config);

    int (*fnGetBrightness)(const rk_aiq_ctx *context, int *brightness);
    int (*fnSetBrightness)(const rk_aiq_ctx *context, int brightness);
    int (*fnGetContrast)(const rk_aiq_ctx *context, int *contrast);
    int (*fnSetContrast)(const rk_aiq_ctx *context, int contrast);
    int (*fnGetSaturation)(const rk_aiq_ctx *context, int *saturation);
    int (*fnSetSaturation)(const rk_aiq_ctx *context, int saturation);
    int (*fnGetSharpness)(const rk_aiq_ctx *context, int *sharpness);
    int (*fnSetSharpness)(const rk_aiq_ctx *context, int sharpness);
    int (*fnSetMirrorFlip)(const rk_aiq_ctx *context, bool mirror, bool flip, int skipFrmCnt);
} rk_aiq_impl;


static int rk_aiq_load(rk_aiq_impl *aiq_lib) {
    if (!(aiq_lib->handle = dlopen("librkaiq.so", RTLD_LAZY | RTLD_GLOBAL)))
        HAL_ERROR("rk_aiq", "Failed to load library!\nError: %s\n", dlerror());

    if (!(aiq_lib->fnInit = (rk_aiq_ctx*(*)(const char*, const char*, void*, void*))
        hal_symbol_load("rk_aiq", aiq_lib->handle, "rk_aiq_uapi2_sysctl_init")))
        return EXIT_FAILURE;

    if (!(aiq_lib->fnPreInitBuf = (int(*)(const char*, const char*, int))
        hal_symbol_load("rk_aiq", aiq_lib->handle, "rk_aiq_uapi2_sysctl_preInit_devBufCnt")))
        return EXIT_FAILURE;

    if (!(aiq_lib->fnPreInitScene = (int(*)(const char*, const char*, const char*))
        hal_symbol_load("rk_aiq", aiq_lib->handle, "rk_aiq_uapi2_sysctl_preInit_scene")))
        return EXIT_FAILURE;

    if (!(aiq_lib->fnDeinit = (int(*)(rk_aiq_ctx*))
        hal_symbol_load("rk_aiq", aiq_lib->handle, "rk_aiq_uapi2_sysctl_deinit")))
        return EXIT_FAILURE;

    if (!(aiq_lib->fnGetSensorFromV4l2 = (const char*(*)(const char*))
        hal_symbol_load("rk_aiq", aiq_lib->handle, "rk_aiq_uapi2_sysctl_getBindedSnsEntNmByVd")))
        return EXIT_FAILURE;

    if (!(aiq_lib->fnPrepare = (int(*)(rk_aiq_ctx*, int, int, rk_aiq_work))
        hal_symbol_load("rk_aiq", aiq_lib->handle, "rk_aiq_uapi2_sysctl_prepare")))
        return EXIT_FAILURE;

    if (!(aiq_lib->fnStart = (int(*)(rk_aiq_ctx*))
        hal_symbol_load("rk_aiq", aiq_lib->handle, "rk_aiq_uapi2_sysctl_start")))
        return EXIT_FAILURE;

    if (!(aiq_lib->fnStop = (int(*)(rk_aiq_ctx*))
        hal_symbol_load("rk_aiq", aiq_lib->handle, "rk_aiq_uapi2_sysctl_stop")))
        return EXIT_FAILURE;

    if (!(aiq_lib->fnGetVersion = (int(*)(char**))
        hal_symbol_load("rk_aiq", aiq_lib->handle, "rk_aiq_uapi2_get_version_info")))
        return EXIT_FAILURE;

    if (!(aiq_lib->fnGetExpMode = (int(*)(const rk_aiq_ctx*, int*))
        hal_symbol_load("rk_aiq", aiq_lib->handle, "rk_aiq_uapi2_getExpMode")))
        return EXIT_FAILURE;

    if (!(aiq_lib->fnSetExpMode = (int(*)(const rk_aiq_ctx*, int))
        hal_symbol_load("rk_aiq", aiq_lib->handle, "rk_aiq_uapi2_setExpMode")))
        return EXIT_FAILURE;

    if (!(aiq_lib->fnGetExpGainRange = (int(*)(const rk_aiq_ctx*, rk_aiq_rng*))
        hal_symbol_load("rk_aiq", aiq_lib->handle, "rk_aiq_uapi2_getExpGainRange")))
        return EXIT_FAILURE;

    if (!(aiq_lib->fnGetExpTimeRange = (int(*)(const rk_aiq_ctx*, rk_aiq_rng*))
        hal_symbol_load("rk_aiq", aiq_lib->handle, "rk_aiq_uapi2_getExpTimeRange")))
        return EXIT_FAILURE;

    if (!(aiq_lib->fnGetWhiteBalMode = (int(*)(const rk_aiq_ctx*, rk_aiq_wbl*))
        hal_symbol_load("rk_aiq", aiq_lib->handle, "rk_aiq_uapi2_getWBMode")))
        return EXIT_FAILURE;

    if (!(aiq_lib->fnSetWhiteBalMode = (int(*)(const rk_aiq_ctx*, rk_aiq_wbl))
        hal_symbol_load("rk_aiq", aiq_lib->handle, "rk_aiq_uapi2_setWBMode")))
        return EXIT_FAILURE;

    if (!(aiq_lib->fnGetManualWhiteBal = (int(*)(const rk_aiq_ctx*, rk_aiq_mwb*))
        hal_symbol_load("rk_aiq", aiq_lib->handle, "rk_aiq_user_api2_awb_GetMwbAttrib")))
        return EXIT_FAILURE;

    if (!(aiq_lib->fnSetManualWhiteBal = (int(*)(const rk_aiq_ctx*, rk_aiq_mwb))
        hal_symbol_load("rk_aiq", aiq_lib->handle, "rk_aiq_user_api2_awb_SetMwbAttrib")))
        return EXIT_FAILURE;

    if (!(aiq_lib->fnGetBrightness = (int(*)(const rk_aiq_ctx*, int*))
        hal_symbol_load("rk_aiq", aiq_lib->handle, "rk_aiq_uapi2_getBrightness")))
        return EXIT_FAILURE;

    if (!(aiq_lib->fnSetBrightness = (int(*)(const rk_aiq_ctx*, int))
        hal_symbol_load("rk_aiq", aiq_lib->handle, "rk_aiq_uapi2_setBrightness")))
        return EXIT_FAILURE;

    if (!(aiq_lib->fnGetContrast = (int(*)(const rk_aiq_ctx*, int*))
        hal_symbol_load("rk_aiq", aiq_lib->handle, "rk_aiq_uapi2_getContrast")))
        return EXIT_FAILURE;

    if (!(aiq_lib->fnSetContrast = (int(*)(const rk_aiq_ctx*, int))
        hal_symbol_load("rk_aiq", aiq_lib->handle, "rk_aiq_uapi2_setContrast")))
        return EXIT_FAILURE;

    if (!(aiq_lib->fnGetSaturation = (int(*)(const rk_aiq_ctx*, int*))
        hal_symbol_load("rk_aiq", aiq_lib->handle, "rk_aiq_uapi2_getSaturation")))
        return EXIT_FAILURE;

    if (!(aiq_lib->fnSetSaturation = (int(*)(const rk_aiq_ctx*, int))
        hal_symbol_load("rk_aiq", aiq_lib->handle, "rk_aiq_uapi2_setSaturation")))
        return EXIT_FAILURE;

    if (!(aiq_lib->fnGetSharpness = (int(*)(const rk_aiq_ctx*, int*))
        hal_symbol_load("rk_aiq", aiq_lib->handle, "rk_aiq_uapi2_getSharpness")))
        return EXIT_FAILURE;

    if (!(aiq_lib->fnSetSharpness = (int(*)(const rk_aiq_ctx*, int))
        hal_symbol_load("rk_aiq", aiq_lib->handle, "rk_aiq_uapi2_setSharpness")))
        return EXIT_FAILURE;

    if (!(aiq_lib->fnSetMirrorFlip = (int(*)(const rk_aiq_ctx*, bool, bool, int))
        hal_symbol_load("rk_aiq", aiq_lib->handle, "rk_aiq_uapi2_setMirrorFlip")))
        return EXIT_FAILURE;

    return EXIT_SUCCESS;
}

static void rk_aiq_unload(rk_aiq_impl *aiq_lib) {
    if (aiq_lib->handle) dlclose(aiq_lib->handle);
    aiq_lib->handle = NULL;

    memset(aiq_lib, 0, sizeof(*aiq_lib));
}
