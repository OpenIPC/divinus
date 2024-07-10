#pragma once

#include "i6c_common.h"

#define I6C_AUD_CHN_NUM 8

typedef enum {
    I6C_AUD_BIT_16,
    I6C_AUD_BIT_32,
    I6C_AUD_BIT_END
} i6c_aud_bit;

typedef enum {
    I6C_AUD_CLK_OFF,
    I6C_AUD_CLK_12_288M,
    I6C_AUD_CLK_16_384M,
    I6C_AUD_CLK_18_432M,
    I6C_AUD_CLK_24_576M,
    I6C_AUD_CLK_24M,
    I6C_AUD_CLK_48M
} i6c_aud_clk;

typedef enum {
    I6C_AUD_G726_16K,
    I6C_AUD_G726_24K,
    I6C_AUD_G726_32K,
    I6C_AUD_G726_40K
} i6c_aud_g726t;

typedef enum {
    I6C_AUD_INTF_I2S_MASTER,
    I6C_AUD_INTF_I2S_SLAVE,
    I6C_AUD_INTF_TDM_MASTER,
    I6C_AUD_INTF_TDM_SLAVE,
    I6C_AUD_INTF_END
} i6c_aud_intf;

// Paired peripheral with two of its channels
typedef enum {
    I6C_AUD_INPUT_NONE,
    I6C_AUD_INPUT_ADC_AB,
    I6C_AUD_INPUT_ADC_CD,
    I6C_AUD_INPUT_DMIC_A_01,
    I6C_AUD_INPUT_DMIC_A_23,
    I6C_AUD_INPUT_I2S_A_01,
    I6C_AUD_INPUT_I2S_A_23,
    I6C_AUD_INPUT_I2S_A_45,
    I6C_AUD_INPUT_I2S_A_67,
    I6C_AUD_INPUT_I2S_A_89,
    I6C_AUD_INPUT_I2S_A_AB,
    I6C_AUD_INPUT_I2S_A_CD,
    I6C_AUD_INPUT_I2S_A_EF,
    I6C_AUD_INPUT_I2S_B_01,
    I6C_AUD_INPUT_I2S_B_23,
    I6C_AUD_INPUT_I2S_B_45,
    I6C_AUD_INPUT_I2S_B_67,
    I6C_AUD_INPUT_I2S_B_89,
    I6C_AUD_INPUT_I2S_B_AB,
    I6C_AUD_INPUT_I2S_B_CD,
    I6C_AUD_INPUT_I2S_B_EF,
    I6C_AUD_INPUT_I2S_C_01,
    I6C_AUD_INPUT_I2S_C_23,
    I6C_AUD_INPUT_I2S_C_45,
    I6C_AUD_INPUT_I2S_C_67,
    I6C_AUD_INPUT_I2S_C_89,
    I6C_AUD_INPUT_I2S_C_AB,
    I6C_AUD_INPUT_I2S_C_CD,
    I6C_AUD_INPUT_I2S_C_EF,
    I6C_AUD_INPUT_I2S_D_01,
    I6C_AUD_INPUT_I2S_D_23,
    I6C_AUD_INPUT_I2S_D_45,
    I6C_AUD_INPUT_I2S_D_67,
    I6C_AUD_INPUT_I2S_D_89,
    I6C_AUD_INPUT_I2S_D_AB,
    I6C_AUD_INPUT_I2S_D_CD,
    I6C_AUD_INPUT_I2S_D_EF,
    I6C_AUD_INPUT_ECHO_A,
    I6C_AUD_INPUT_HDMI_A,
    I6C_AUD_INPUT_DMIC_A_45,
    I6C_AUD_INPUT_END
} i6c_aud_input;

typedef enum {
    I6C_AUD_SND_MONO   = 1,
    I6C_AUD_SND_STEREO = 2,
    I6C_AUD_SND_4CH    = 4,
    I6C_AUD_SND_6CH    = 6,
    I6C_AUD_SND_8CH    = 8,
    I6C_AUD_SND_END
} i6c_aud_snd;

typedef enum {
    I6C_AUD_TYPE_G711A,
    I6C_AUD_TYPE_G711U,
    I6C_AUD_TYPE_G726,
} i6c_aud_type;

typedef struct {
    i6c_aud_intf intf;
    i6c_aud_bit bit;
    int leftJustOn;
    // Accept industry standards from 8000 to 96000Hz
    int rate;
    i6c_aud_clk clock;
    char syncRxClkOn;
    unsigned int tdmSlotNum;
} i6c_aud_i2s;

typedef struct {
    int reserved;
    i6c_aud_snd sound;
    // Accept industry standards from 8000 to 96000Hz
    int rate;
    unsigned int periodSize;
    char interleavedOn;
} i6c_aud_cnf;

typedef struct {
    // If interleaved, only one address present
    void *addr[I6C_AUD_CHN_NUM];
    unsigned int length[I6C_AUD_CHN_NUM];
    unsigned long long timestamp;
    unsigned long long sequence;
} i6c_aud_frm;

typedef struct {
    void *handle;

    int (*fnAttachToDevice)(int device, i6c_aud_input inputs[], unsigned char inputSize);
    int (*fnDisableDevice)(int device);
    int (*fnEnableDevice)(int device, i6c_aud_cnf *config);

    int (*fnDisableGroup)(int device, unsigned char group);
    int (*fnEnableGroup)(int device, unsigned char group);

    int (*fnSetI2SConfig)(i6c_aud_input input, i6c_aud_i2s *config);

    int (*fnSetGain)(i6c_aud_input input, char leftLevel, char rightLevel);
    int (*fnSetMute)(int device, unsigned char group, char actives[], unsigned char activeSize);
    int (*fnSetVolume)(int device, unsigned char group, char levels[], unsigned char levelSize);

    int (*fnFreeFrame)(int device, unsigned char group, i6c_aud_frm *frame, i6c_aud_frm *echoFrame);
    int (*fnGetFrame)(int device, unsigned char group, i6c_aud_frm *frame, i6c_aud_frm *echoFrame, int millis);
} i6c_aud_impl;

static int i6c_aud_load(i6c_aud_impl *aud_lib) {
    if (!(aud_lib->handle = dlopen("libmi_ai.so", RTLD_LAZY | RTLD_GLOBAL)))
        HAL_ERROR("i6c_aud", "Failed to load library!\nError: %s\n", dlerror());

    if (!(aud_lib->fnAttachToDevice = (int(*)(int device, i6c_aud_input inputs[], unsigned char inputSize))
        hal_symbol_load("i6c_aud", aud_lib->handle, "MI_AI_AttachIf")))
        return EXIT_FAILURE;

    if (!(aud_lib->fnDisableDevice = (int(*)(int device))
        hal_symbol_load("i6c_aud", aud_lib->handle, "MI_AI_Close")))
        return EXIT_FAILURE;

    if (!(aud_lib->fnEnableDevice = (int(*)(int device, i6c_aud_cnf *config))
        hal_symbol_load("i6c_aud", aud_lib->handle, "MI_AI_Open")))
        return EXIT_FAILURE;

    if (!(aud_lib->fnDisableGroup = (int(*)(int device, unsigned char group))
        hal_symbol_load("i6c_aud", aud_lib->handle, "MI_AI_DisableChnGroup")))
        return EXIT_FAILURE;

    if (!(aud_lib->fnEnableGroup = (int(*)(int device, unsigned char group))
        hal_symbol_load("i6c_aud", aud_lib->handle, "MI_AI_EnableChnGroup")))
        return EXIT_FAILURE;

    if (!(aud_lib->fnSetI2SConfig = (int(*)(i6c_aud_input input, i6c_aud_i2s *config))
        hal_symbol_load("i6c_aud", aud_lib->handle, "MI_AI_SetI2SConfig")))
        return EXIT_FAILURE;

    if (!(aud_lib->fnSetGain = (int(*)(i6c_aud_input input, char leftLevel, char rightLevel))
        hal_symbol_load("i6c_aud", aud_lib->handle, "MI_AI_SetIfGain")))
        return EXIT_FAILURE;

    if (!(aud_lib->fnSetMute = (int(*)(int device, unsigned char group, char actives[], unsigned char activeSize))
        hal_symbol_load("i6c_aud", aud_lib->handle, "MI_AI_SetMute")))
        return EXIT_FAILURE;

    if (!(aud_lib->fnSetVolume = (int(*)(int device, unsigned char group, char levels[], unsigned char levelSize))
        hal_symbol_load("i6c_aud", aud_lib->handle, "MI_AI_SetGain")))
        return EXIT_FAILURE;

    if (!(aud_lib->fnFreeFrame = (int(*)(int device, unsigned char group, i6c_aud_frm *frame, i6c_aud_frm *echoFrame))
        hal_symbol_load("i6c_aud", aud_lib->handle, "MI_AI_ReleaseData")))
        return EXIT_FAILURE;

    if (!(aud_lib->fnGetFrame = (int(*)(int device, unsigned char group, i6c_aud_frm *frame, i6c_aud_frm *echoFrame, int millis))
        hal_symbol_load("i6c_aud", aud_lib->handle, "MI_AI_Read")))
        return EXIT_FAILURE;

    return EXIT_SUCCESS;
}

static void i6c_aud_unload(i6c_aud_impl *aud_lib) {
    if (aud_lib->handle) dlclose(aud_lib->handle);
    aud_lib->handle = NULL;
    memset(aud_lib, 0, sizeof(*aud_lib));
}