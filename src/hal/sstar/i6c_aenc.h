#pragma once

#include "i6c_common.h"

#define I6C_AENC_CHN_NUM 16

typedef enum {
    I6C_AENC_BIT_16,
    I6C_AENC_BIT_32,
    I6C_AENC_BIT_END
} i6c_aenc_bit;

typedef enum {
    I6C_AENC_CLK_OFF,
    I6C_AENC_CLK_12_288M,
    I6C_AENC_CLK_16_384M,
    I6C_AENC_CLK_18_432M,
    I6C_AENC_CLK_24_576M,
    I6C_AENC_CLK_24M,
    I6C_AENC_CLK_48M
} i6c_aenc_clk;

typedef enum {
    I6C_AENC_G726_16K,
    I6C_AENC_G726_24K,
    I6C_AENC_G726_32K,
    I6C_AENC_G726_40K
} i6c_aenc_g726t;

typedef enum {
    I6C_AENC_INTF_I2S_MASTER,
    I6C_AENC_INTF_I2S_SLAVE,
    I6C_AENC_INTF_TDM_MASTER,
    I6C_AENC_INTF_TDM_SLAVE,
    I6C_AENC_INTF_END
} i6c_aenc_intf;

typedef enum {
    I6C_AENC_SND_MONO,
    I6C_AENC_SND_STEREO,
    I6C_AENC_SND_4CH,
    I6C_AENC_SND_6CH,
    I6C_AEND_SND_8CH,
    I6C_AENC_SND_END
} i6c_aenc_snd;

typedef enum {
    I6C_AENC_TYPE_G711A,
    I6C_AENC_TYPE_G711U,
    I6C_AENC_TYPE_G726,
} i6c_aenc_type;

typedef struct {
    i6c_aenc_intf intf;
    i6c_aenc_bit bit;
    int LeftJustOn;
    // Accept industry standards from 8000 to 96000Hz
    int rate;
    i6c_aenc_clk clock;
    char syncRxClkOn;
    unsigned int tdmSlotNum;
} i6c_aenc_i2s;

typedef struct {
    int reserved;
    i6c_aenc_snd sound;
    // Accept industry standards from 8000 to 96000Hz
    int rate;
    unsigned int periodSize;
    char interleavedOn;
} i6c_aenc_cnf;

typedef struct {
    void *handle;
    
    int (*fnDisableDevice)(int device);
    int (*fnEnableDevice)(int device, i6c_aenc_cnf *config);

    int (*fnDisableChannel)(int device, unsigned char channel);
    int (*fnEnableChannel)(int device, unsigned char channel);
} i6c_aenc_impl;

static int i6c_aenc_load(i6c_aenc_impl *aenc_lib) {
    if (!(aenc_lib->handle = dlopen("libmi_ai.so", RTLD_LAZY | RTLD_GLOBAL))) {
        fprintf(stderr, "[i6c_aenc] Failed to load library!\nError: %s\n", dlerror());
        return EXIT_FAILURE;
    }

    if (!(aenc_lib->fnDisableDevice = (int(*)(int device))
        dlsym(aenc_lib->handle, "MI_AI_Close"))) {
        fprintf(stderr, "[i6c_aenc] Failed to acquire symbol MI_AI_Close!\n");
        return EXIT_FAILURE;
    }

    if (!(aenc_lib->fnEnableDevice = (int(*)(int device, i6c_aenc_cnf *config))
        dlsym(aenc_lib->handle, "MI_AI_Open"))) {
        fprintf(stderr, "[i6c_aenc] Failed to acquire symbol MI_AI_Open!\n");
        return EXIT_FAILURE;
    }

    if (!(aenc_lib->fnDisableChannel = (int(*)(int device, unsigned char channel))
        dlsym(aenc_lib->handle, "MI_AI_DisableChnGroup"))) {
        fprintf(stderr, "[i6c_aenc] Failed to acquire symbol MI_AI_DisableChnGroup!\n");
        return EXIT_FAILURE;
    }

    if (!(aenc_lib->fnEnableChannel = (int(*)(int device, unsigned char channel))
        dlsym(aenc_lib->handle, "MI_AI_EnableChnGroup"))) {
        fprintf(stderr, "[i6c_aenc] Failed to acquire symbol MI_AI_EnableChnGroup!\n");
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}

static void i6c_aenc_unload(i6c_aenc_impl *aenc_lib) {
    if (aenc_lib->handle)
        dlclose(aenc_lib->handle = NULL);
    memset(aenc_lib, 0, sizeof(*aenc_lib));
}