#pragma once

#include "v3_common.h"

#define V3_AUD_CHN_NUM 2

typedef enum {
    V3_AUD_BIT_8,
    V3_AUD_BIT_16,
    V3_AUD_BIT_24
} v3_aud_bit;

typedef enum {
    V3_AUD_INTF_I2S_MASTER,
    V3_AUD_INTF_I2S_SLAVE,
    V3_AUD_INTF_PCM_SLAVE_STD,
    V3_AUD_INTF_PCM_SLAVE_NSTD,
    V3_AUD_INTF_PCM_MASTER_STD,
    V3_AUD_INTF_PCM_MASTER_NSTD,
    V3_AUD_INTF_END
} v3_aud_intf;

typedef struct {
    // Accept industry standards from
    // 8000 to 96000Hz, plus 64000Hz
    int rate;
    v3_aud_bit bit;
    v3_aud_intf intf;
    int stereoOn;
    // 8-to-16 bit, expand mode
    unsigned int expandOn;
    unsigned int frmNum;
    unsigned int packNumPerFrm;
    unsigned int chnNum;
    unsigned int syncRxClkOn;
} v3_aud_cnf;

typedef struct {
    v3_aud_bit bit;
    int stereoOn;
    void *addr[2];
    unsigned int phy[2];
    unsigned long long timestamp;
    unsigned int sequence;
    unsigned int length;
    unsigned int poolId[2];
} v3_aud_frm;

typedef struct {
    v3_aud_frm frame;
    char isValid;
    char isSysBound;
} v3_aud_efrm;

typedef struct {
    void *handle, *handleGoke;
    
    int (*fnDisableDevice)(int device);
    int (*fnEnableDevice)(int device);
    int (*fnSetDeviceConfig)(int device, v3_aud_cnf *config);

    int (*fnDisableChannel)(int device, int channel);
    int (*fnEnableChannel)(int device, int channel);

    int (*fnFreeFrame)(int device, int channel, v3_aud_frm *frame, v3_aud_efrm *encFrame);
    int (*fnGetFrame)(int device, int channel, v3_aud_frm *frame, v3_aud_efrm *encFrame, int millis);
} v3_aud_impl;

static int v3_aud_load(v3_aud_impl *aud_lib) {
    if (!(aud_lib->handle = dlopen("libmpi.so", RTLD_LAZY | RTLD_GLOBAL)))
        HAL_ERROR("v3_aud", "Failed to load library!\nError: %s\n", dlerror());

    if (!(aud_lib->fnDisableDevice = (int(*)(int device))
        hal_symbol_load("v3_aud", aud_lib->handle, "HI_MPI_AI_Disable")))
        return EXIT_FAILURE;

    if (!(aud_lib->fnEnableDevice = (int(*)(int device))
        hal_symbol_load("v3_aud", aud_lib->handle, "HI_MPI_AI_Enable")))
        return EXIT_FAILURE;

    if (!(aud_lib->fnSetDeviceConfig = (int(*)(int device, v3_aud_cnf *config))
        hal_symbol_load("v3_aud", aud_lib->handle, "HI_MPI_AI_SetPubAttr")))
        return EXIT_FAILURE;

    if (!(aud_lib->fnDisableChannel = (int(*)(int device, int channel))
        hal_symbol_load("v3_aud", aud_lib->handle, "HI_MPI_AI_DisableChn")))
        return EXIT_FAILURE;

    if (!(aud_lib->fnEnableChannel = (int(*)(int device, int channel))
        hal_symbol_load("v3_aud", aud_lib->handle, "HI_MPI_AI_EnableChn")))
        return EXIT_FAILURE;

    if (!(aud_lib->fnFreeFrame = (int(*)(int device, int channel, v3_aud_frm *frame, v3_aud_efrm *encFrame))
        hal_symbol_load("v3_aud", aud_lib->handle, "HI_MPI_AI_ReleaseFrame")))
        return EXIT_FAILURE;

    if (!(aud_lib->fnGetFrame = (int(*)(int device, int channel, v3_aud_frm *frame, v3_aud_efrm *encFrame, int millis))
        hal_symbol_load("v3_aud", aud_lib->handle, "HI_MPI_AI_GetFrame")))
        return EXIT_FAILURE;

    return EXIT_SUCCESS;
}

static void v3_aud_unload(v3_aud_impl *aud_lib) {
    if (aud_lib->handle) dlclose(aud_lib->handle);
    aud_lib->handle = NULL;
    if (aud_lib->handleGoke) dlclose(aud_lib->handleGoke);
    aud_lib->handleGoke = NULL;
    memset(aud_lib, 0, sizeof(*aud_lib));
}