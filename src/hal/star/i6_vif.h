#pragma once

#include "i6_common.h"

typedef enum {
    I6_VIF_FRATE_FULL,
    I6_VIF_FRATE_HALF,
    I6_VIF_FRATE_QUART,
    I6_VIF_FRATE_OCTANT,
    I6_VIF_FRATE_3QUARTS,
    I6_VIF_FRATE_END
} i6_vif_frate;

typedef enum {
    I6_VIF_WORK_1MULTIPLEX,
    I6_VIF_WORK_2MULTIPLEX,
    I6_VIF_WORK_4MULTIPLEX,
    I6_VIF_WORK_RGB_REALTIME,
    I6_VIF_WORK_RGB_FRAME,
    I6_VIF_WORK_END
} i6_vif_work;

typedef struct {
    i6_common_intf intf;
    i6_vif_work work;
    i6_common_hdr hdr;
    i6_common_edge edge;
    i6_common_input input;
    char bitswap;
    i6_common_sync sync;
} i6_vif_dev;

typedef struct {
    i6_common_rect capt;
    i6_common_dim dest;
    // Values 0-3 correspond to No, Top, Bottom, Both
    int field;
    int interlaceOn;
    i6_common_pixfmt pixFmt;
    i6_vif_frate frate;
    unsigned int frameLineCnt;
} i6_vif_port;

#define MI_S32 int
#define MI_U64 unsigned long long

typedef enum

{

    E_MI_VIF_CALLBACK_ISR,

    E_MI_VIF_CALLBACK_MAX,

} MI_VIF_CallBackMode_e;

typedef enum

{

    E_MI_VIF_IRQ_FRAMESTART, //frame start irq

    E_MI_VIF_IRQ_FRAMEEND, //frame end irq

    E_MI_VIF_IRQ_LINEHIT, //frame line hit irq

    E_MI_VIF_IRQ_MAX,

} MI_VIF_IrqType_e;

typedef MI_S32 (*MI_VIF_CALLBK_FUNC)(MI_U64 u64Data);

typedef struct MI_VIF_CallBackParam_s

{

    MI_VIF_CallBackMode_e eCallBackMode;

    MI_VIF_IrqType_e eIrqType;

    MI_VIF_CALLBK_FUNC pfnCallBackFunc;

    MI_U64 u64Data;

} MI_VIF_CallBackParam_t;



typedef struct {
    void *handle;

    int (*fnDisableDevice)(int device);
    int (*fnEnableDevice)(int device);
    int (*fnSetDeviceConfig)(int device, i6_vif_dev *config);

    int (*fnDisablePort)(int channel, int port);
    int (*fnEnablePort)(int channel, int port);
    int (*fnSetPortConfig)(int channel, int port, i6_vif_port *config);
    int (*fnSetCB)(int u32VifChn, MI_VIF_CallBackParam_t *config);
    int (*fnUnsetCB)(int u32VifChn, MI_VIF_CallBackParam_t *config);
} i6_vif_impl;

static int i6_vif_load(i6_vif_impl *vif_lib) {
    if (!(vif_lib->handle = dlopen("libmi_vif.so", RTLD_LAZY | RTLD_GLOBAL)))
        HAL_ERROR("i6_vif", "Failed to load library!\nError: %s\n", dlerror());

    if (!(vif_lib->fnDisableDevice = (int(*)(int device))
        hal_symbol_load("i6_vif", vif_lib->handle, "MI_VIF_DisableDev")))
        return EXIT_FAILURE;

    if (!(vif_lib->fnEnableDevice = (int(*)(int device))
        hal_symbol_load("i6_vif", vif_lib->handle, "MI_VIF_EnableDev")))
        return EXIT_FAILURE;

    if (!(vif_lib->fnSetDeviceConfig = (int(*)(int device, i6_vif_dev *config))
        hal_symbol_load("i6_vif", vif_lib->handle, "MI_VIF_SetDevAttr")))
        return EXIT_FAILURE;

    if (!(vif_lib->fnDisablePort = (int(*)(int channel, int port))
        hal_symbol_load("i6_vif", vif_lib->handle, "MI_VIF_DisableChnPort")))
        return EXIT_FAILURE;

    if (!(vif_lib->fnEnablePort = (int(*)(int channel, int port))
        hal_symbol_load("i6_vif", vif_lib->handle, "MI_VIF_EnableChnPort")))
        return EXIT_FAILURE;

    if (!(vif_lib->fnSetPortConfig = (int(*)(int channel, int port, i6_vif_port *config))
        hal_symbol_load("i6_vif", vif_lib->handle, "MI_VIF_SetChnPortAttr")))
        return EXIT_FAILURE;

    if (!(vif_lib->fnSetCB = (int(*)(int channel, MI_VIF_CallBackParam_t *config))
        hal_symbol_load("i6_vif", vif_lib->handle, "MI_VIF_CallBackTask_Register")))
        return EXIT_FAILURE;

    if (!(vif_lib->fnUnsetCB = (int(*)(int channel, MI_VIF_CallBackParam_t *config))
        hal_symbol_load("i6_vif", vif_lib->handle, "MI_VIF_CallBackTask_UnRegister")))
        return EXIT_FAILURE;

    return EXIT_SUCCESS;
}

static void i6_vif_unload(i6_vif_impl *vif_lib) {
    if (vif_lib->handle) dlclose(vif_lib->handle);
    vif_lib->handle = NULL;
    memset(vif_lib, 0, sizeof(*vif_lib));
}