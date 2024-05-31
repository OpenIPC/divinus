#pragma once

#include "tx_common.h"

typedef enum {
    TX_ISP_COMM_I2C = 1,
    TX_ISP_COMM_SPI,
} tx_isp_comm;

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
	tx_isp_comm mode;
	union {
		tx_isp_i2c i2c;
		tx_isp_spi spi;
	};
	unsigned short rstPin;
	unsigned short powDownPin;
	unsigned short powUpPin;
} tx_isp_snr;

static tx_isp_snr tx_sensors[] = {
    { .name = "c23a98",  .mode = TX_ISP_COMM_I2C, .i2c.addr = 0x36 },
    { .name = "c4390",   .mode = TX_ISP_COMM_I2C, .i2c.addr = 0x36 },
    { .name = "gc1034",  .mode = TX_ISP_COMM_I2C, .i2c.addr = 0x21 },
    { .name = "gc2053",  .mode = TX_ISP_COMM_I2C, .i2c.addr = 0x37 },
    { .name = "gc4653",  .mode = TX_ISP_COMM_I2C, .i2c.addr = 0x29 },
    { .name = "imx307",  .mode = TX_ISP_COMM_I2C, .i2c.addr = 0x1a },
    { .name = "imx327",  .mode = TX_ISP_COMM_I2C, .i2c.addr = 0x36 },
    { .name = "imx335",  .mode = TX_ISP_COMM_I2C, .i2c.addr = 0x1a },
    { .name = "jxf23",   .mode = TX_ISP_COMM_I2C, .i2c.addr = 0x40 },
    { .name = "jxf37",   .mode = TX_ISP_COMM_I2C, .i2c.addr = 0x40 },
    { .name = "jxh62",   .mode = TX_ISP_COMM_I2C, .i2c.addr = 0x30 },
    { .name = "jxk03",   .mode = TX_ISP_COMM_I2C, .i2c.addr = 0x40 },
    { .name = "jxk05",   .mode = TX_ISP_COMM_I2C, .i2c.addr = 0x40 },
    { .name = "jxq03",   .mode = TX_ISP_COMM_I2C, .i2c.addr = 0x30 },
    { .name = "os02k10", .mode = TX_ISP_COMM_I2C, .i2c.addr = 0x36 },
    { .name = "os04b10", .mode = TX_ISP_COMM_I2C, .i2c.addr = 0x3c },
    { .name = "os05a10", .mode = TX_ISP_COMM_I2C, .i2c.addr = 0x36 },
    { .name = "ov5648",  .mode = TX_ISP_COMM_I2C, .i2c.addr = 0x36 },
    { .name = "sc2232h", .mode = TX_ISP_COMM_I2C, .i2c.addr = 0x30 },
    { .name = "sc2235",  .mode = TX_ISP_COMM_I2C, .i2c.addr = 0x30 },
    { .name = "sc2239",  .mode = TX_ISP_COMM_I2C, .i2c.addr = 0x30 },
    { .name = "sc2310",  .mode = TX_ISP_COMM_I2C, .i2c.addr = 0x30 },
    { .name = "sc2315e", .mode = TX_ISP_COMM_I2C, .i2c.addr = 0x30 },
    { .name = "sc2335",  .mode = TX_ISP_COMM_I2C, .i2c.addr = 0x30 },
    { .name = "sc3235",  .mode = TX_ISP_COMM_I2C, .i2c.addr = 0x30 },
    { .name = "sc3335",  .mode = TX_ISP_COMM_I2C, .i2c.addr = 0x30 },
    { .name = "sc4335",  .mode = TX_ISP_COMM_I2C, .i2c.addr = 0x30 },
    { .name = "sc5235",  .mode = TX_ISP_COMM_I2C, .i2c.addr = 0x30 }
};

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

    if (!(isp_lib->fnExit = (int(*)(void))
        dlsym(isp_lib->handle, "IMP_ISP_Close"))) {
        fprintf(stderr, "[tx_isp] Failed to acquire symbol IMP_ISP_Close!\n");
        return EXIT_FAILURE;
    }

    if (!(isp_lib->fnInit = (int(*)(void))
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
