#pragma once

#include "f2_common.h"

/* Video input attributes as produced by the sensor driver and consumed by the ISP (28 bytes) */
typedef struct {
    unsigned short vts;        /* frame length in lines */
    unsigned short hts;        /* line length in pixel clocks */
    unsigned short height;
    unsigned short width;
    unsigned short xOffset;
    unsigned short yOffset;
    unsigned short height2;    /* equal to height when not in WDR */
    unsigned short width2;
    unsigned int reserved;
    unsigned int format;       /* bayer/mode word, low 2 bits used */
    unsigned int laneCnt;        /* stored as (laneCnt - 4) & 0xf */
} f2_isp_viattr;

/* libmipi mipi_init() argument */
typedef struct {
    int freqRange;
    int sensorMode;
    int rawType;
    int longFrameVc;           /* 0xff = single frame */
    int shortFrameVc;
    int laneCnt;
} f2_isp_mipi;

/*
 * Sensor operations table registered with API_ISP_SensorRegCb() (0x68 bytes,
 * copied by value into the ISP core). Optional entries may be NULL. Under the
 * V200 model the vendor sensor library exports this table; see f2_snr.h.
 */
typedef struct {
    const char *name;
    int (*pfnSetGain)(unsigned int gain);                /* 0x04  gain in 1/64 steps (64 = 1x) */
    int (*pfnGetInputAttr)(f2_isp_viattr *attr);
    int (*pfnGetGain)(unsigned int *gain);
    int (*pfnSetIntegration)(unsigned int lines);
    int (*pfnSetFrameHeight)(int multiplier);            /* 0x14  VTS = nominal VTS * multiplier */
    int (*pfnGetIntegration)(unsigned int *lines);
    int (*pfnSetFlipMirror)(unsigned int bits);          /* 0x1c  bit0 flip, bit1 mirror */
    int (*pfnGetFlipMirror)(unsigned int *bits);
    void *reserved24;
    int (*pfnInit)(void);
    int (*pfnReset)(void);
    int (*pfnDeinit)(void);
    int (*pfnSetFormat)(int format);
    int (*pfnKick)(void);                                /* 0x38  optional */
    int (*pfnSetRegister)(unsigned int reg, unsigned int val);
    void *reserved40;
    int (*pfnSetExposureRatio)(void *ratio);             /* 0x44  WDR, optional */
    int (*pfnGetExposureRatio)(void *ratio);             /* 0x48  WDR, optional */
    void *reserved4c;
    int (*pfnSetChipId)(unsigned int id);
    int (*pfnGetRegister)(unsigned int reg, unsigned short *val);
    int (*pfnGetAwbGain)(unsigned int rgb[3]);           /* 0x58  optional */
    int (*pfnSetAwbGain)(unsigned int rgb[3]);           /* 0x5c  optional */
    void *reserved60;
    void *reserved64;
} f2_isp_snrops;

typedef struct {
    unsigned int frameStart;   /* counters */
    unsigned int frameEnd;
    unsigned int framerate;
    unsigned int width;
    unsigned int height;
    unsigned int reserved;
    unsigned int overflow;
} f2_isp_vistate;

typedef struct {
    const char *name;
    f2_common_dim dim;
    unsigned char fps;
    unsigned int format;
} f2_snr_mode;

static const f2_snr_mode f2_snr_modes[] = {
    { "gc4653_mipi",    { .width = 1920, .height = 1080 }, 25, 0x203 },
    { "gc4653_mipi",    { .width = 1920, .height = 1080 }, 30, 0x204 },
    { "gc4653_mipi",    { .width = 2560, .height = 1440 }, 20, 0x602 },
    { "gc4653_mipi",    { .width = 2560, .height = 1440 }, 25, 0x603 },
    { "gc5603_mipi",    { .width = 2880, .height = 1616 },  0, F2_SNR_FMT },
    { "jxf32_mipi",     { .width = 1920, .height = 1080 },  0, F2_SNR_FMT },
    { "mn34425_mipi",   { .width = 1920, .height = 1080 },  0, F2_SNR_FMT },
    { "ovos02k_mipi",   { .width = 1920, .height = 1080 },  0, F2_SNR_FMT },
    { "ovos04c10_mipi", { .width = 2560, .height = 1440 },  0, F2_SNR_FMT },
    { "ovos05_mipi",    { .width = 2592, .height = 1944 },  0, F2_SNR_FMT },
    { "ovos08_mipi",    { .width = 3840, .height = 2160 },  0, F2_SNR_FMT },
};

typedef struct {
    void *handle, *handleCore, *handleMipi, *handleAdv, *handleCommon, *handleSmartIr;

    int (*fnAeSendCommand)(unsigned int cmd, void *arg);
    int (*fnExit)(void);
    int (*fnGetInputAttr)(f2_isp_viattr *attr);
    int (*fnGetInputState)(f2_isp_vistate *state);
    int (*fnInit)(void);
    int (*fnLoadParam)(void *param);
    int (*fnMemInit)(unsigned int width, unsigned int height);
    int (*fnRegisterSensor)(unsigned int index, f2_isp_snrops *ops);
    int (*fnRun)(void);
    int (*fnSensorInit)(void);
    int (*fnEnableSensor)(void);
    int (*fnSetFlipMirrorEx)(unsigned int mirror, unsigned int flip, unsigned int bayer);
    int (*fnSetSensorFormat)(unsigned int format);
    int (*fnTelemetry)(unsigned int *framerate, unsigned int *width, unsigned int *height);
    int (*fnUnregisterSensor)(unsigned int index);

    int (*fnAdvInit)(void);
    int (*fnAdvSensorInit)(void);
    int (*fnAdvSetColorMode)(int color);
    int (*fnGetSaturation)(void *sat);   /* record, byte 5 nonzero = colour */
    int (*fnSetSaturation)(void *sat);

    int (*fnSmartIrInit)(void);
    int (*fnSmartIrGetConfig)(void *cfg);
    int (*fnSmartIrSetConfig)(void *cfg);
    unsigned char (*fnSmartIrStatus)(int prevStatus);
    int (*fnGetAeInfo)(void *info);      /* [1] gain, [3] exposure, [4] total (1/64) */
    void (*fnMipiInit)(f2_isp_mipi *config);
} f2_isp_impl;

static int f2_isp_load(f2_isp_impl *isp_lib) {
    /* libisp resolves isp_core_* against libispcore at load time */
    if (!(isp_lib->handleCore = dlopen("libispcore.so", RTLD_NOW | RTLD_GLOBAL)))
        HAL_ERROR("f2_isp", "Failed to load library!\nError: %s\n", dlerror());

    if (!(isp_lib->handle = dlopen("libisp.so", RTLD_NOW | RTLD_GLOBAL)))
        HAL_ERROR("f2_isp", "Failed to load library!\nError: %s\n", dlerror());

    if (!(isp_lib->handleMipi = dlopen("libmipi.so", RTLD_NOW | RTLD_GLOBAL)))
        HAL_ERROR("f2_isp", "Failed to load library!\nError: %s\n", dlerror());

    /* libadvapi_isp/libadvapi_smartir resolve fh_sys_* against libadvapi */
    if (!(isp_lib->handleCommon = dlopen("libadvapi.so", RTLD_NOW | RTLD_GLOBAL)))
        HAL_ERROR("f2_isp", "Failed to load library!\nError: %s\n", dlerror());

    if (!(isp_lib->handleAdv = dlopen("libadvapi_isp.so", RTLD_NOW | RTLD_GLOBAL)))
        HAL_ERROR("f2_isp", "Failed to load library!\nError: %s\n", dlerror());

    if (!(isp_lib->handleSmartIr = dlopen("libadvapi_smartir.so", RTLD_NOW | RTLD_GLOBAL)))
        HAL_WARNING("f2_isp", "SmartIR library unavailable, day/night by gain disabled\n");

    /* Optional: the V200 libisp only exports AFSendCmd, no AE command hook */
    isp_lib->fnAeSendCommand = (int(*)(unsigned int, void *))dlsym(isp_lib->handle, "API_ISP_AESendCmd");

    if (!(isp_lib->fnExit = (int(*)(void))
        hal_symbol_load("f2_isp", isp_lib->handle, "API_ISP_Exit")))
        return EXIT_FAILURE;

    if (!(isp_lib->fnGetInputAttr = (int(*)(f2_isp_viattr *))
        hal_symbol_load("f2_isp", isp_lib->handle, "API_ISP_GetViAttr")))
        return EXIT_FAILURE;

    if (!(isp_lib->fnGetInputState = (int(*)(f2_isp_vistate *))
        hal_symbol_load("f2_isp", isp_lib->handle, "API_ISP_GetVIState")))
        return EXIT_FAILURE;

    if (!(isp_lib->fnInit = (int(*)(void))
        hal_symbol_load("f2_isp", isp_lib->handle, "API_ISP_Init")))
        return EXIT_FAILURE;

    if (!(isp_lib->fnLoadParam = (int(*)(void *))
        hal_symbol_load("f2_isp", isp_lib->handle, "API_ISP_LoadIspParam")))
        return EXIT_FAILURE;

    if (!(isp_lib->fnMemInit = (int(*)(unsigned int, unsigned int))
        hal_symbol_load("f2_isp", isp_lib->handle, "API_ISP_MemInit")))
        return EXIT_FAILURE;

    if (!(isp_lib->fnRegisterSensor = (int(*)(unsigned int, f2_isp_snrops *))
        hal_symbol_load("f2_isp", isp_lib->handle, "API_ISP_SensorRegCb")))
        return EXIT_FAILURE;

    if (!(isp_lib->fnRun = (int(*)(void))
        hal_symbol_load("f2_isp", isp_lib->handle, "API_ISP_Run")))
        return EXIT_FAILURE;

    if (!(isp_lib->fnSensorInit = (int(*)(void))
        hal_symbol_load("f2_isp", isp_lib->handle, "API_ISP_SensorInit")))
        return EXIT_FAILURE;

    if (!(isp_lib->fnEnableSensor = (int(*)(void))
        hal_symbol_load("f2_isp", isp_lib->handle, "API_ISP_SensorKick")))
        return EXIT_FAILURE;

    if (!(isp_lib->fnSetFlipMirrorEx = (int(*)(unsigned int, unsigned int, unsigned int))
        hal_symbol_load("f2_isp", isp_lib->handle, "API_ISP_SetMirrorAndflipEx")))
        return EXIT_FAILURE;

    if (!(isp_lib->fnSetSensorFormat = (int(*)(unsigned int))
        hal_symbol_load("f2_isp", isp_lib->handle, "API_ISP_SetSensorFmt")))
        return EXIT_FAILURE;

    if (!(isp_lib->fnUnregisterSensor = (int(*)(unsigned int))
        hal_symbol_load("f2_isp", isp_lib->handle, "API_ISP_SensorUnRegCb")))
        return EXIT_FAILURE;

    isp_lib->fnTelemetry = (int(*)(unsigned int*, unsigned int*, unsigned int*))
        dlsym(isp_lib->handle, "API_ISP_GetViTelemetry");

    if (!(isp_lib->fnAdvInit = (int(*)(void))
        hal_symbol_load("f2_isp", isp_lib->handleAdv, "FHAdv_Isp_Init")))
        return EXIT_FAILURE;

    if (!(isp_lib->fnAdvSensorInit = (int(*)(void))
        hal_symbol_load("f2_isp", isp_lib->handleAdv, "FHAdv_Isp_SensorInit")))
        return EXIT_FAILURE;

    if (!(isp_lib->fnGetSaturation = (int(*)(void *))
        hal_symbol_load("f2_isp", isp_lib->handle, "API_ISP_GetSaturation")))
        return EXIT_FAILURE;

    if (!(isp_lib->fnSetSaturation = (int(*)(void *))
        hal_symbol_load("f2_isp", isp_lib->handle, "API_ISP_SetSaturation")))
        return EXIT_FAILURE;

    if (!(isp_lib->fnAdvSetColorMode = (int(*)(int))
        hal_symbol_load("f2_isp", isp_lib->handleAdv, "FHAdv_Isp_SetColorMode")))
        return EXIT_FAILURE;

    if (!(isp_lib->fnMipiInit = (void(*)(f2_isp_mipi *))
        hal_symbol_load("f2_isp", isp_lib->handleMipi, "mipi_init")))
        return EXIT_FAILURE;

    isp_lib->fnGetAeInfo = (int(*)(void*))dlsym(isp_lib->handle, "API_ISP_GetAeInfo");

    if (isp_lib->handleSmartIr) {
        isp_lib->fnSmartIrInit = (int(*)(void))dlsym(isp_lib->handleSmartIr, "FHAdv_SmartIR_Init");
        isp_lib->fnSmartIrStatus = (unsigned char(*)(int))dlsym(isp_lib->handleSmartIr, "FHAdv_SmartIR_GetDayNightStatus");
        isp_lib->fnSmartIrGetConfig = (int(*)(void*))dlsym(isp_lib->handleSmartIr, "FHAdv_SmartIR_GetCfg");
        isp_lib->fnSmartIrSetConfig = (int(*)(void*))dlsym(isp_lib->handleSmartIr, "FHAdv_SmartIR_SetCfg");
    }

    return EXIT_SUCCESS;
}

static void f2_isp_unload(f2_isp_impl *isp_lib) {
    if (isp_lib->handleSmartIr) dlclose(isp_lib->handleSmartIr);
    if (isp_lib->handleAdv) dlclose(isp_lib->handleAdv);
    if (isp_lib->handleCommon) dlclose(isp_lib->handleCommon);
    if (isp_lib->handleMipi) dlclose(isp_lib->handleMipi);
    if (isp_lib->handle) dlclose(isp_lib->handle);
    if (isp_lib->handleCore) dlclose(isp_lib->handleCore);
    memset(isp_lib, 0, sizeof(*isp_lib));
}
