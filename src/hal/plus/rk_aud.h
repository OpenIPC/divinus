#pragma once

#include "rk_common.h"

#define RK_AUD_CHN_NUM 2

typedef enum {
    RK_AUD_BIT_8,
    RK_AUD_BIT_16,
    RK_AUD_BIT_24,
    RK_AUD_BIT_32,
    RK_AUD_BIT_FLOAT,
    RK_AUD_BIT_END
} rk_aud_bit;

typedef enum {
    RK_AUD_MODE_MONO,
    RK_AUD_MODE_STEREO,
    RK_AUD_MODE_4CH,
    RK_AUD_MODE_6CH,
    RK_AUD_MODE_8CH,
    RK_AUD_MODE_END
} rk_aud_mode;

typedef struct {
    unsigned int cardChnNum, cardRate;
    rk_aud_bit cardBit;
    // Accept industry standards from
    // 8000 to 96000Hz, plus 64000Hz
    unsigned int rate;
    rk_aud_bit bit;
    rk_aud_mode mode;
    // 8-to-16 bit, expand mode
    unsigned int expandOn;
    unsigned int frmNum;
    unsigned int packNumPerFrm;
    unsigned int chnNum;
    // If not setted, will use the device index
    // to open the audio card
    char cardName­[8];
} rk_aud_cnf;

typedef struct {
    void *mbBlk;
    rk_aud_bit bit;
    rk_aud_mode mode;
    unsigned long long timestamp;
    unsigned int sequence;
    unsigned int length;
    int bypassMbOn;
} rk_aud_frm;

typedef struct {
    rk_aud_frm frame;
    char isValid;
    char isSysBound;
} rk_aud_efrm;

typedef struct {
    void *handle;
    
    int (*fnDisableDevice)(int device);
    int (*fnEnableDevice)(int device);
    int (*fnSetDeviceConfig)(int device, rk_aud_cnf *config);

    int (*fnDisableChannel)(int device, int channel);
    int (*fnEnableChannel)(int device, int channel);

    int (*fnFreeFrame)(int device, int channel, rk_aud_frm *frame, rk_aud_efrm *encFrame);
    int (*fnGetFrame)(int device, int channel, rk_aud_frm *frame, rk_aud_efrm *encFrame, int millis);
} rk_aud_impl;

static int rk_aud_load(rk_aud_impl *aud_lib) {
    if (!(aud_lib->handle = dlopen("librockit.so", RTLD_LAZY | RTLD_GLOBAL))) {
        fprintf(stderr, "[rk_aud] Failed to load library!\nError: %s\n", dlerror());
        return EXIT_FAILURE;
    }

    if (!(aud_lib->fnDisableDevice = (int(*)(int device))
        dlsym(aud_lib->handle, "RK_MPI_AI_Disable"))) {
        fprintf(stderr, "[rk_aud] Failed to acquire symbol RK_MPI_AI_Disable!\n");
        return EXIT_FAILURE;
    }

    if (!(aud_lib->fnEnableDevice = (int(*)(int device))
        dlsym(aud_lib->handle, "RK_MPI_AI_Enable"))) {
        fprintf(stderr, "[rk_aud] Failed to acquire symbol RK_MPI_AI_Enable!\n");
        return EXIT_FAILURE;
    }

    if (!(aud_lib->fnSetDeviceConfig = (int(*)(int device, rk_aud_cnf *config))
        dlsym(aud_lib->handle, "RK_MPI_AI_SetPubAttr"))) {
        fprintf(stderr, "[rk_aud] Failed to acquire symbol RK_MPI_AI_SetPubAttr!\n");
        return EXIT_FAILURE;
    }

    if (!(aud_lib->fnDisableChannel = (int(*)(int device, int channel))
        dlsym(aud_lib->handle, "RK_MPI_AI_DisableChn"))) {
        fprintf(stderr, "[rk_aud] Failed to acquire symbol RK_MPI_AI_DisableChn!\n");
        return EXIT_FAILURE;
    }

    if (!(aud_lib->fnEnableChannel = (int(*)(int device, int channel))
        dlsym(aud_lib->handle, "RK_MPI_AI_EnableChn"))) {
        fprintf(stderr, "[rk_aud] Failed to acquire symbol RK_MPI_AI_EnableChn!\n");
        return EXIT_FAILURE;
    }

    if (!(aud_lib->fnFreeFrame = (int(*)(int device, int channel, rk_aud_frm *frame, rk_aud_efrm *encFrame))
        dlsym(aud_lib->handle, "RK_MPI_AI_ReleaseFrame"))) {
        fprintf(stderr, "[rk_aud] Failed to acquire symbol RK_MPI_AI_ReleaseFrame!\n");
        return EXIT_FAILURE;
    }

    if (!(aud_lib->fnGetFrame = (int(*)(int device, int channel, rk_aud_frm *frame, rk_aud_efrm *encFrame, int millis))
        dlsym(aud_lib->handle, "RK_MPI_AI_GetFrame"))) {
        fprintf(stderr, "[rk_aud] Failed to acquire symbol RK_MPI_AI_GetFrame!\n");
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}

static void rk_aud_unload(rk_aud_impl *aud_lib) {
    if (aud_lib->handle) dlclose(aud_lib->handle);
    aud_lib->handle = NULL;
    memset(aud_lib, 0, sizeof(*aud_lib));
}