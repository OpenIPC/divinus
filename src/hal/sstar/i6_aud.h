#pragma once

#include "i6_common.h"

#define I6_AUD_CHN_NUM 16

typedef enum {
    I6_AUD_CLK_OFF,
    I6_AUD_CLK_12_288M,
    I6_AUD_CLK_16_384M,
    I6_AUD_CLK_18_432M,
    I6_AUD_CLK_24_576M,
    I6_AUD_CLK_24M,
    I6_AUD_CLK_48M
} i6_aud_clk;

typedef enum {
    I6_AUD_G726_16K,
    I6_AUD_G726_24K,
    I6_AUD_G726_32K,
    I6_AUD_G726_40K
} i6_aud_g726t;

typedef enum {
    I6_AUD_INTF_I2S_MASTER,
    I6_AUD_INTF_I2S_SLAVE,
    I6_AUD_INTF_TDM_MASTER,
    I6_AUD_INTF_TDM_SLAVE,
    I6_AUD_INTF_END
} i6_aud_intf;

typedef enum {
    I6_AUD_SND_MONO,
    I6_AUD_SND_STEREO,
    I6_AUD_SND_QUEUE,
    I6_AUD_SND_END
} i6_aud_snd;

typedef enum {
    I6_AUD_TYPE_G711A,
    I6_AUD_TYPE_G711U,
    I6_AUD_TYPE_G726,
} i6_aud_type;

typedef struct {
    int LeftJustOn;
    i6_aud_clk clock;
    char syncRxClkOn;
} i6_aud_i2s;

typedef struct {
    // Accept industry standards from 8000 to 96000Hz
    int rate;
    int bit24On;
    i6_aud_intf intf;
    i6_aud_snd sound;
    unsigned int frmNum;
    unsigned int packNumPerFrm;
    unsigned int codecChnNum;
    unsigned int chnNum;
    i6_aud_i2s i2s;
} i6_aud_cnf;

typedef struct {
    int bit24On;
    i6_aud_snd sound;
    void *addr[I6_AUD_CHN_NUM];
    unsigned long long timestamp;
    unsigned int sequence;
    unsigned int length;
    unsigned int poolId[2];
    void *pcmAddr[I6_AUD_CHN_NUM];
    unsigned int pcmLength;
} i6_aud_frm;

typedef struct {
    i6_aud_frm frame;
    char isValid;
} i6_aud_efrm;

typedef struct {
    // Accept industry standards from 8000 to 96000Hz
    int rate;
    i6_aud_snd sound;
} i6_aud_g711;

typedef struct {
    // Accept industry standards from 8000 to 96000Hz
    int rate;
    i6_aud_snd sound;
    i6_aud_g726t type;
} i6_aud_g726;

typedef struct {
    i6_aud_type type;
    union {
        i6_aud_g711 g711;
        i6_aud_g726t g726;
    };
} i6_aud_para;

typedef struct {
    void *handle;
    
    int (*fnDisableDevice)(int device);
    int (*fnEnableDevice)(int device);
    int (*fnSetDeviceConfig)(int device, i6_aud_cnf *config);

    int (*fnDisableChannel)(int device, int channel);
    int (*fnEnableChannel)(int device, int channel);

    int (*fnDisableEncoding)(int device, int channel);
    int (*fnEnableEncoding)(int device, int channel);
    int (*fnSetEncodingParam)(int device, int channel, i6_aud_para *param);

    int (*fnSetMute)(int device, int channel, char active);
    int (*fnSetVolume)(int device, int channel, int dbLevel);

    int (*fnFreeFrame)(int device, int channel, i6_aud_frm *frame, i6_aud_efrm *encFrame);
    int (*fnGetFrame)(int device, int channel, i6_aud_frm *frame, i6_aud_efrm *encFrame, int millis);
} i6_aud_impl;

static int i6_aud_load(i6_aud_impl *aud_lib) {
    if (!(aud_lib->handle = dlopen("libmi_ai.so", RTLD_LAZY | RTLD_GLOBAL))) {
        fprintf(stderr, "[i6_aud] Failed to load library!\nError: %s\n", dlerror());
        return EXIT_FAILURE;
    }

    if (!(aud_lib->fnDisableDevice = (int(*)(int device))
        dlsym(aud_lib->handle, "MI_AI_Disable"))) {
        fprintf(stderr, "[i6_aud] Failed to acquire symbol MI_AI_Disable!\n");
        return EXIT_FAILURE;
    }

    if (!(aud_lib->fnEnableDevice = (int(*)(int device))
        dlsym(aud_lib->handle, "MI_AI_Enable"))) {
        fprintf(stderr, "[i6_aud] Failed to acquire symbol MI_AI_Enable!\n");
        return EXIT_FAILURE;
    }

    if (!(aud_lib->fnSetDeviceConfig = (int(*)(int device, i6_aud_cnf *config))
        dlsym(aud_lib->handle, "MI_AI_SetPubAttr"))) {
        fprintf(stderr, "[i6_aud] Failed to acquire symbol MI_AI_SetPubAttr!\n");
        return EXIT_FAILURE;
    }

    if (!(aud_lib->fnDisableChannel = (int(*)(int device, int channel))
        dlsym(aud_lib->handle, "MI_AI_DisableChn"))) {
        fprintf(stderr, "[i6_aud] Failed to acquire symbol MI_AI_DisableChn!\n");
        return EXIT_FAILURE;
    }

    if (!(aud_lib->fnEnableChannel = (int(*)(int device, int channel))
        dlsym(aud_lib->handle, "MI_AI_EnableChn"))) {
        fprintf(stderr, "[i6_aud] Failed to acquire symbol MI_AI_EnableChn!\n");
        return EXIT_FAILURE;
    }

    if (!(aud_lib->fnDisableEncoding = (int(*)(int device, int channel))
        dlsym(aud_lib->handle, "MI_AI_DisableAenc"))) {
        fprintf(stderr, "[i6_aud] Failed to acquire symbol MI_AI_DisableChn!\n");
        return EXIT_FAILURE;
    }

    if (!(aud_lib->fnEnableEncoding = (int(*)(int device, int channel))
        dlsym(aud_lib->handle, "MI_AI_EnableAenc"))) {
        fprintf(stderr, "[i6_aud] Failed to acquire symbol MI_AI_EnableChn!\n");
        return EXIT_FAILURE;
    }

    if (!(aud_lib->fnSetEncodingParam = (int(*)(int device, int channel, i6_aud_para *param))
        dlsym(aud_lib->handle, "MI_AI_SetAencAttr"))) {
        fprintf(stderr, "[i6_aud] Failed to acquire symbol MI_AI_SetAencAttr!\n");
        return EXIT_FAILURE;
    }

    if (!(aud_lib->fnSetMute = (int(*)(int device, int channel, char active))
        dlsym(aud_lib->handle, "MI_AI_SetMute"))) {
        fprintf(stderr, "[i6_aud] Failed to acquire symbol MI_AI_SetMute!\n");
        return EXIT_FAILURE;
    }

    if (!(aud_lib->fnSetVolume = (int(*)(int device, int channel, int dbLevel))
        dlsym(aud_lib->handle, "MI_AI_SetVqeVolume"))) {
        fprintf(stderr, "[i6_aud] Failed to acquire symbol MI_AI_SetVqeVolume!\n");
        return EXIT_FAILURE;
    }

    if (!(aud_lib->fnFreeFrame = (int(*)(int device, int channel, i6_aud_frm *frame, i6_aud_efrm *encFrame))
        dlsym(aud_lib->handle, "MI_AI_ReleaseFrame"))) {
        fprintf(stderr, "[i6_aud] Failed to acquire symbol MI_AI_ReleaseFrame!\n");
        return EXIT_FAILURE;
    }

    if (!(aud_lib->fnGetFrame = (int(*)(int device, int channel, i6_aud_frm *frame, i6_aud_efrm *encFrame, int millis))
        dlsym(aud_lib->handle, "MI_AI_GetFrame"))) {
        fprintf(stderr, "[i6_aud] Failed to acquire symbol MI_AI_GetFrame!\n");
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}

static void i6_aud_unload(i6_aud_impl *aud_lib) {
    if (aud_lib->handle)
        dlclose(aud_lib->handle = NULL);
    memset(aud_lib, 0, sizeof(*aud_lib));
}