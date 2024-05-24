#pragma once

#include "tx_common.h"

typedef struct {
    char type[20];
    int addr;
    int bus;
} tx_isp_i2c;

typedef struct {
    char alias[32];
    int bus;
} tx_isp_spi;

typedef struct {
	char name[32];
	int spiMode;
	union {
		tx_isp_i2c i2c;
		tx_isp_spi spi;
	};
	unsigned short rstPin;
	unsigned short powDownPin;
	unsigned short powUpPin;
} tx_isp_snr;


typedef struct {
    void *handle;
    
    int (*fnExit)(void);
    int (*fnInit)(void);
    int (*fnLoadConfig)(char *path);

    int (*fnAddSensor)(tx_isp_snr *sensor);
    int (*fnDeleteSensor)(tx_isp_snr *sensor);
    int (*fnDisableSensor)(void);
    int (*fnEnableSensor)(void);
} tx_isp_impl;

static int tx_isp_load(tx_isp_impl *isp_lib) {
    if (!(isp_lib->handle = dlopen("libimp.so", RTLD_LAZY | RTLD_GLOBAL))) {
        fprintf(stderr, "[tx_isp] Failed to load library!\nError: %s\n", dlerror());
        return EXIT_FAILURE;
    }

    if (!(isp_lib->fnExit = (int(*)(int channel, tx_fs_chn *config))
        dlsym(isp_lib->handle, "IMP_ISP_Close"))) {
        fprintf(stderr, "[tx_isp] Failed to acquire symbol IMP_ISP_Close!\n");
        return EXIT_FAILURE;
    }

    if (!(isp_lib->fnInit = (int(*)(int channel))
        dlsym(isp_lib->handle, "IMP_ISP_Open"))) {
        fprintf(stderr, "[tx_isp] Failed to acquire symbol IMP_ISP_Open!\n");
        return EXIT_FAILURE;
    }

    if (!(isp_lib->fnLoadConfig = (int(*)(char *path))
        dlsym(isp_lib->handle, "IMP_ISP_SetDefaultBinPath"))) {
        fprintf(stderr, "[tx_isp] Failed to acquire symbol IMP_ISP_SetDefaultBinPath!\n");
        return EXIT_FAILURE;
    }

    if (!(isp_lib->fnAddSensor = (int(*)(tx_isp_snr *sensor))
        dlsym(isp_lib->handle, "IMP_ISP_AddSensor"))) {
        fprintf(stderr, "[tx_isp] Failed to acquire symbol IMP_ISP_AddSensor!\n");
        return EXIT_FAILURE;
    }

    if (!(isp_lib->fnDeleteSensor = (int(*)(tx_isp_snr *sensor))
        dlsym(isp_lib->handle, "IMP_ISP_DelSensor"))) {
        fprintf(stderr, "[tx_isp] Failed to acquire symbol IMP_ISP_DelSensor!\n");
        return EXIT_FAILURE;
    }

    if (!(isp_lib->fnDisableSensor = (int(*)(void))
        dlsym(isp_lib->handle, "IMP_ISP_DisableSensor"))) {
        fprintf(stderr, "[tx_isp] Failed to acquire symbol IMP_ISP_DisableSensor!\n");
        return EXIT_FAILURE;
    }

    if (!(isp_lib->fnEnableSensor = (int(*)(void))
        dlsym(isp_lib->handle, "IMP_ISP_EnableSensor"))) {
        fprintf(stderr, "[tx_isp] Failed to acquire symbol IMP_ISP_EnableSensor!\n");
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}

static void tx_isp_unload(tx_isp_impl *isp_lib) {
    if (isp_lib->handle) dlclose(isp_lib->handle);
    isp_lib->handle = NULL;
    memset(isp_lib, 0, sizeof(*isp_lib));
}