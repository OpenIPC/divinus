#pragma once

#include "cvi_common.h"
#include "cvi_isp.h"

#include <asm/ioctl.h>
#include <fcntl.h>

#define CVI_SNR_IOC_MAGIC 'm'
#define CVI_SNR_LVDS_LANE_NUM 4
#define CVI_SNR_MIPI_LANE_NUM 4
#define CVI_SNR_WDR_VC_NUM 2

enum {
    CVI_SNR_CMD_CONF_DEV = 1,
    CVI_SNR_CMD_CONF_CMV = 4,
    CVI_SNR_CMD_RST_SENS,
    CVI_SNR_CMD_UNRST_SENS,
    CVI_SNR_CMD_RST_MIPI,
    CVI_SNR_CMD_UNRST_MIPI,
    CVI_SNR_CMD_RST_SLVS,
    CVI_SNR_CMD_UNRST_SLVS,
    CVI_SNR_CMD_CONF_HSMODE,
    CVI_SNR_CMD_CLKON_MIPI,
    CVI_SNR_CMD_CLKOFF_MIPI,
    CVI_SNR_CMD_CLKON_SLVS,
    CVI_SNR_CMD_CLKOFF_SLVS,
    CVI_SNR_CMD_CLKON_SENS,
    CVI_SNR_CMD_CLKOFF_SENS
};

typedef enum {
    CVI_SNR_BRMUX_NONE,
    CVI_SNR_BRMUX_2,
    CVI_SNR_BRMUX_3,
    CVI_SNR_BRMUX_4
} cvi_snr_brmux;

typedef enum {
    CVI_SNR_GAIN_SHARE,
    CVI_SNR_GAIN_WDR_2F,
    CVI_SNR_GAIN_WDR_3F,
    CVI_SNR_GAIN_ONLY_LEF
} cvi_snr_gain;

typedef enum {
    CVI_SNR_INPUT_BT656,
    CVI_SNR_INPUT_BT601,
    CVI_SNR_INPUT_DIGITAL_CAMERA,
    CVI_SNR_INPUT_INTERLEAVED,
    CVI_SNR_INPUT_MIPI,
    CVI_SNR_INPUT_LVDS,
    CVI_SNR_INPUT_HISPI,
    CVI_SNR_INPUT_SUBLVDS,
    CVI_SNR_INPUT_END
} cvi_snr_input;

typedef enum {
    CVI_SNR_LFID_NONE,
    CVI_SNR_LFID_INSAV,
    CVI_SNR_LFID_INDATA,
    CVI_SNR_LFID_END
} cvi_snr_lfid;

typedef enum {
    CVI_SNR_LVSYNCT_NORMAL,
    CVI_SNR_LVSYNCT_SHARE,
    CVI_SNR_LVSYNCT_HCONNECT,
    CVI_SNR_LVSYNCT_END
} cvi_snr_lvsynct;

typedef enum {
    CVI_SNR_LWDR_NONE,
    CVI_SNR_LWDR_2F,
    CVI_SNR_LWDR_3F,
    CVI_SNR_LWDR_4F,
    CVI_SNR_LWDR_DOL2F,
    CVI_SNR_LWDR_DOL3F,
    CVI_SNR_LWDR_DOL4F,
    CVI_SNR_LWDR_END
} cvi_snr_lwdr;

typedef enum {
    CVI_SNR_MACCK_200M,
    CVI_SNR_MACCK_300M,
    CVI_SNR_MACCK_400M,
    CVI_SNR_MACCK_500M,
    CVI_SNR_MACCK_600M,
    CVI_SNR_MACCK_END
} cvi_snr_macck;

typedef enum {
    CVI_SNR_MWDR_NONE,
    CVI_SNR_MWDR_VC,
    CVI_SNR_MWDR_DT,
    CVI_SNR_MWDR_DOL,
    CVI_SNR_MWDR_MANUAL, /* Applicable in SOI */
    CVI_SNR_MWDR_END
} cvi_snr_mwdr;

typedef enum {
    CVI_SNR_PLLFR_NONE,
    CVI_SNR_PLLFR_37P125M,
    CVI_SNR_PLLFR_25M,
    CVI_SNR_PLLFR_27M,
    CVI_SNR_PLLFR_24M,
    CVI_SNR_PLLFR_26M,
    CVI_SNR_PLLFR_END
} cvi_snr_pllfr;

typedef struct {
    cvi_snr_lfid type;
    unsigned char outputFil;
} cvi_snr_fid;

typedef struct {
    cvi_snr_lvsynct type;
    unsigned short hBlank1;
    unsigned short hBlank2; 
} cvi_snr_lvsync;

typedef struct {
    cvi_snr_lwdr wdr;
    int syncSavOn;
    cvi_common_prec prec;
    int dataBeOn;
    int syncBeOn;
    // Value -1 signifies a lane is disabled
    short laneId[CVI_SNR_LVDS_LANE_NUM];
    /* Each lane has two virtual channel, each has four params
       If syncSavOn is false: SOF, EOF, SOL, EOL
       If syncSavOn is true: invalid sav, invalid eav, valid sav, valid eav  */
    unsigned short syncCode[CVI_SNR_LVDS_LANE_NUM * CVI_SNR_WDR_VC_NUM * 4];
    cvi_snr_lvsync vsync;
    cvi_snr_fid fid;
    char pnSwap[CVI_SNR_LVDS_LANE_NUM + 1];
} cvi_snr_lvds;

typedef struct {
    cvi_common_prec prec;
        // Value -1 signifies a lane is disabled
    short laneId[CVI_SNR_MIPI_LANE_NUM + 1];
    cvi_snr_mwdr mode;
    short dataType[CVI_SNR_WDR_VC_NUM];
    char pnSwap[CVI_SNR_MIPI_LANE_NUM + 1];
    unsigned char dphyEnable;
    unsigned char dphyHsSettle;
    unsigned int demuxOn;
    unsigned char vcMap[4];
} cvi_snr_mipi;

typedef struct {
    unsigned int cam;
    cvi_snr_pllfr freq;
} cvi_snr_pllck;

typedef struct {
    cvi_snr_input input;
    cvi_snr_macck macck;
    cvi_snr_pllck pllck;
    union {
        cvi_snr_mipi mipi;
        cvi_snr_lvds lvds;
    };
    unsigned int device;
    cvi_common_dim dest;
} cvi_snr_dev;

typedef struct {
    unsigned int mipiDev;
    short laneId[CVI_SNR_MIPI_LANE_NUM + 1];
    signed char pnSwap[CVI_SNR_MIPI_LANE_NUM + 1];
    unsigned char mclk;
    char mclkOn;
} cvi_snr_rx;

typedef union {
    signed char i2c;
    struct {
        signed char dev : 4;
        signed char cs  : 4;
    } ssp;
} cvi_snr_bus;

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
    unsigned short useHwSync;
    cvi_snr_gain gain;
    int l2sDstFixOn;
    cvi_snr_brmux brmux;
} cvi_snr_init;

typedef struct {
    int  (*pfnRegisterCallback)(int pipe, cvi_isp_alg *aeLibrary, cvi_isp_alg *awbLibrary);
    int  (*pfnUnRegisterCallback)(int pipe, cvi_isp_alg *aeLibrary, cvi_isp_alg *awbLibrary);
    int  (*pfnSetBusInfo)(int pipe, cvi_snr_bus bus);
    void (*pfnStandby)(int pipe);
    void (*pfnRestart)(int pipe);
    void (*pfnMirrorFlip)(int pipe, cvi_isp_dir mode);
    int  (*pfnWriteReg)(int pipe, int addr, int data);
    int  (*pfnReadReg)(int pipe, int addr);
    int  (*pfnSetInit)(int pipe, cvi_snr_init *config);
    int  (*pfnPatchRxAttr)(cvi_snr_rx *config);
    void (*pfnPatchI2cAddr)(int addr);
    int  (*pfnGetRxAttr)(int pipe, cvi_snr_dev *device);
    int  (*pfnExpSensorCb)(void *config);
    int  (*pfnExpAeCb)(void *config);
    int  (*pfnSnsProbe)(int pipe);
} cvi_snr_obj;

typedef struct {
    void *handle;
    cvi_snr_obj *obj;
} cvi_snr_drv_impl;

static const char cvi_snr_endp[] = {"/dev/cvi-mipi-rx"};