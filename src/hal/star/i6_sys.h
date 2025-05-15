#pragma once

#include "i6_common.h"

#define I6_SYS_API "1.0"

typedef enum {
    I6_SYS_LINK_FRAMEBASE = 0x1,
    I6_SYS_LINK_LOWLATENCY = 0x2,
    I6_SYS_LINK_REALTIME = 0x4,
    I6_SYS_LINK_AUTOSYNC = 0x8,
    I6_SYS_LINK_RING = 0x10
} i6_sys_link;

typedef enum {
    I6_SYS_MOD_IVE,
    I6_SYS_MOD_VDF,
    I6_SYS_MOD_VENC,
    I6_SYS_MOD_RGN,
    I6_SYS_MOD_AI,
    I6_SYS_MOD_AO,
    I6_SYS_MOD_VIF,
    I6_SYS_MOD_VPE,
    I6_SYS_MOD_VDEC,
    I6_SYS_MOD_SYS,
    I6_SYS_MOD_FB,
    I6_SYS_MOD_HDMI,
    I6_SYS_MOD_DIVP,
    I6_SYS_MOD_GFX,
    I6_SYS_MOD_VDISP,
    I6_SYS_MOD_DISP,
    I6_SYS_MOD_OS,
    I6_SYS_MOD_IAE,
    I6_SYS_MOD_MD,
    I6_SYS_MOD_OD,
    I6_SYS_MOD_SHADOW,
    I6_SYS_MOD_WARP,
    I6_SYS_MOD_UAC,
    I6_SYS_MOD_LDC,
    I6_SYS_MOD_SD,
    I6_SYS_MOD_PANEL,
    I6_SYS_MOD_CIPHER,
    I6_SYS_MOD_SNR,
    I6_SYS_MOD_WLAN,
    I6_SYS_MOD_IPU,
    I6_SYS_MOD_MIPITX,
    I6_SYS_MOD_GYRO,
    I6_SYS_MOD_JPD,
    I6_SYS_MOD_ISP,
    I6_SYS_MOD_SCL,
    I6_SYS_MOD_WBC,
    I6_SYS_MOD_DSP,
    I6_SYS_MOD_PCIE,
    I6_SYS_MOD_DUMMY,
    I6_SYS_MOD_NIR,
    I6_SYS_MOD_DPU,
    I6_SYS_MOD_END,
} i6_sys_mod;

typedef struct {
    i6_sys_mod module;
    unsigned int device;
    unsigned int channel;
    unsigned int port;
} i6_sys_bind;

typedef struct {
    unsigned char version[128];
} i6_sys_ver;

typedef struct {
    void *handle, *handleCamOsWrapper;
    
    int (*fnExit)(void);
    int (*fnGetVersion)(i6_sys_ver *version);
    int (*fnInit)(void);

    int (*fnBind)(i6_sys_bind *source, i6_sys_bind *dest,
        unsigned int srcFps, unsigned int dstFps);
    int (*fnBindExt)(i6_sys_bind *source, i6_sys_bind *dest, unsigned int srcFps, 
        unsigned int dstFps, i6_sys_link link, unsigned int linkParam);
    int (*fnSetOutputDepth)(i6_sys_bind *bind, unsigned int usrDepth, unsigned int bufDepth);
    int (*fnUnbind)(i6_sys_bind *source, i6_sys_bind *dest);
} i6_sys_impl;

static int i6_sys_load(i6_sys_impl *sys_lib) {
    sys_lib->handleCamOsWrapper = dlopen("libcam_os_wrapper.so", RTLD_LAZY | RTLD_GLOBAL);

    if (!(sys_lib->handle = dlopen("libmi_sys.so", RTLD_LAZY | RTLD_GLOBAL)))
        HAL_ERROR("i6_sys", "Failed to load library!\nError: %s\n", dlerror());

    if (!(sys_lib->fnExit = (int(*)(void))
        hal_symbol_load("i6_sys", sys_lib->handle, "MI_SYS_Exit")))
        return EXIT_FAILURE;

    if (!(sys_lib->fnGetVersion = (int(*)(i6_sys_ver *version))
        hal_symbol_load("i6_sys", sys_lib->handle, "MI_SYS_GetVersion")))
        return EXIT_FAILURE;

    if (!(sys_lib->fnInit = (int(*)(void))
        hal_symbol_load("i6_sys", sys_lib->handle, "MI_SYS_Init")))
        return EXIT_FAILURE;

    if (!(sys_lib->fnBind = (int(*)(i6_sys_bind *source, i6_sys_bind *dest,
        unsigned int srcFps, unsigned int dstFps))
        hal_symbol_load("i6_sys", sys_lib->handle, "MI_SYS_BindChnPort")))
        return EXIT_FAILURE;

    if (!(sys_lib->fnBindExt = (int(*)(i6_sys_bind *source, i6_sys_bind *dest, unsigned int srcFps,
        unsigned int dstFps, i6_sys_link link, unsigned int linkParam))
        hal_symbol_load("i6_sys", sys_lib->handle, "MI_SYS_BindChnPort2")))
        return EXIT_FAILURE;

    if (!(sys_lib->fnSetOutputDepth = (int(*)(i6_sys_bind *bind, unsigned int usrDepth, unsigned int bufDepth))
        hal_symbol_load("i6_sys", sys_lib->handle, "MI_SYS_SetChnOutputPortDepth")))
        return EXIT_FAILURE;

    if (!(sys_lib->fnUnbind = (int(*)(i6_sys_bind *source, i6_sys_bind *dest))
        hal_symbol_load("i6_sys", sys_lib->handle, "MI_SYS_UnBindChnPort")))
        return EXIT_FAILURE;

    return EXIT_SUCCESS;
}

static void i6_sys_unload(i6_sys_impl *sys_lib) {
    if (sys_lib->handle) dlclose(sys_lib->handle);
    sys_lib->handle = NULL;
    if (sys_lib->handleCamOsWrapper) dlclose(sys_lib->handleCamOsWrapper);
    sys_lib->handleCamOsWrapper = NULL;
    memset(sys_lib, 0, sizeof(*sys_lib));
}