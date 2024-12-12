#pragma once

#include "m6_common.h"

#define M6_AUD_CHN_NUM 16

typedef enum {
    M6_AUD_BIT_16,
    M6_AUD_BIT_24,
    M6_AUD_BIT_32,
    M6_AUD_BIT_END
} m6_aud_bit;

typedef enum {
    M6_AUD_CLK_OFF,
    M6_AUD_CLK_12_288M,
    M6_AUD_CLK_16_384M,
    M6_AUD_CLK_18_432M,
    M6_AUD_CLK_24_576M,
    M6_AUD_CLK_24M,
    M6_AUD_CLK_48M
} m6_aud_clk;

typedef enum {
    M6_AUD_G726_16K,
    M6_AUD_G726_24K,
    M6_AUD_G726_32K,
    M6_AUD_G726_40K
} m6_aud_g726t;

typedef enum {
    M6_AUD_INTF_I2S_MASTER,
    M6_AUD_INTF_I2S_SLAVE,
    M6_AUD_INTF_TDM_MASTER,
    M6_AUD_INTF_TDM_SLAVE,
    M6_AUD_INTF_END
} m6_aud_intf;

typedef enum {
    M6_AUD_SND_MONO,
    M6_AUD_SND_STEREO,
    M6_AUD_SND_QUEUE,
    M6_AUD_SND_END
} m6_aud_snd;

typedef enum {
    M6_AUD_TYPE_G711A,
    M6_AUD_TYPE_G711U,
    M6_AUD_TYPE_G726,
} m6_aud_type;

typedef struct {
    int leftJustOn;
    m6_aud_clk clock;
    char syncRxClkOn;
    unsigned int tdmSlotNum;
    m6_aud_bit bit;
} m6_aud_i2s;

typedef struct {
    // Accept industry standards from 8000 to 96000Hz
    int rate;
    m6_aud_bit bit;
    m6_aud_intf intf;
    m6_aud_snd sound;
    unsigned int frmNum;
    unsigned int packNumPerFrm;
    unsigned int codecChnNum;
    unsigned int chnNum;
    m6_aud_i2s i2s;
} m6_aud_cnf;

typedef struct {
    m6_aud_bit bit;
    m6_aud_snd sound;
    void *addr[M6_AUD_CHN_NUM];
    unsigned long long timestamp;
    unsigned int sequence;
    unsigned int length[M6_AUD_CHN_NUM];
    unsigned int poolId[2];
    void *pcmAddr[M6_AUD_CHN_NUM];
    unsigned int pcmLength[M6_AUD_CHN_NUM];
} m6_aud_frm;

typedef struct {
    m6_aud_frm frame;
    char isValid;
} m6_aud_efrm;

typedef struct {
    // Accept industry standards from 8000 to 96000Hz
    int rate;
    m6_aud_snd sound;
} m6_aud_g711;

typedef struct {
    // Accept industry standards from 8000 to 96000Hz
    int rate;
    m6_aud_snd sound;
    m6_aud_g726t type;
} m6_aud_g726;

typedef struct {
    m6_aud_type type;
    union {
        m6_aud_g711 g711;
        m6_aud_g726t g726;
    };
} m6_aud_para;

typedef struct {
    void *handle;
    
    int (*fnDisableDevice)(int device);
    int (*fnEnableDevice)(int device);
    int (*fnSetDeviceConfig)(int device, m6_aud_cnf *config);

    int (*fnDisableChannel)(int device, int channel);
    int (*fnEnableChannel)(int device, int channel);

    int (*fnDisableEncoding)(int device, int channel);
    int (*fnEnableEncoding)(int device, int channel);
    int (*fnSetEncodingParam)(int device, int channel, m6_aud_para *param);

    int (*fnSetMute)(int device, int channel, char active);
    int (*fnSetVolume)(int device, int channel, int dbLevel);

    int (*fnFreeFrame)(int device, int channel, m6_aud_frm *frame, m6_aud_efrm *encFrame);
    int (*fnGetFrame)(int device, int channel, m6_aud_frm *frame, m6_aud_efrm *encFrame, int millis);
} m6_aud_impl;

static int m6_aud_load(m6_aud_impl *aud_lib) {
    if (!(aud_lib->handle = dlopen("libmi_ai.so", RTLD_LAZY | RTLD_GLOBAL)))
        HAL_ERROR("m6_aud", "Failed to load library!\nError: %s\n", dlerror());

    if (!(aud_lib->fnDisableDevice = (int(*)(int device))
        hal_symbol_load("m6_aud", aud_lib->handle, "MI_AI_Disable")))
        return EXIT_FAILURE;

    if (!(aud_lib->fnEnableDevice = (int(*)(int device))
        hal_symbol_load("m6_aud", aud_lib->handle, "MI_AI_Enable")))
        return EXIT_FAILURE;

    if (!(aud_lib->fnSetDeviceConfig = (int(*)(int device, m6_aud_cnf *config))
        hal_symbol_load("m6_aud", aud_lib->handle, "MI_AI_SetPubAttr")))
        return EXIT_FAILURE;

    if (!(aud_lib->fnDisableChannel = (int(*)(int device, int channel))
        hal_symbol_load("m6_aud", aud_lib->handle, "MI_AI_DisableChn")))
        return EXIT_FAILURE;

    if (!(aud_lib->fnEnableChannel = (int(*)(int device, int channel))
        hal_symbol_load("m6_aud", aud_lib->handle, "MI_AI_EnableChn")))
        return EXIT_FAILURE;

    if (!(aud_lib->fnDisableEncoding = (int(*)(int device, int channel))
        hal_symbol_load("m6_aud", aud_lib->handle, "MI_AI_DisableAenc")))
        return EXIT_FAILURE;

    if (!(aud_lib->fnEnableEncoding = (int(*)(int device, int channel))
        hal_symbol_load("m6_aud", aud_lib->handle, "MI_AI_EnableAenc")))
        return EXIT_FAILURE;

    if (!(aud_lib->fnSetEncodingParam = (int(*)(int device, int channel, m6_aud_para *param))
        hal_symbol_load("m6_aud", aud_lib->handle, "MI_AI_SetAencAttr")))
        return EXIT_FAILURE;

    if (!(aud_lib->fnSetMute = (int(*)(int device, int channel, char active))
        hal_symbol_load("m6_aud", aud_lib->handle, "MI_AI_SetMute")))
        return EXIT_FAILURE;

    if (!(aud_lib->fnSetVolume = (int(*)(int device, int channel, int dbLevel))
        hal_symbol_load("m6_aud", aud_lib->handle, "MI_AI_SetVqeVolume")))
        return EXIT_FAILURE;

    if (!(aud_lib->fnFreeFrame = (int(*)(int device, int channel, m6_aud_frm *frame, m6_aud_efrm *encFrame))
        hal_symbol_load("m6_aud", aud_lib->handle, "MI_AI_ReleaseFrame")))
        return EXIT_FAILURE;

    if (!(aud_lib->fnGetFrame = (int(*)(int device, int channel, m6_aud_frm *frame, m6_aud_efrm *encFrame, int millis))
        hal_symbol_load("m6_aud", aud_lib->handle, "MI_AI_GetFrame")))
        return EXIT_FAILURE;

    return EXIT_SUCCESS;
}

static void m6_aud_unload(m6_aud_impl *aud_lib) {
    if (aud_lib->handle) dlclose(aud_lib->handle);
    aud_lib->handle = NULL;
    memset(aud_lib, 0, sizeof(*aud_lib));
}