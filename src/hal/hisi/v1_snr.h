#pragma once

#include "v1_common.h"

#define V1_SNR_LANE_NUM 8
#define V1_SNR_WDR_VC_NUM 4

typedef enum {
    V1_SNR_INPUT_MIPI,
    V1_SNR_INPUT_SUBLVDS,
    V1_SNR_INPUT_LVDS,
    V1_SNR_INPUT_HISPI,
    V1_SNR_INPUT_CMOS_18V,
    V1_SNR_INPUT_CMOS_33V,
    V1_SNR_INPUT_BT1120,
    V1_SNR_INPUT_BYPASS,
    V1_SNR_INPUT_END
} v1_snr_input;

typedef enum {
    V1_SNR_LWDR_NONE,
    V1_SNR_LWDR_2F,
    V1_SNR_LWDR_3F,
    V1_SNR_LWDR_4F,
    V1_SNR_LWDR_DOL2F,
    V1_SNR_LWDR_DOL3F,
    V1_SNR_LWDR_DOL4F,
    V1_SNR_LWDR_END
} v1_snr_lwdr;

typedef struct {
    v1_common_dim dest;
    v1_snr_lwdr wdr;
    int syncSavOn;
    v1_common_prec prec;
    int dataBeOn;
    int syncBeOn;
    // Value -1 signifies a lane is disabled
    short laneId[V1_SNR_LANE_NUM];
    /* Each lane has two virtual channel, each has four params
       If syncSavOn is false: SOF, EOF, SOL, EOL
       If syncSavOn is true: invalid sav, invalid eav, valid sav, valid eav  */
    unsigned short syncCode[V1_SNR_LANE_NUM * V1_SNR_WDR_VC_NUM * 4];
} v1_snr_lvds;

typedef struct {
    v1_common_prec prec;
    // Value -1 signifies a lane is disabled
    short laneId[V1_SNR_LANE_NUM];
} v1_snr_mipi;

typedef struct {
    void *handle;

    int (*fnInit)(void);
    int (*fnRegisterCallback)(void);
    int (*fnUnRegisterCallback)(void);
} v1_snr_drv_impl;