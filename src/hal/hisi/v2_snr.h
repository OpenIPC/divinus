#pragma once

#include "v2_common.h"

#include <asm/ioctl.h>
#include <fcntl.h>

#define V2_SNR_IOC_MAGIC 'm'
#define V2_SNR_LANE_NUM 8
#define V2_SNR_WDR_VC_NUM 4

enum {
    V2_SNR_CMD_CONF_DEV = 1,
    V2_SNR_CMD_CONF_EDGE,
    V2_SNR_CMD_CONF_MSB
};

typedef enum {
    V2_SNR_INPUT_MIPI,
    V2_SNR_INPUT_SUBLVDS,
    V2_SNR_INPUT_LVDS,
    V2_SNR_INPUT_HISPI,
    V2_SNR_INPUT_CMOS_18V,
    V2_SNR_INPUT_CMOS_33V,
    V2_SNR_INPUT_BT1120,
    V2_SNR_INPUT_BYPASS,
    V2_SNR_INPUT_END
} v2_snr_input;

typedef enum {
    V2_SNR_LWDR_NONE,
    V2_SNR_LWDR_2F,
    V2_SNR_LWDR_3F,
    V2_SNR_LWDR_4F,
    V2_SNR_LWDR_DOL2F,
    V2_SNR_LWDR_DOL3F,
    V2_SNR_LWDR_DOL4F,
    V2_SNR_LWDR_END
} v2_snr_lwdr;

typedef struct {
    v2_common_dim dest;
    v2_snr_lwdr wdr;
    int syncSavOn;
    v2_common_prec prec;
    int dataBeOn;
    int syncBeOn;
    // Value -1 signifies a lane is disabled
    short laneId[V2_SNR_LANE_NUM];
    /* Each lane has two virtual channel, each has four params
       If syncSavOn is false: SOF, EOF, SOL, EOL
       If syncSavOn is true: invalid sav, invalid eav, valid sav, valid eav  */
    unsigned short syncCode[V2_SNR_LANE_NUM * V2_SNR_WDR_VC_NUM * 4];
} v2_snr_lvds;

typedef struct {
    v2_common_prec prec;
    // Value -1 signifies a lane is disabled
    short laneId[V2_SNR_LANE_NUM];
} v2_snr_mipi;

typedef struct {
    v2_snr_input input;
    union {
        v2_snr_mipi mipi;
        v2_snr_lvds lvds;
    };
} v2_snr_dev;

typedef struct {
    void *handle;

    int (*fnRegisterCallback)(void);
    int (*fnUnRegisterCallback)(void);
} v2_snr_drv_impl;

static const char v2_snr_endp[] = {"/dev/hi_mipi"};