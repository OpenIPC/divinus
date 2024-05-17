#pragma once

#include "i6_common.h"

typedef enum {
    I6_SNR_HWHDR_NONE,
    I6_SNR_HWHDR_SONY_DOL,
    I6_SNR_HWHDR_DCG,
    I6_SNR_HWHDR_EMBED_RAW8,
    I6_SNR_HWHDR_EMBED_RAW10,
    I6_SNR_HWHDR_EMBED_RAW12,
    I6_SNR_HWHDR_EMBED_RAW16
} i6_snr_hwhdr;

typedef struct {
    unsigned int laneCnt;
    unsigned int rgbFmtOn;
    i6_common_input input;
    unsigned int hsyncMode;
    unsigned int sampDelay;
    i6_snr_hwhdr hwHdr;
    unsigned int virtChn;
    unsigned int packType[2];
} i6_snr_mipi;

typedef struct {
    unsigned int multplxNum;
    i6_common_sync sync;
    i6_common_edge edge;
    char bitswap;
} i6_snr_bt656;

typedef struct {
    i6_common_sync sync;
} i6_snr_par;

typedef union {
    i6_snr_par parallel;
    i6_snr_mipi mipi;
    i6_snr_bt656 bt656;
} i6_snr_intfattr;

typedef struct {
    unsigned int planeCnt;
    i6_common_intf intf;
    i6_common_hdr hdr;
    i6_snr_intfattr intfAttr;
    char earlyInit;
} i6_snr_pad;

typedef struct {
    unsigned int planeId;
    char sensName[32];
    i6_common_rect capt;
    i6_common_bayer bayer;
    i6_common_prec precision;
    int hdrSrc;
    // Value in microseconds
    unsigned int shutter;
    // Value multiplied by 1024
    unsigned int sensGain;
    unsigned int compGain;
    i6_common_pixfmt pixFmt;
} i6_snr_plane;

typedef struct {
    i6_common_rect crop;
    i6_common_dim output;
    unsigned int maxFps;
    unsigned int minFps;
    char desc[32];
} __attribute__((packed, aligned(4))) i6_snr_res;

typedef struct {
    void *handle;
    
    int (*fnDisable)(int sensor);
    int (*fnEnable)(int sensor);

    int (*fnSetFramerate)(int sensor, unsigned int framerate);
    int (*fnSetHDR)(int sensor, char active);

    int (*fnGetPadInfo)(int sensor, i6_snr_pad *info);
    int (*fnGetPlaneInfo)(int sensor, unsigned int index, i6_snr_plane *info);

    int (*fnCurrentResolution)(int sensor, unsigned char *index, i6_snr_res *resolution);
    int (*fnGetResolution)(int sensor, unsigned char index, i6_snr_res *resolution);
    int (*fnGetResolutionCount)(int sensor, unsigned int *count);
    int (*fnSetResolution)(int sensor, unsigned char index);
} i6_snr_impl;

int i6_snr_load(i6_snr_impl *snr_lib) {
    if (!(snr_lib->handle = dlopen("libmi_sensor.so", RTLD_NOW))) {
        fprintf(stderr, "[i6_snr] Failed to load library!\n");
        return EXIT_FAILURE;
    }

    if (!(snr_lib->fnDisable = (int(*)(int sensor))
        dlsym(snr_lib->handle, "MI_SNR_Disable"))) {
        fprintf(stderr, "[i6_snr] Failed to acquire symbol MI_SNR_Disable!\n");
        return EXIT_FAILURE;
    }

    if (!(snr_lib->fnEnable = (int(*)(int sensor))
        dlsym(snr_lib->handle, "MI_SNR_Enable"))) {
        fprintf(stderr, "[i6_snr] Failed to acquire symbol MI_SNR_Enable!\n");
        return EXIT_FAILURE;
    }

    if (!(snr_lib->fnSetFramerate = (int(*)(int sensor, unsigned int framerate))
        dlsym(snr_lib->handle, "MI_SNR_SetFps"))) {
        fprintf(stderr, "[i6_snr] Failed to acquire symbol MI_SNR_SetFps!\n");
        return EXIT_FAILURE;
    }

    if (!(snr_lib->fnSetHDR = (int(*)(int sensor, char active))
        dlsym(snr_lib->handle, "MI_SNR_SetPlaneMode"))) {
        fprintf(stderr, "[i6_snr] Failed to acquire symbol MI_SNR_SetPlaneMode!\n");
        return EXIT_FAILURE;
    }

    if (!(snr_lib->fnGetPadInfo = (int(*)(int sensor, i6_snr_pad *info))
        dlsym(snr_lib->handle, "MI_SNR_GetPadInfo"))) {
        fprintf(stderr, "[i6_snr] Failed to acquire symbol MI_SNR_GetPadInfo!\n");
        return EXIT_FAILURE;
    }

    if (!(snr_lib->fnGetPlaneInfo = (int(*)(int sensor, unsigned int index, i6_snr_plane *info))
        dlsym(snr_lib->handle, "MI_SNR_GetPadInfo"))) {
        fprintf(stderr, "[i6_snr] Failed to acquire symbol MI_SNR_GetPadInfo!\n");
        return EXIT_FAILURE;
    }

    if (!(snr_lib->fnCurrentResolution = (int(*)(int sensor, unsigned char *index, i6_snr_res *resolution))
        dlsym(snr_lib->handle, "MI_SNR_GetCurRes"))) {
        fprintf(stderr, "[i6_snr] Failed to acquire symbol MI_SNR_GetCurRes!\n");
        return EXIT_FAILURE;
    }

    if (!(snr_lib->fnGetResolution = (int(*)(int sensor, unsigned char index, i6_snr_res *resolution))
        dlsym(snr_lib->handle, "MI_SNR_GetRes"))) {
        fprintf(stderr, "[i6_snr] Failed to acquire symbol MI_SNR_GetRes!\n");
        return EXIT_FAILURE;
    }

    if (!(snr_lib->fnGetResolutionCount = (int(*)(int sensor, unsigned int *count))
        dlsym(snr_lib->handle, "MI_SNR_QueryResCount"))) {
        fprintf(stderr, "[i6_snr] Failed to acquire symbol MI_SNR_QueryResCount!\n");
        return EXIT_FAILURE;
    }

    if (!(snr_lib->fnSetResolution = (int(*)(int sensor, unsigned char index))
        dlsym(snr_lib->handle, "MI_SNR_SetRes"))) {
        fprintf(stderr, "[i6_snr] Failed to acquire symbol MI_SNR_SetRes!\n");
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}

void i6_snr_unload(i6_snr_impl *snr_lib) {
    if (snr_lib->handle)
        dlclose(snr_lib->handle = NULL);
    memset(snr_lib, 0, sizeof(*snr_lib));
}