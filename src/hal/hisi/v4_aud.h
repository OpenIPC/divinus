#pragma once

#include "v4_common.h"

#define V4_AUD_CHN_NUM 2

typedef enum {
    V4_AUD_BIT_8,
    V4_AUD_BIT_16,
    V4_AUD_BIT_24
} v4_aud_bit;

typedef enum {
    V4_AUD_I2ST_INNERCODEC,
    V4_AUD_I2ST_INNERHDMI,
    V4_AUD_I2ST_EXTERN
} v4_aud_i2st;

typedef enum {
    V4_AUD_INTF_I2S_MASTER,
    V4_AUD_INTF_I2S_SLAVE,
    V4_AUD_INTF_PCM_SLAVE_STD,
    V4_AUD_INTF_PCM_SLAVE_NSTD,
    V4_AUD_INTF_PCM_MASTER_STD,
    V4_AUD_INTF_PCM_MASTER_NSTD,
    V4_AUD_INTF_END
} v4_aud_intf;

typedef struct {
    // Accept industry standards from
    // 8000 to 96000Hz, plus 64000Hz
    int rate;
    v4_aud_bit bit;
    v4_aud_intf intf;
    int stereoOn;
    // 8-to-16 bit, expand mode
    unsigned int expandOn;
    unsigned int frmNum;
    unsigned int packNumPerFrm;
    unsigned int chnNum;
    unsigned int syncRxClkOn;
    v4_aud_i2st i2sType;
} v4_aud_cnf;

typedef struct {
    v4_aud_bit bit;
    int stereoOn;
    char *addr[2];
    unsigned long long phy[2];
    unsigned long long timestamp;
    unsigned int sequence;
    unsigned int length;
    unsigned int poolId[2];
} v4_aud_frm;

typedef struct {
    v4_aud_frm frame;
    char isValid;
    char isSysBound;
} v4_aud_efrm;

typedef struct {
    void *handle, *handleGoke;
    
    int (*fnDisableDevice)(int device);
    int (*fnEnableDevice)(int device);
    int (*fnSetDeviceConfig)(int device, v4_aud_cnf *config);

    int (*fnDisableChannel)(int device, int channel);
    int (*fnEnableChannel)(int device, int channel);

    int (*fnFreeFrame)(int device, int channel, v4_aud_frm *frame, v4_aud_efrm *encFrame);
    int (*fnGetFrame)(int device, int channel, v4_aud_frm *frame, v4_aud_efrm *encFrame, int millis);
} v4_aud_impl;

static int v4_aud_load(v4_aud_impl *aud_lib) {
    if ( !(aud_lib->handle = dlopen("libmpi.so", RTLD_LAZY | RTLD_GLOBAL)) &&

        (!(aud_lib->handleGoke = dlopen("libgk_api.so", RTLD_LAZY | RTLD_GLOBAL)) ||
         !(aud_lib->handle = dlopen("libhi_mpi.so", RTLD_LAZY | RTLD_GLOBAL)))) {
        fprintf(stderr, "[v4_aud] Failed to load library!\nError: %s\n", dlerror());
        return EXIT_FAILURE;
    }

    if (!(aud_lib->fnDisableDevice = (int(*)(int device))
        dlsym(aud_lib->handle, "HI_MPI_AI_Disable"))) {
        fprintf(stderr, "[v4_aud] Failed to acquire symbol HI_MPI_AI_Disable!\n");
        return EXIT_FAILURE;
    }

    if (!(aud_lib->fnEnableDevice = (int(*)(int device))
        dlsym(aud_lib->handle, "HI_MPI_AI_Enable"))) {
        fprintf(stderr, "[v4_aud] Failed to acquire symbol HI_MPI_AI_Enable!\n");
        return EXIT_FAILURE;
    }

    if (!(aud_lib->fnSetDeviceConfig = (int(*)(int device, v4_aud_cnf *config))
        dlsym(aud_lib->handle, "HI_MPI_AI_SetPubAttr"))) {
        fprintf(stderr, "[v4_aud] Failed to acquire symbol HI_MPI_AI_SetPubAttr!\n");
        return EXIT_FAILURE;
    }

    if (!(aud_lib->fnDisableChannel = (int(*)(int device, int channel))
        dlsym(aud_lib->handle, "HI_MPI_AI_DisableChn"))) {
        fprintf(stderr, "[v4_aud] Failed to acquire symbol HI_MPI_AI_DisableChn!\n");
        return EXIT_FAILURE;
    }

    if (!(aud_lib->fnEnableChannel = (int(*)(int device, int channel))
        dlsym(aud_lib->handle, "HI_MPI_AI_EnableChn"))) {
        fprintf(stderr, "[v4_aud] Failed to acquire symbol HI_MPI_AI_EnableChn!\n");
        return EXIT_FAILURE;
    }

    if (!(aud_lib->fnFreeFrame = (int(*)(int device, int channel, v4_aud_frm *frame, v4_aud_efrm *encFrame))
        dlsym(aud_lib->handle, "HI_MPI_AI_ReleaseFrame"))) {
        fprintf(stderr, "[v4_aud] Failed to acquire symbol HI_MPI_AI_ReleaseFrame!\n");
        return EXIT_FAILURE;
    }

    if (!(aud_lib->fnGetFrame = (int(*)(int device, int channel, v4_aud_frm *frame, v4_aud_efrm *encFrame, int millis))
        dlsym(aud_lib->handle, "HI_MPI_AI_GetFrame"))) {
        fprintf(stderr, "[v4_aud] Failed to acquire symbol HI_MPI_AI_GetFrame!\n");
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}

static void v4_aud_unload(v4_aud_impl *aud_lib) {
    if (aud_lib->handle) dlclose(aud_lib->handle);
    aud_lib->handle = NULL;
    if (aud_lib->handleGoke) dlclose(aud_lib->handleGoke);
    aud_lib->handleGoke = NULL;
    memset(aud_lib, 0, sizeof(*aud_lib));
}