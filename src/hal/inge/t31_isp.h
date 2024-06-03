#pragma once

#include "t31_common.h"

typedef enum {
    T31_ISP_COMM_I2C = 1,
    T31_ISP_COMM_SPI,
} t31_isp_comm;

typedef enum {
    T31_ISP_FLICK_OFF,
    T31_ISP_FLICK_50HZ,
    T31_ISP_FLICK_60HZ
} t31_isp_flick;

typedef struct {
    char type[20];
    int addr;
    int bus;
} t31_isp_i2c;

typedef struct {
    char alias[32];
    int bus;
} t31_isp_spi;

typedef struct {
	char name[32];
	t31_isp_comm mode;
	union {
		t31_isp_i2c i2c;
		t31_isp_spi spi;
	};
	unsigned short rstPin;
	unsigned short powDownPin;
	unsigned short powUpPin;
} t31_isp_snr;

static t31_common_dim t31_dims[] = {
    { .width = 1920, .height = 1080 }, // c23a98
    { .width = 2560, .height = 1440 }, // c4390
    { .width = 1280, .height = 720 },  // gc1034
    { .width = 1920, .height = 1080 }, // gc2053
    { .width = 2560, .height = 1440 }, // gc4653
    { .width = 1920, .height = 1080 }, // imx307
    { .width = 1920, .height = 1080 }, // imx327
    { .width = 2592, .height = 1944 }, // imx335
    { .width = 1920, .height = 1080 }, // jxf23
    { .width = 1920, .height = 1080 }, // jxf37
    { .width = 1280, .height = 720 },  // jxh62
    { .width = 2592, .height = 1944 }, // jxk03
    { .width = 2560, .height = 1920 }, // jxk05
    { .width = 2304, .height = 1296 }, // jxq03
    { .width = 1920, .height = 1080 }, // os02k10
    { .width = 2560, .height = 1440 }, // os04b10
    { .width = 2592, .height = 1944 }, // os05a10
    { .width = 2592, .height = 1944 }, // ov5648
    { .width = 1920, .height = 1080 }, // sc2232h
    { .width = 1920, .height = 1080 }, // sc2235
    { .width = 1920, .height = 1080 }, // sc2239
    { .width = 1920, .height = 1080 }, // sc2310
    { .width = 1920, .height = 1080 }, // sc2315
    { .width = 1920, .height = 1080 }, // sc2335
    { .width = 2304, .height = 1296 }, // sc3235
    { .width = 2304, .height = 1296 }, // sc3335
    { .width = 2560, .height = 1440 }, // sc4335
    { .width = 2560, .height = 1920 }  // sc5235
};

static t31_isp_snr t31_sensors[] = {
    { .name = "c23a98",  .mode = T31_ISP_COMM_I2C, .i2c.addr = 0x36 },
    { .name = "c4390",   .mode = T31_ISP_COMM_I2C, .i2c.addr = 0x36 },
    { .name = "gc1034",  .mode = T31_ISP_COMM_I2C, .i2c.addr = 0x21 },
    { .name = "gc2053",  .mode = T31_ISP_COMM_I2C, .i2c.addr = 0x37 },
    { .name = "gc4653",  .mode = T31_ISP_COMM_I2C, .i2c.addr = 0x29 },
    { .name = "imx307",  .mode = T31_ISP_COMM_I2C, .i2c.addr = 0x1a },
    { .name = "imx327",  .mode = T31_ISP_COMM_I2C, .i2c.addr = 0x36 },
    { .name = "imx335",  .mode = T31_ISP_COMM_I2C, .i2c.addr = 0x1a },
    { .name = "jxf23",   .mode = T31_ISP_COMM_I2C, .i2c.addr = 0x40 },
    { .name = "jxf37",   .mode = T31_ISP_COMM_I2C, .i2c.addr = 0x40 },
    { .name = "jxh62",   .mode = T31_ISP_COMM_I2C, .i2c.addr = 0x30 },
    { .name = "jxk03",   .mode = T31_ISP_COMM_I2C, .i2c.addr = 0x40 },
    { .name = "jxk05",   .mode = T31_ISP_COMM_I2C, .i2c.addr = 0x40 },
    { .name = "jxq03",   .mode = T31_ISP_COMM_I2C, .i2c.addr = 0x30 },
    { .name = "os02k10", .mode = T31_ISP_COMM_I2C, .i2c.addr = 0x36 },
    { .name = "os04b10", .mode = T31_ISP_COMM_I2C, .i2c.addr = 0x3c },
    { .name = "os05a10", .mode = T31_ISP_COMM_I2C, .i2c.addr = 0x36 },
    { .name = "ov5648",  .mode = T31_ISP_COMM_I2C, .i2c.addr = 0x36 },
    { .name = "sc2232h", .mode = T31_ISP_COMM_I2C, .i2c.addr = 0x30 },
    { .name = "sc2235",  .mode = T31_ISP_COMM_I2C, .i2c.addr = 0x30 },
    { .name = "sc2239",  .mode = T31_ISP_COMM_I2C, .i2c.addr = 0x30 },
    { .name = "sc2310",  .mode = T31_ISP_COMM_I2C, .i2c.addr = 0x30 },
    { .name = "sc2315e", .mode = T31_ISP_COMM_I2C, .i2c.addr = 0x30 },
    { .name = "sc2335",  .mode = T31_ISP_COMM_I2C, .i2c.addr = 0x30 },
    { .name = "sc3235",  .mode = T31_ISP_COMM_I2C, .i2c.addr = 0x30 },
    { .name = "sc3335",  .mode = T31_ISP_COMM_I2C, .i2c.addr = 0x30 },
    { .name = "sc4335",  .mode = T31_ISP_COMM_I2C, .i2c.addr = 0x30 },
    { .name = "sc5235",  .mode = T31_ISP_COMM_I2C, .i2c.addr = 0x30 }
};

typedef struct {
    void *handle;

    int (*fnEnableTuning)(void);
    int (*fnExit)(void);
    int (*fnInit)(void);
    int (*fnLoadConfig)(char *path);
    int (*fnSetAntiFlicker)(t31_isp_flick mode);
    int (*fnSetFlip)(int mode);
    int (*fnSetRunningMode)(int nightOn);

    int (*fnAddSensor)(t31_isp_snr *sensor);
    int (*fnDeleteSensor)(t31_isp_snr *sensor);
    int (*fnDisableSensor)(void);
    int (*fnEnableSensor)(void);
} t31_isp_impl;

static int t31_isp_load(t31_isp_impl *isp_lib) {
    if (!(isp_lib->handle = dlopen("libimp.so", RTLD_LAZY | RTLD_GLOBAL))) {
        fprintf(stderr, "[t31_isp] Failed to load library!\nError: %s\n", dlerror());
        return EXIT_FAILURE;
    }

    if (!(isp_lib->fnEnableTuning = (int(*)(void))
        dlsym(isp_lib->handle, "IMP_ISP_EnableTuning"))) {
        fprintf(stderr, "[t31_isp] Failed to acquire symbol IMP_ISP_EnableTuning!\n");
        return EXIT_FAILURE;
    }

    if (!(isp_lib->fnExit = (int(*)(void))
        dlsym(isp_lib->handle, "IMP_ISP_Close"))) {
        fprintf(stderr, "[t31_isp] Failed to acquire symbol IMP_ISP_Close!\n");
        return EXIT_FAILURE;
    }

    if (!(isp_lib->fnInit = (int(*)(void))
        dlsym(isp_lib->handle, "IMP_ISP_Open"))) {
        fprintf(stderr, "[t31_isp] Failed to acquire symbol IMP_ISP_Open!\n");
        return EXIT_FAILURE;
    }

    if (!(isp_lib->fnLoadConfig = (int(*)(char *path))
        dlsym(isp_lib->handle, "IMP_ISP_SetDefaultBinPath"))) {
        fprintf(stderr, "[t31_isp] Failed to acquire symbol IMP_ISP_SetDefaultBinPath!\n");
        return EXIT_FAILURE;
    }

    if (!(isp_lib->fnSetAntiFlicker = (int(*)(t31_isp_flick mode))
        dlsym(isp_lib->handle, "IMP_ISP_Tuning_SetAntiFlickerAttr"))) {
        fprintf(stderr, "[t31_isp] Failed to acquire symbol IMP_ISP_Tuning_SetAntiFlickerAttr!\n");
        return EXIT_FAILURE;
    }

    if (!(isp_lib->fnSetFlip = (int(*)(int mode))
        dlsym(isp_lib->handle, "IMP_ISP_Tuning_SetHVFLIP"))) {
        fprintf(stderr, "[t31_isp] Failed to acquire symbol IMP_ISP_Tuning_SetHVFLIP!\n");
        return EXIT_FAILURE;
    }

    if (!(isp_lib->fnSetRunningMode = (int(*)(int nightOn))
        dlsym(isp_lib->handle, "IMP_ISP_Tuning_SetISPRunningMode"))) {
        fprintf(stderr, "[t31_isp] Failed to acquire symbol IMP_ISP_Tuning_SetISPRunningMode!\n");
        return EXIT_FAILURE;
    }

    if (!(isp_lib->fnAddSensor = (int(*)(t31_isp_snr *sensor))
        dlsym(isp_lib->handle, "IMP_ISP_AddSensor"))) {
        fprintf(stderr, "[t31_isp] Failed to acquire symbol IMP_ISP_AddSensor!\n");
        return EXIT_FAILURE;
    }

    if (!(isp_lib->fnDeleteSensor = (int(*)(t31_isp_snr *sensor))
        dlsym(isp_lib->handle, "IMP_ISP_DelSensor"))) {
        fprintf(stderr, "[t31_isp] Failed to acquire symbol IMP_ISP_DelSensor!\n");
        return EXIT_FAILURE;
    }

    if (!(isp_lib->fnDisableSensor = (int(*)(void))
        dlsym(isp_lib->handle, "IMP_ISP_DisableSensor"))) {
        fprintf(stderr, "[t31_isp] Failed to acquire symbol IMP_ISP_DisableSensor!\n");
        return EXIT_FAILURE;
    }

    if (!(isp_lib->fnEnableSensor = (int(*)(void))
        dlsym(isp_lib->handle, "IMP_ISP_EnableSensor"))) {
        fprintf(stderr, "[t31_isp] Failed to acquire symbol IMP_ISP_EnableSensor!\n");
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}

static void t31_isp_unload(t31_isp_impl *isp_lib) {
    if (isp_lib->handle) dlclose(isp_lib->handle);
    isp_lib->handle = NULL;
    memset(isp_lib, 0, sizeof(*isp_lib));
}
