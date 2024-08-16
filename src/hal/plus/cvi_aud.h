#pragma once

#include "cvi_common.h"

#define CVI_AUD_CHN_NUM 3

typedef enum {
    CVI_AUD_BIT_8,
    CVI_AUD_BIT_16,
    CVI_AUD_BIT_24,
    CVI_AUD_BIT_32
} cvi_aud_bit;

typedef enum {
    CVI_AUD_I2ST_INNERCODEC,
    CVI_AUD_I2ST_INNERHDMI,
    CVI_AUD_I2ST_EXTERN
} cvi_aud_i2st;

typedef enum {
    CVI_AUD_INTF_I2S_MASTER,
    CVI_AUD_INTF_I2S_SLAVE,
    CVI_AUD_INTF_PCM_SLAVE_STD,
    CVI_AUD_INTF_PCM_SLAVE_NSTD,
    CVI_AUD_INTF_PCM_MASTER_STD,
    CVI_AUD_INTF_PCM_MASTER_NSTD,
    CVI_AUD_INTF_END
} cvi_aud_intf;

typedef struct {
    // Accept industry standards from
    // 8000 to 64000Hz
    int rate;
    cvi_aud_bit bit;
    cvi_aud_intf intf;
    int stereoOn;
    // 8-to-16 bit, expand mode
    unsigned int expandOn;
    unsigned int frmNum;
    unsigned int packNumPerFrm;
    unsigned int chnNum;
    unsigned int syncRxClkOn;
    cvi_aud_i2st i2sType;
} cvi_aud_cnf;

typedef struct {
    cvi_aud_bit bit;
    int stereoOn;
    char *addr[2];
    unsigned long long phy[2];
    unsigned long long timestamp;
    unsigned int sequence;
    unsigned int length;
    unsigned int poolId[2];
} cvi_aud_frm;

typedef struct {
    cvi_aud_frm frame;
    char isValid;
    char isSysBound;
} cvi_aud_efrm;

typedef struct {
    void *handle;
    
    int (*fnDisableDevice)(int device);
    int (*fnEnableDevice)(int device);
    int (*fnSetDeviceConfig)(int device, cvi_aud_cnf *config);

    int (*fnDisableChannel)(int device, int channel);
    int (*fnEnableChannel)(int device, int channel);

    int (*fnFreeFrame)(int device, int channel, cvi_aud_frm *frame, cvi_aud_efrm *encFrame);
    int (*fnGetFrame)(int device, int channel, cvi_aud_frm *frame, cvi_aud_efrm *encFrame, int millis);
} cvi_aud_impl;

static int cvi_aud_load(cvi_aud_impl *aud_lib) {
    if (!(aud_lib->handle = dlopen("libcvi_audio.so", RTLD_LAZY | RTLD_GLOBAL)))
        HAL_ERROR("cvi_aud", "Failed to load library!\nError: %s\n", dlerror());

    if (!(aud_lib->fnDisableDevice = (int(*)(int device))
        hal_symbol_load("cvi_aud", aud_lib->handle, "CVI_AI_Disable")))
        return EXIT_FAILURE;

    if (!(aud_lib->fnEnableDevice = (int(*)(int device))
        hal_symbol_load("cvi_aud", aud_lib->handle, "CVI_AI_Enable")))
        return EXIT_FAILURE;

    if (!(aud_lib->fnSetDeviceConfig = (int(*)(int device, cvi_aud_cnf *config))
        hal_symbol_load("cvi_aud", aud_lib->handle, "CVI_AI_SetPubAttr")))
        return EXIT_FAILURE;

    if (!(aud_lib->fnDisableChannel = (int(*)(int device, int channel))
        hal_symbol_load("cvi_aud", aud_lib->handle, "CVI_AI_DisableChn")))
        return EXIT_FAILURE;

    if (!(aud_lib->fnEnableChannel = (int(*)(int device, int channel))
        hal_symbol_load("cvi_aud", aud_lib->handle, "CVI_AI_EnableChn")))
        return EXIT_FAILURE;

    if (!(aud_lib->fnFreeFrame = (int(*)(int device, int channel, cvi_aud_frm *frame, cvi_aud_efrm *encFrame))
        hal_symbol_load("cvi_aud", aud_lib->handle, "CVI_AI_ReleaseFrame")))
        return EXIT_FAILURE;

    if (!(aud_lib->fnGetFrame = (int(*)(int device, int channel, cvi_aud_frm *frame, cvi_aud_efrm *encFrame, int millis))
        hal_symbol_load("cvi_aud", aud_lib->handle, "CVI_AI_GetFrame")))
        return EXIT_FAILURE;

    return EXIT_SUCCESS;
}

static void cvi_aud_unload(cvi_aud_impl *aud_lib) {
    if (aud_lib->handle) dlclose(aud_lib->handle);
    aud_lib->handle = NULL;
    memset(aud_lib, 0, sizeof(*aud_lib));
}