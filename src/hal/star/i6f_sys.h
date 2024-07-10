#pragma once

#include "i6f_common.h"

#define I6F_SYS_API "1.0"

typedef enum {
    I6F_SYS_LINK_FRAMEBASE = 0x1,
    I6F_SYS_LINK_LOWLATENCY = 0x2,
    I6F_SYS_LINK_REALTIME = 0x4,
    I6F_SYS_LINK_AUTOSYNC = 0x8,
    I6F_SYS_LINK_RING = 0x10
} i6f_sys_link;

typedef enum {
    I6F_SYS_MOD_IVE,
    I6F_SYS_MOD_VDF,
    I6F_SYS_MOD_VENC,
    I6F_SYS_MOD_RGN,
    I6F_SYS_MOD_AI,
    I6F_SYS_MOD_AO,
    I6F_SYS_MOD_VIF,
    I6F_SYS_MOD_VPE,
    I6F_SYS_MOD_VDEC,
    I6F_SYS_MOD_SYS,
    I6F_SYS_MOD_FB,
    I6F_SYS_MOD_HDMI,
    I6F_SYS_MOD_DIVP,
    I6F_SYS_MOD_GFX,
    I6F_SYS_MOD_VDISP,
    I6F_SYS_MOD_DISP,
    I6F_SYS_MOD_OS,
    I6F_SYS_MOD_IAE,
    I6F_SYS_MOD_MD,
    I6F_SYS_MOD_OD,
    I6F_SYS_MOD_SHADOW,
    I6F_SYS_MOD_WARP,
    I6F_SYS_MOD_UAC,
    I6F_SYS_MOD_LDC,
    I6F_SYS_MOD_SD,
    I6F_SYS_MOD_PANEL,
    I6F_SYS_MOD_CIPHER,
    I6F_SYS_MOD_SNR,
    I6F_SYS_MOD_WLAN,
    I6F_SYS_MOD_IPU,
    I6F_SYS_MOD_MIPITX,
    I6F_SYS_MOD_GYRO,
    I6F_SYS_MOD_JPD,
    I6F_SYS_MOD_ISP,
    I6F_SYS_MOD_SCL,
    I6F_SYS_MOD_WBC,
    I6F_SYS_MOD_END,
} i6f_sys_mod;

typedef enum {
    I6F_SYS_POOL_ENCODER_RING,
    I6F_SYS_POOL_CHANNEL,
    I6F_SYS_POOL_DEVICE,
    I6F_SYS_POOL_OUTPUT
} i6f_sys_pooltype;

typedef struct {
    i6f_sys_mod module;
    unsigned int device;
    unsigned int channel;
    unsigned int port;
} i6f_sys_bind;

typedef struct {
    i6f_sys_mod module;
    unsigned int device;
    unsigned int channel;
    unsigned char heapName[32];
    unsigned int heapSize;
} i6f_sys_poolchn;

typedef struct {
    i6f_sys_mod module;
    unsigned int device;
    unsigned int reserved;
    unsigned char heapName[32];
    unsigned int heapSize;
} i6f_sys_pooldev;

typedef struct {
    unsigned int ringSize;
    unsigned char heapName[32];
} i6f_sys_poolenc;

typedef struct {
    i6f_sys_mod module;
    unsigned int device;
    unsigned int channel;
    unsigned int port;
    unsigned char heapName[32];
    unsigned int heapSize;
} i6f_sys_poolout;

typedef struct {
    i6f_sys_pooltype type;
    char create;
    union {
        i6f_sys_poolchn channel;
        i6f_sys_pooldev device;
        i6f_sys_poolenc encode;
        i6f_sys_poolout output;
    } config;
} i6f_sys_pool;

typedef struct {
    unsigned char version[128];
} i6f_sys_ver;

typedef struct {
    void *handle, *handleCamOsWrapper;
    
    int (*fnExit)(unsigned short chip);
    int (*fnGetVersion)(unsigned short chip, i6f_sys_ver *version);
    int (*fnInit)(unsigned short chip);

    int (*fnBind)(unsigned short chip, i6f_sys_bind *source, i6f_sys_bind *dest, i6f_sys_link *link);
    int (*fnBindExt)(unsigned short chip, i6f_sys_bind *source, i6f_sys_bind *dest, 
        unsigned int srcFps, unsigned int dstFps, i6f_sys_link link, unsigned int linkParam);
    int (*fnSetOutputDepth)(unsigned short chip, i6f_sys_bind *bind, unsigned int usrDepth, 
        unsigned int bufDepth);
    int (*fnUnbind)(unsigned short chip, i6f_sys_bind *source, i6f_sys_bind *dest);

    int (*fnConfigPool)(unsigned short chip, i6f_sys_pool *config);
} i6f_sys_impl;

static int i6f_sys_load(i6f_sys_impl *sys_lib) {
    if (!(sys_lib->handleCamOsWrapper = dlopen("libcam_os_wrapper.so", RTLD_LAZY | RTLD_GLOBAL)))
        HAL_ERROR("i6f_sys", "Failed to load dependency library!\nError: %s\n", dlerror());

    if (!(sys_lib->handle = dlopen("libmi_sys.so", RTLD_LAZY | RTLD_GLOBAL)))
        HAL_ERROR("i6f_sys", "Failed to load library!\nError: %s\n", dlerror());

    if (!(sys_lib->fnExit = (int(*)(unsigned short chip))
        hal_symbol_load("i6f_sys", sys_lib->handle, "MI_SYS_Exit")))
        return EXIT_FAILURE;

    if (!(sys_lib->fnGetVersion = (int(*)(unsigned short chip, i6f_sys_ver *version))
        hal_symbol_load("i6f_venc", sys_lib->handle, "MI_SYS_GetVersion")))
        return EXIT_FAILURE;

    if (!(sys_lib->fnInit = (int(*)(unsigned short chip))
        hal_symbol_load("i6f_venc", sys_lib->handle, "MI_SYS_Init")))
        return EXIT_FAILURE;

    if (!(sys_lib->fnBind = (int(*)(unsigned short chip, i6f_sys_bind *source, i6f_sys_bind *dest, i6f_sys_link *link))
        hal_symbol_load("i6f_venc", sys_lib->handle, "MI_SYS_BindChnPort")))
        return EXIT_FAILURE;

    if (!(sys_lib->fnBindExt = (int(*)(unsigned short chip, i6f_sys_bind *source, i6f_sys_bind *dest, 
        unsigned int srcFps, unsigned int dstFps, i6f_sys_link link, unsigned int linkParam))
        hal_symbol_load("i6f_venc", sys_lib->handle, "MI_SYS_BindChnPort2")))
        return EXIT_FAILURE;

    if (!(sys_lib->fnSetOutputDepth = (int(*)(unsigned short chip, i6f_sys_bind *bind,
        unsigned int usrDepth, unsigned int bufDepth))
        hal_symbol_load("i6f_venc", sys_lib->handle, "MI_SYS_SetChnOutputPortDepth")))
        return EXIT_FAILURE;

    if (!(sys_lib->fnUnbind = (int(*)(unsigned short chip, i6f_sys_bind *source, i6f_sys_bind *dest))
        hal_symbol_load("i6f_venc", sys_lib->handle, "MI_SYS_UnBindChnPort")))
        return EXIT_FAILURE;

    if (!(sys_lib->fnConfigPool = (int(*)(unsigned short chip, i6f_sys_pool *config))
        hal_symbol_load("i6f_venc", sys_lib->handle, "MI_SYS_ConfigPrivateMMAPool")))
        return EXIT_FAILURE;

    return EXIT_SUCCESS;
}

static void i6f_sys_unload(i6f_sys_impl *sys_lib) {
    if (sys_lib->handle) dlclose(sys_lib->handle);
    sys_lib->handle = NULL;
    if (sys_lib->handleCamOsWrapper) dlclose(sys_lib->handleCamOsWrapper);
    sys_lib->handleCamOsWrapper = NULL;
    memset(sys_lib, 0, sizeof(*sys_lib));
}