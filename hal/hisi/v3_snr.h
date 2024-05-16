#include "v3_common.h"

#include <asm/ioctl.h>
#include <fcntl.h>

#define V3_SNR_ENDPOINT "/dev/hi_mipi"
#define V3_SNR_IOC_MAGIC 'm'
#define V3_SNR_LVDS_LANE_NUM 4
#define V3_SNR_MIPI_LANE_NUM 4
#define V3_SNR_WDR_VC_NUM 4

enum {
    V3_SNR_CMD_CONF_DEV = 1,
    V3_SNR_CMD_CONF_CLKEDGE,
    V3_SNR_CMD_CONF_OUTMSB,
    V3_SNR_CMD_CONF_CMV,
    V3_SNR_CMD_RST_SENS,
    V3_SNR_CMD_UNRST_SENS,
    V3_SNR_CMD_RST_INTF,
    V3_SNR_CMD_UNRST_INTF,
    V3_SNR_CMD_CONF_CROP
};

typedef enum {
    V3_SNR_INPUT_MIPI,
    V3_SNR_INPUT_SUBLVDS,
    V3_SNR_INPUT_LVDS,
    V3_SNR_INPUT_HISPI,
    V3_SNR_INPUT_CMOS,
    V3_SNR_INPUT_BT1120,
    V3_SNR_INPUT_BYPASS,
    V3_SNR_INPUT_END
} v3_snr_input;

typedef enum {
    V3_SNR_LFID_NONE,
    V3_SNR_LFID_INSAV,
    V3_SNR_LFID_INDATA,
    V3_SNR_LFID_END
} v3_snr_lfid;

typedef enum {
    V3_SNR_LVSYNCT_NORMAL,
    V3_SNR_LVSYNCT_SHARE,
    V3_SNR_LVSYNCT_HCONNECT,
    V3_SNR_LVSYNCT_END
} v3_snr_lvsynct;

typedef enum {
    V3_SNR_LWDR_NONE,
    V3_SNR_LWDR_2F,
    V3_SNR_LWDR_3F,
    V3_SNR_LWDR_4F,
    V3_SNR_LWDR_DOL2F,
    V3_SNR_LWDR_DOL3F,
    V3_SNR_LWDR_DOL4F,
    V3_SNR_LWDR_END
} v3_snr_lwdr;

typedef enum {
    V3_SNR_MWDR_NONE,
    V3_SNR_MWDR_VC,
    V3_SNR_MWDR_DT,
    V3_SNR_MWDR_DOL,
    V3_SNR_MWDR_END
} v3_snr_mwdr;

typedef struct {
    v3_snr_lvsynct type;
    unsigned short hBlank1;
    unsigned short hBlank2; 
} v3_snr_lvsync;

typedef struct
{
    v3_common_dim capt;
    v3_common_prec prec;
    v3_snr_lwdr wdr;

    int syncSavOn;
    v3_snr_lvsynct vsync;
    v3_snr_lfid fid;

    int dataBeOn;
    int syncBeOn;
    // Value -1 signifies a lane is disabled
    short laneId[V3_SNR_LVDS_LANE_NUM];

    /* Each virtual channel on each lane has four parameters
       If syncSavOn is true: valid SAV, EAV then invalid SAV, EAV
       If syncSavOn is false: SOL, EOL, SOF, EOF */
    unsigned short sync_code
        [V3_SNR_LVDS_LANE_NUM][V3_SNR_WDR_VC_NUM][4];
} v3_snr_lvds;

typedef struct {
    v3_common_prec prec;
    v3_snr_mwdr wdr;
    // Value -1 signifies a lane is disabled
    short laneId[V3_SNR_MIPI_LANE_NUM];
    // Serves when SNR_MWDR_DT is set
    short type[V3_SNR_WDR_VC_NUM];
} v3_snr_mipi;

typedef struct {
    unsigned int device;
    v3_snr_input input;
    union {
        v3_snr_mipi mipi;
        v3_snr_lvds lvds;
    };
} v3_snr_dev;