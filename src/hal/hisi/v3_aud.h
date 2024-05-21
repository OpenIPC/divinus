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
    int expandOn;
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
    void *handle;
    
    int (*fnDisableDevice)(int device);
    int (*fnEnableDevice)(int device);
    int (*fnSetDeviceConfig)(int device, v3_aud_cnf *config);

    int (*fnDisableChannel)(int device, int channel);
    int (*fnEnableChannel)(int device, int channel);

    int (*fnSetVolume)(int device, int channel, int dbLevel);

    int (*fnFreeFrame)(int device, int channel, v3_aud_frm *frame, v3_aud_efrm *encFrame);
    int (*fnGetFrame)(int device, int channel, v3_aud_frm *frame, v3_aud_efrm *encFrame, int millis);
} v3_aud_impl;

static int v3_aud_load(v3_aud_impl *aud_lib) {
    if (!(aud_lib->handle = dlopen("libmpi.so", RTLD_LAZY | RTLD_GLOBAL))) {
        fprintf(stderr, "[v3_aud] Failed to load library!\nError: %s\n", dlerror());
        return EXIT_FAILURE;
    }

    if (!(aud_lib->fnDisableDevice = (int(*)(int device))
        dlsym(aud_lib->handle, "HI_MPI_AI_Disable"))) {
        fprintf(stderr, "[v3_aud] Failed to acquire symbol HI_MPI_AI_Disable!\n");
        return EXIT_FAILURE;
    }

    if (!(aud_lib->fnEnableDevice = (int(*)(int device))
        dlsym(aud_lib->handle, "HI_MPI_AI_Enable"))) {
        fprintf(stderr, "[v3_aud] Failed to acquire symbol HI_MPI_AI_Enable!\n");
        return EXIT_FAILURE;
    }

    if (!(aud_lib->fnSetDeviceConfig = (int(*)(int device, v3_aud_cnf *config))
        dlsym(aud_lib->handle, "HI_MPI_AI_SetPubAttr"))) {
        fprintf(stderr, "[v3_aud] Failed to acquire symbol HI_MPI_AI_SetPubAttr!\n");
        return EXIT_FAILURE;
    }

    if (!(aud_lib->fnDisableChannel = (int(*)(int device, int channel))
        dlsym(aud_lib->handle, "HI_MPI_AI_DisableChn"))) {
        fprintf(stderr, "[v3_aud] Failed to acquire symbol HI_MPI_AI_DisableChn!\n");
        return EXIT_FAILURE;
    }

    if (!(aud_lib->fnEnableChannel = (int(*)(int device, int channel))
        dlsym(aud_lib->handle, "HI_MPI_AI_EnableChn"))) {
        fprintf(stderr, "[v3_aud] Failed to acquire symbol HI_MPI_AI_EnableChn!\n");
        return EXIT_FAILURE;
    }


    if (!(aud_lib->fnSetVolume = (int(*)(int device, int channel, int dbLevel))
        dlsym(aud_lib->handle, "HI_MPI_AI_SetVqeVolume"))) {
        fprintf(stderr, "[v3_aud] Failed to acquire symbol HI_MPI_AI_SetVqeVolume!\n");
        return EXIT_FAILURE;
    }

    if (!(aud_lib->fnFreeFrame = (int(*)(int device, int channel, v3_aud_frm *frame, v3_aud_efrm *encFrame))
        dlsym(aud_lib->handle, "HI_MPI_AI_ReleaseFrame"))) {
        fprintf(stderr, "[v3_aud] Failed to acquire symbol HI_MPI_AI_ReleaseFrame!\n");
        return EXIT_FAILURE;
    }

    if (!(aud_lib->fnGetFrame = (int(*)(int device, int channel, v3_aud_frm *frame, v3_aud_efrm *encFrame, int millis))
        dlsym(aud_lib->handle, "HI_MPI_AI_GetFrame"))) {
        fprintf(stderr, "[v3_aud] Failed to acquire symbol HI_MPI_AI_GetFrame!\n");
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}

static void v3_aud_unload(v3_aud_impl *aud_lib) {
    if (aud_lib->handle) dlclose(aud_lib->handle);
    aud_lib->handle = NULL;
    memset(aud_lib, 0, sizeof(*aud_lib));
}