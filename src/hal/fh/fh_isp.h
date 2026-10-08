#pragma once

#include "fh_common.h"

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
    unsigned int lanes;        /* stored as (lanes - 4) & 0xf */
} fh_isp_viattr;

/* libmipi mipi_init() argument */
typedef struct {
    int freqRange;
    int sensorMode;
    int rawType;
    int longFrameVc;           /* 0xff = single frame */
    int shortFrameVc;
    int laneNum;
} fh_isp_mipi;

/*
 * Sensor operations table registered with API_ISP_SensorRegCb() (0x68 bytes,
 * copied by value into the ISP core). Optional entries may be NULL.
 */
typedef struct {
    const char *name;                                   /* 0x00 */
    int (*fnSetGain)(unsigned int gain);                /* 0x04  gain in 1/64 steps (64 = 1x) */
    int (*fnGetInputAttr)(fh_isp_viattr *attr);         /* 0x08 */
    int (*fnGetGain)(unsigned int *gain);               /* 0x0c */
    int (*fnSetIntegration)(unsigned int lines);        /* 0x10 */
    int (*fnSetFrameHeight)(int multiplier);            /* 0x14  VTS = nominal VTS * multiplier */
    int (*fnGetIntegration)(unsigned int *lines);       /* 0x18 */
    int (*fnSetFlipMirror)(unsigned int bits);          /* 0x1c  bit0 flip, bit1 mirror */
    int (*fnGetFlipMirror)(unsigned int *bits);         /* 0x20 */
    void *reserved24;
    int (*fnInit)(void);                                /* 0x28 */
    int (*fnReset)(void);                               /* 0x2c */
    int (*fnDeinit)(void);                              /* 0x30 */
    int (*fnSetFormat)(int format);                     /* 0x34 */
    int (*fnKick)(void);                                /* 0x38  optional */
    int (*fnSetRegister)(unsigned int reg, unsigned int val); /* 0x3c */
    void *reserved40;
    int (*fnSetExposureRatio)(void *ratio);             /* 0x44  WDR, optional */
    int (*fnGetExposureRatio)(void *ratio);             /* 0x48  WDR, optional */
    void *reserved4c;
    int (*fnSetChipId)(unsigned int id);                /* 0x50 */
    int (*fnGetRegister)(unsigned int reg, unsigned short *val); /* 0x54 */
    int (*fnGetAwbGain)(unsigned int rgb[3]);           /* 0x58  optional */
    int (*fnSetAwbGain)(unsigned int rgb[3]);           /* 0x5c  optional */
    void *reserved60;
    void *reserved64;
} fh_isp_snrops;

typedef struct {
    unsigned int frameStart;   /* counters */
    unsigned int frameEnd;
    unsigned int framerate;
    unsigned int width;
    unsigned int height;
    unsigned int reserved;
    unsigned int overflow;
} fh_isp_vistate;

typedef struct {
    void *handle, *handleCore, *handleMipi, *handleAdv;

    int (*fnEnableAe)(unsigned int enable);
    int (*fnAeSendCmd)(unsigned int cmd, void *arg);
    int (*fnEnableAwb)(unsigned int enable);
    int (*fnExit)(void);
    int (*fnGetInputAttr)(fh_isp_viattr *attr);
    int (*fnGetInputState)(fh_isp_vistate *state);
    int (*fnInit)(void);
    int (*fnLoadParam)(void *param);
    int (*fnMemInit)(unsigned int width, unsigned int height);
    int (*fnRegisterSensor)(unsigned int index, fh_isp_snrops *ops);
    int (*fnRun)(void);
    int (*fnSensorInit)(void);
    int (*fnSetFlipMirror)(unsigned int mirror, unsigned int flip);
    int (*fnSetFlipMirrorEx)(unsigned int mirror, unsigned int flip, unsigned int bayer);
    int (*fnSetFramerateDiv)(unsigned int div);
    int (*fnSetSensorFormat)(unsigned int format);
    int (*fnUnregisterSensor)(unsigned int index);

    int (*fnAdvInit)(void);
    int (*fnSmartIrInit)(void);
    int (*fnSmartIrSetAttr)(int rgbir);
    unsigned char (*fnSmartIrStatus)(int prevStatus);
    int (*fnSmartIrGetThreshold)(unsigned short *th);   /* 4 x u16, optional */
    int (*fnSmartIrSetThreshold)(unsigned short *th);
    int (*fnGetAeInfo)(void *info);                     /* [1] gain, [3] exposure, [4] total (1/64) */
    int (*fnAdvSetColorMode)(int color);
    int (*fnGetSaturation)(void *sat);   /* 20-byte record, byte 5 nonzero = colour */
    int (*fnSetSaturation)(void *sat);
    void (*fnMipiInit)(fh_isp_mipi *config);
} fh_isp_impl;

static int fh_isp_load(fh_isp_impl *isp_lib) {
    /* libisp resolves isp_core_* against libispcore at load time */
    if (!(isp_lib->handleCore = dlopen("libispcore.so", RTLD_NOW | RTLD_GLOBAL)))
        HAL_ERROR("fh_isp", "Failed to load library!\nError: %s\n", dlerror());

    if (!(isp_lib->handle = dlopen("libisp.so", RTLD_NOW | RTLD_GLOBAL)))
        HAL_ERROR("fh_isp", "Failed to load library!\nError: %s\n", dlerror());

    if (!(isp_lib->handleMipi = dlopen("libmipi.so", RTLD_NOW | RTLD_GLOBAL)))
        HAL_ERROR("fh_isp", "Failed to load library!\nError: %s\n", dlerror());

    if (!(isp_lib->handleAdv = dlopen("libadvapi.so", RTLD_NOW | RTLD_GLOBAL)))
        HAL_ERROR("fh_isp", "Failed to load library!\nError: %s\n", dlerror());

    if (!(isp_lib->fnEnableAe = (int(*)(unsigned int enable))
        hal_symbol_load("fh_isp", isp_lib->handle, "API_ISP_AEAlgEn")))
        return EXIT_FAILURE;

    if (!(isp_lib->fnAeSendCmd = (int(*)(unsigned int cmd, void *arg))
        hal_symbol_load("fh_isp", isp_lib->handle, "API_ISP_AESendCmd")))
        return EXIT_FAILURE;

    if (!(isp_lib->fnEnableAwb = (int(*)(unsigned int enable))
        hal_symbol_load("fh_isp", isp_lib->handle, "API_ISP_AWBAlgEn")))
        return EXIT_FAILURE;

    if (!(isp_lib->fnExit = (int(*)(void))
        hal_symbol_load("fh_isp", isp_lib->handle, "API_ISP_Exit")))
        return EXIT_FAILURE;

    if (!(isp_lib->fnGetInputAttr = (int(*)(fh_isp_viattr *attr))
        hal_symbol_load("fh_isp", isp_lib->handle, "API_ISP_GetViAttr")))
        return EXIT_FAILURE;

    if (!(isp_lib->fnGetInputState = (int(*)(fh_isp_vistate *state))
        hal_symbol_load("fh_isp", isp_lib->handle, "API_ISP_GetVIState")))
        return EXIT_FAILURE;

    if (!(isp_lib->fnInit = (int(*)(void))
        hal_symbol_load("fh_isp", isp_lib->handle, "API_ISP_Init")))
        return EXIT_FAILURE;

    if (!(isp_lib->fnLoadParam = (int(*)(void *param))
        hal_symbol_load("fh_isp", isp_lib->handle, "API_ISP_LoadIspParam")))
        return EXIT_FAILURE;

    if (!(isp_lib->fnMemInit = (int(*)(unsigned int width, unsigned int height))
        hal_symbol_load("fh_isp", isp_lib->handle, "API_ISP_MemInit")))
        return EXIT_FAILURE;

    if (!(isp_lib->fnRegisterSensor = (int(*)(unsigned int index, fh_isp_snrops *ops))
        hal_symbol_load("fh_isp", isp_lib->handle, "API_ISP_SensorRegCb")))
        return EXIT_FAILURE;

    if (!(isp_lib->fnRun = (int(*)(void))
        hal_symbol_load("fh_isp", isp_lib->handle, "API_ISP_Run")))
        return EXIT_FAILURE;

    if (!(isp_lib->fnSensorInit = (int(*)(void))
        hal_symbol_load("fh_isp", isp_lib->handle, "API_ISP_SensorInit")))
        return EXIT_FAILURE;

    if (!(isp_lib->fnSetFlipMirror = (int(*)(unsigned int mirror, unsigned int flip))
        hal_symbol_load("fh_isp", isp_lib->handle, "API_ISP_SetMirrorAndflip")))
        return EXIT_FAILURE;

    if (!(isp_lib->fnSetFlipMirrorEx = (int(*)(unsigned int mirror, unsigned int flip, unsigned int bayer))
        hal_symbol_load("fh_isp", isp_lib->handle, "API_ISP_SetMirrorAndflipEx")))
        return EXIT_FAILURE;

    if (!(isp_lib->fnSetFramerateDiv = (int(*)(unsigned int div))
        hal_symbol_load("fh_isp", isp_lib->handle, "API_ISP_SetSensorFrameRate")))
        return EXIT_FAILURE;

    if (!(isp_lib->fnSetSensorFormat = (int(*)(unsigned int format))
        hal_symbol_load("fh_isp", isp_lib->handle, "API_ISP_SetSensorFmt")))
        return EXIT_FAILURE;

    if (!(isp_lib->fnUnregisterSensor = (int(*)(unsigned int index))
        hal_symbol_load("fh_isp", isp_lib->handle, "API_ISP_SensorUnRegCb")))
        return EXIT_FAILURE;

    if (!(isp_lib->fnAdvInit = (int(*)(void))
        hal_symbol_load("fh_isp", isp_lib->handleAdv, "FHAdv_Isp_Init")))
        return EXIT_FAILURE;

    /* SmartIR: image-gain based day/night detection, optional */
    isp_lib->fnSmartIrInit = (int(*)(void))dlsym(isp_lib->handleAdv, "FHAdv_SmartIR_Init");
    isp_lib->fnSmartIrSetAttr = (int(*)(int))dlsym(isp_lib->handleAdv, "FHAdv_SmartIR_SetAttr");
    isp_lib->fnSmartIrStatus = (unsigned char(*)(int))dlsym(isp_lib->handleAdv, "FHAdv_SmartIR_GetDayNightStatus");
    isp_lib->fnSmartIrGetThreshold = (int(*)(unsigned short*))dlsym(isp_lib->handleAdv, "FHAdv_SmartIR_Getthreshold");
    isp_lib->fnSmartIrSetThreshold = (int(*)(unsigned short*))dlsym(isp_lib->handleAdv, "FHAdv_SmartIR_Setthreshold");
    isp_lib->fnGetAeInfo = (int(*)(void*))dlsym(isp_lib->handle, "API_ISP_GetAeInfo");

    if (!(isp_lib->fnGetSaturation = (int(*)(void *sat))
        hal_symbol_load("fh_isp", isp_lib->handle, "API_ISP_GetSaturation")))
        return EXIT_FAILURE;

    if (!(isp_lib->fnSetSaturation = (int(*)(void *sat))
        hal_symbol_load("fh_isp", isp_lib->handle, "API_ISP_SetSaturation")))
        return EXIT_FAILURE;

    if (!(isp_lib->fnAdvSetColorMode = (int(*)(int color))
        hal_symbol_load("fh_isp", isp_lib->handleAdv, "FHAdv_Isp_SetColorMode")))
        return EXIT_FAILURE;

    if (!(isp_lib->fnMipiInit = (void(*)(fh_isp_mipi *config))
        hal_symbol_load("fh_isp", isp_lib->handleMipi, "mipi_init")))
        return EXIT_FAILURE;

    return EXIT_SUCCESS;
}

static void fh_isp_unload(fh_isp_impl *isp_lib) {
    if (isp_lib->handleAdv) dlclose(isp_lib->handleAdv);
    if (isp_lib->handleMipi) dlclose(isp_lib->handleMipi);
    if (isp_lib->handle) dlclose(isp_lib->handle);
    if (isp_lib->handleCore) dlclose(isp_lib->handleCore);
    memset(isp_lib, 0, sizeof(*isp_lib));
}
