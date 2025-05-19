#pragma once

#include "m6_common.h"

typedef enum {
    M6_SNR_HWHDR_NONE,
    M6_SNR_HWHDR_SONY_DOL,
    M6_SNR_HWHDR_DCG,
    M6_SNR_HWHDR_EMBED_RAW8,
    M6_SNR_HWHDR_EMBED_RAW10,
    M6_SNR_HWHDR_EMBED_RAW12,
    M6_SNR_HWHDR_EMBED_RAW14,
    M6_SNR_HWHDR_EMBED_RAW16
} m6_snr_hwhdr;

typedef struct {
    unsigned int laneCnt;
    unsigned int rgbFmtOn;
    unsigned int hsyncMode;
    unsigned int sampDelay;
    m6_snr_hwhdr hwHdr;
    unsigned int virtChn;
    unsigned int packType[2];
} m6_snr_mipi;

typedef struct {
    int vsyncInv;
    int hsyncInv;
    int pixclkInv;
    unsigned int vsyncDelay;
    unsigned int hsyncDelay;
    unsigned int pixclkDelay;
} m6_snr_sync;

typedef struct {
    unsigned int multplxNum;
    m6_snr_sync sync;
    m6_common_edge edge;
    char bitswap;
} m6_snr_bt656;

typedef struct {
    m6_snr_sync sync;
} m6_snr_par;

typedef union {
    m6_snr_par parallel;
    m6_snr_mipi mipi;
    m6_snr_bt656 bt656;
} m6_snr_intfattr;

typedef struct {
    unsigned int planeCnt;
    m6_common_intf intf;
    m6_common_hdr hdr;
    m6_snr_intfattr intfAttr;
    char earlyInit;
} m6_snr_pad;

typedef struct {
    unsigned int planeId;
    char sensName[32];
    m6_common_rect capt;
    m6_common_bayer bayer;
    m6_common_prec precision;
    int hdrSrc;
    // Value in microseconds
    unsigned int shutter;
    // Value multiplied by 1024
    unsigned int sensGain;
    unsigned int compGain;
    m6_common_pixfmt pixFmt;
} m6_snr_plane;

typedef struct {
    m6_common_rect crop;
    m6_common_dim output;
    unsigned int maxFps;
    unsigned int minFps;
    char desc[32];
} __attribute__((packed, aligned(4))) m6_snr_res;

typedef struct {
    void *handle;
    
    int (*fnDisable)(unsigned int sensor);
    int (*fnEnable)(unsigned int sensor);

    int (*fnSetFramerate)(unsigned int sensor, unsigned int framerate);
    int (*fnSetOrientation)(unsigned int sensor, unsigned char mirror, unsigned char flip);

    int (*fnGetPadInfo)(unsigned int sensor, m6_snr_pad *info);
    int (*fnGetPlaneInfo)(unsigned int sensor, unsigned int index, m6_snr_plane *info);
    int (*fnSetPlaneMode)(unsigned int sensor, unsigned char active);

    int (*fnCurrentResolution)(unsigned int sensor, unsigned char *index, m6_snr_res *resolution);
    int (*fnGetResolution)(unsigned int sensor, unsigned char index, m6_snr_res *resolution);
    int (*fnGetResolutionCount)(unsigned int sensor, unsigned int *count);
    int (*fnSetResolution)(unsigned int sensor, unsigned char index);

    int (*fnCustomFunction)(unsigned int sensor, unsigned int command, unsigned int size, void *data, int drvOrUsr);
} m6_snr_impl;

static int m6_snr_load(m6_snr_impl *snr_lib) {
    if (!(snr_lib->handle = dlopen("libmi_sensor.so", RTLD_LAZY | RTLD_GLOBAL)))
        HAL_ERROR("m6_snr", "Failed to load library!\nError: %s\n", dlerror());

    if (!(snr_lib->fnDisable = (int(*)(unsigned int sensor))
        hal_symbol_load("m6_snr", snr_lib->handle, "MI_SNR_Disable")))
        return EXIT_FAILURE;

    if (!(snr_lib->fnEnable = (int(*)(unsigned int sensor))
        hal_symbol_load("m6_snr", snr_lib->handle, "MI_SNR_Enable")))
        return EXIT_FAILURE;

    if (!(snr_lib->fnSetFramerate = (int(*)(unsigned int sensor, unsigned int framerate))
        hal_symbol_load("m6_snr", snr_lib->handle, "MI_SNR_SetFps")))
        return EXIT_FAILURE;

    if (!(snr_lib->fnSetOrientation = (int(*)(unsigned int sensor, unsigned char mirror, unsigned char flip))
        hal_symbol_load("m6_snr", snr_lib->handle, "MI_SNR_SetOrien")))
        return EXIT_FAILURE;

    if (!(snr_lib->fnGetPadInfo = (int(*)(unsigned int sensor, m6_snr_pad *info))
        hal_symbol_load("m6_snr", snr_lib->handle, "MI_SNR_GetPadInfo")))
        return EXIT_FAILURE;

    if (!(snr_lib->fnGetPlaneInfo = (int(*)(unsigned int sensor, unsigned int index, m6_snr_plane *info))
        hal_symbol_load("m6_snr", snr_lib->handle, "MI_SNR_GetPlaneInfo")))
        return EXIT_FAILURE;

    if (!(snr_lib->fnSetPlaneMode = (int(*)(unsigned int sensor, unsigned char active))
        hal_symbol_load("m6_snr", snr_lib->handle, "MI_SNR_SetPlaneMode")))
        return EXIT_FAILURE;

    if (!(snr_lib->fnCurrentResolution = (int(*)(unsigned int sensor, unsigned char *index, m6_snr_res *resolution))
        hal_symbol_load("m6_snr", snr_lib->handle, "MI_SNR_GetCurRes")))
        return EXIT_FAILURE;

    if (!(snr_lib->fnGetResolution = (int(*)(unsigned int sensor, unsigned char index, m6_snr_res *resolution))
        hal_symbol_load("m6_snr", snr_lib->handle, "MI_SNR_GetRes")))
        return EXIT_FAILURE;

    if (!(snr_lib->fnGetResolutionCount = (int(*)(unsigned int sensor, unsigned int *count))
        hal_symbol_load("m6_snr", snr_lib->handle, "MI_SNR_QueryResCount")))
        return EXIT_FAILURE;

    if (!(snr_lib->fnSetResolution = (int(*)(unsigned int sensor, unsigned char index))
        hal_symbol_load("m6_snr", snr_lib->handle, "MI_SNR_SetRes")))
        return EXIT_FAILURE;

   if (!(snr_lib->fnCustomFunction = (int(*)(unsigned int sensor, unsigned int command, unsigned int size, void *data, int drvOrUsr))
        hal_symbol_load("m6_snr", snr_lib->handle, "MI_SNR_CustFunction")))
        return EXIT_FAILURE;

    return EXIT_SUCCESS;
}

static void m6_snr_unload(m6_snr_impl *snr_lib) {
    if (snr_lib->handle) dlclose(snr_lib->handle);
    snr_lib->handle = NULL;
    memset(snr_lib, 0, sizeof(*snr_lib));
}