#pragma once

#include "v4_common.h"

#include <asm/ioctl.h>
#include <fcntl.h>

#define V4_SNR_IOC_MAGIC 'm'
#define V4_SNR_LVDS_LANE_NUM 4
#define V4_SNR_MIPI_LANE_NUM 4
#define V4_SNR_WDR_VC_NUM 2
#define V4A_SNR_WDR_VC_NUM 4

enum {
    V4_SNR_CMD_CONF_DEV = 1,
    V4_SNR_CMD_CONF_CMV = 4,
    V4_SNR_CMD_RST_SENS,
    V4_SNR_CMD_UNRST_SENS,
    V4_SNR_CMD_RST_MIPI,
    V4_SNR_CMD_UNRST_MIPI,
    V4_SNR_CMD_RST_SLVS,
    V4_SNR_CMD_UNRST_SLVS,
    V4_SNR_CMD_CONF_HSMODE,
    V4_SNR_CMD_CLKON_MIPI,
    V4_SNR_CMD_CLKOFF_MIPI,
    V4_SNR_CMD_CLKON_SLVS,
    V4_SNR_CMD_CLKOFF_SLVS,
    V4_SNR_CMD_CLKON_SENS,
    V4_SNR_CMD_CLKOFF_SENS
};

typedef enum {
    V4_SNR_INPUT_MIPI,
    V4_SNR_INPUT_SUBLVDS,
    V4_SNR_INPUT_LVDS,
    V4_SNR_INPUT_HISPI,
    V4_SNR_INPUT_CMOS,
    V4_SNR_INPUT_BT601,
    V4_SNR_INPUT_BT656,
    V4_SNR_INPUT_BT1120,
    V4_SNR_INPUT_BYPASS,
    V4_SNR_INPUT_END
} v4_snr_input;

typedef enum {
    V4_SNR_LFID_NONE,
    V4_SNR_LFID_INSAV,
    V4_SNR_LFID_INDATA,
    V4_SNR_LFID_END
} v4_snr_lfid;

typedef enum {
    V4_SNR_LVSYNCT_NORMAL,
    V4_SNR_LVSYNCT_SHARE,
    V4_SNR_LVSYNCT_HCONNECT,
    V4_SNR_LVSYNCT_END
} v4_snr_lvsynct;

typedef enum {
    V4_SNR_LWDR_NONE,
    V4_SNR_LWDR_2F,
    V4_SNR_LWDR_3F,
    V4_SNR_LWDR_4F,
    V4_SNR_LWDR_DOL2F,
    V4_SNR_LWDR_DOL3F,
    V4_SNR_LWDR_DOL4F,
    V4_SNR_LWDR_END
} v4_snr_lwdr;

typedef enum {
    V4_SNR_MWDR_NONE,
    V4_SNR_MWDR_VC,
    V4_SNR_MWDR_DT,
    V4_SNR_MWDR_DOL,
    V4_SNR_MWDR_END
} v4_snr_mwdr;

typedef struct {
    v4_snr_lfid type;
    unsigned char outputFil;
} v4_snr_fid;

typedef struct {
    v4_snr_lvsynct type;
    unsigned short hBlank1;
    unsigned short hBlank2; 
} v4_snr_lvsync;

typedef struct {
    v4_common_prec prec;
    v4_snr_lwdr wdr;
    int syncSavOn;
    v4_snr_lvsync vsync;
    v4_snr_fid fid;
    int dataBeOn;
    int syncBeOn;
    // Value -1 signifies a lane is disabled
    short laneId[V4_SNR_LVDS_LANE_NUM];
    /* Each lane has two virtual channel, each has four params
       If syncSavOn is false: SOF, EOF, SOL, EOL
       If syncSavOn is true: invalid sav, invalid eav, valid sav, valid eav  */
    unsigned short syncCode[V4_SNR_LVDS_LANE_NUM * V4_SNR_WDR_VC_NUM * 4];
} v4_snr_lvds;

typedef struct {
    v4_common_prec prec;
    v4_snr_lwdr wdr;
    int syncSavOn;
    v4_snr_lvsync vsync;
    v4_snr_fid fid;
    int dataBeOn;
    int syncBeOn;
    // Value -1 signifies a lane is disabled
    short laneId[V4_SNR_LVDS_LANE_NUM];
    /* Each lane has two virtual channel, each has four params
       If syncSavOn is false: SOF, EOF, SOL, EOL
       If syncSavOn is true: invalid sav, invalid eav, valid sav, valid eav  */
    unsigned short syncCode[V4_SNR_LVDS_LANE_NUM * V4A_SNR_WDR_VC_NUM * 4];
} v4a_snr_lvds;

typedef struct {
    v4_common_prec prec;
    v4_snr_mwdr mode;
    // Value -1 signifies a lane is disabled
    short laneId[V4_SNR_MIPI_LANE_NUM];
    union {
        short wdrVcType[V4_SNR_WDR_VC_NUM];
    };
} v4_snr_mipi;

typedef struct {
    v4_common_prec prec;
    v4_snr_mwdr mode;
    // Value -1 signifies a lane is disabled
    short laneId[V4_SNR_MIPI_LANE_NUM];
    union {
        short wdrVcType[V4A_SNR_WDR_VC_NUM];
    };
} v4a_snr_mipi;

typedef struct {
    unsigned int device;
    v4_snr_input input;
    int dataRate2X;
    v4_common_rect rect;
    union {
        v4_snr_mipi mipi;
        v4_snr_lvds lvds;
    };
} v4_snr_dev;

typedef struct {
    unsigned int device;
    v4_snr_input input;
    int dataRate2X;
    v4_common_rect rect;
    union {
        v4a_snr_mipi mipi;
        v4a_snr_lvds lvds;
    };
} v4a_snr_dev;

typedef union {
    signed char i2c;
    struct {
        signed char dev : 4;
        signed char cs  : 4;
    } ssp;
} v4_snr_bus;

typedef struct {
    unsigned int expTime;
    unsigned int aGain;
    unsigned int dGain;
    unsigned int ispDGain;
    unsigned int exposure;
    unsigned int linesPer500ms;
    unsigned int pirisFNO;
    unsigned short wbRGain;
    unsigned short wbGGain;
    unsigned short wbBGain;
    unsigned short sampleRGain;
    unsigned short sampleBGain;
    unsigned short ccm[9];
} v4_snr_init;

typedef struct {
    int (*pfnRegisterCallback)(int pipe, v4_isp_alg *aeLibrary, v4_isp_alg *awbLibrary);
    int (*pfnUnRegisterCallback)(int pipe, v4_isp_alg *aeLibrary, v4_isp_alg *awbLibrary);
    int (*pfnSetBusInfo)(int pipe, v4_snr_bus bus);
    void (*pfnStandby)(int pipe);
    void (*pfnRestart)(int pipe);
    void (*pfnMirrorFlip)(int pipe, v4_isp_dir mode);
    int (*pfnWriteReg)(int pipe, int addr, int data);
    int (*pfnReadReg)(int pipe, int addr);
    int (*pfnSetInit)(int pipe, v4_snr_init *config);
} v4_snr_obj;

typedef struct {
    void *handle;
    v4_snr_obj *obj;
} v4_snr_drv_impl;

static const char v4_snr_endp[] = {"/dev/hi_mipi"};