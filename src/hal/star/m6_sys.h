#pragma once

#include "m6_common.h"

#define M6_SYS_API "1.0"

typedef enum {
    M6_SYS_LINK_FRAMEBASE = 0x1,
    M6_SYS_LINK_LOWLATENCY = 0x2,
    M6_SYS_LINK_REALTIME = 0x4,
    M6_SYS_LINK_AUTOSYNC = 0x8,
    M6_SYS_LINK_RING = 0x10
} m6_sys_link;

typedef enum {
    M6_SYS_MOD_IVE,
    M6_SYS_MOD_VDF,
    M6_SYS_MOD_VENC,
    M6_SYS_MOD_RGN,
    M6_SYS_MOD_AI,
    M6_SYS_MOD_AO,
    M6_SYS_MOD_VIF,
    M6_SYS_MOD_VPE,
    M6_SYS_MOD_VDEC,
    M6_SYS_MOD_SYS,
    M6_SYS_MOD_FB,
    M6_SYS_MOD_HDMI,
    M6_SYS_MOD_DIVP,
    M6_SYS_MOD_GFX,
    M6_SYS_MOD_VDISP,
    M6_SYS_MOD_DISP,
    M6_SYS_MOD_OS,
    M6_SYS_MOD_IAE,
    M6_SYS_MOD_MD,
    M6_SYS_MOD_OD,
    M6_SYS_MOD_SHADOW,
    M6_SYS_MOD_WARP,
    M6_SYS_MOD_UAC,
    M6_SYS_MOD_LDC,
    M6_SYS_MOD_SD,
    M6_SYS_MOD_PANEL,
    M6_SYS_MOD_CIPHER,
    M6_SYS_MOD_SNR,
    M6_SYS_MOD_WLAN,
    M6_SYS_MOD_IPU,
    M6_SYS_MOD_MIPITX,
    M6_SYS_MOD_GYRO,
    M6_SYS_MOD_JPD,
    M6_SYS_MOD_ISP,
    M6_SYS_MOD_SCL,
    M6_SYS_MOD_WBC,
    M6_SYS_MOD_END,
} m6_sys_mod;

typedef enum {
    M6_SYS_POOL_ENCODER_RING,
    M6_SYS_POOL_CHANNEL,
    M6_SYS_POOL_DEVICE,
    M6_SYS_POOL_OUTPUT
} m6_sys_pooltype;

typedef struct {
    m6_sys_mod module;
    unsigned int device;
    unsigned int channel;
    unsigned int port;
} m6_sys_bind;

typedef struct {
    m6_sys_mod module;
    unsigned int device;
    unsigned int channel;
    unsigned char heapName[32];
    unsigned int heapSize;
} m6_sys_poolchn;

typedef struct {
    m6_sys_mod module;
    unsigned int device;
    unsigned int reserved;
    unsigned char heapName[32];
    unsigned int heapSize;
} m6_sys_pooldev;

typedef struct {
    unsigned int ringSize;
    unsigned char heapName[32];
} m6_sys_poolenc;

typedef struct {
    m6_sys_mod module;
    unsigned int device;
    unsigned int channel;
    unsigned int port;
    unsigned char heapName[32];
    unsigned int heapSize;
} m6_sys_poolout;

typedef struct {
    m6_sys_pooltype type;
    char create;
    union {
        m6_sys_poolchn channel;
        m6_sys_pooldev device;
        m6_sys_poolenc encode;
        m6_sys_poolout output;
    } config;
} m6_sys_pool;

typedef struct {
    unsigned char version[128];
} m6_sys_ver;

typedef struct {
    void *handle, *handleCamOsWrapper;
    
    int (*fnExit)(unsigned short chip);
    int (*fnGetVersion)(unsigned short chip, m6_sys_ver *version);
    int (*fnInit)(unsigned short chip);

    int (*fnBind)(unsigned short chip, m6_sys_bind *source, m6_sys_bind *dest, m6_sys_link *link);
    int (*fnBindExt)(unsigned short chip, m6_sys_bind *source, m6_sys_bind *dest, 
        unsigned int srcFps, unsigned int dstFps, m6_sys_link link, unsigned int linkParam);
    int (*fnSetOutputDepth)(unsigned short chip, m6_sys_bind *bind, unsigned int usrDepth, 
        unsigned int bufDepth);
    int (*fnUnbind)(unsigned short chip, m6_sys_bind *source, m6_sys_bind *dest);

    int (*fnConfigPool)(unsigned short chip, m6_sys_pool *config);
} m6_sys_impl;

static int m6_sys_load(m6_sys_impl *sys_lib) {
    if (!(sys_lib->handleCamOsWrapper = dlopen("libcam_os_wrapper.so", RTLD_LAZY | RTLD_GLOBAL)))
        HAL_ERROR("m6_sys", "Failed to load dependency library!\nError: %s\n", dlerror());

    if (!(sys_lib->handle = dlopen("libmi_sys.so", RTLD_LAZY | RTLD_GLOBAL)))
        HAL_ERROR("m6_sys", "Failed to load library!\nError: %s\n", dlerror());

    if (!(sys_lib->fnExit = (int(*)(unsigned short chip))
        hal_symbol_load("m6_sys", sys_lib->handle, "MI_SYS_Exit")))
        return EXIT_FAILURE;

    if (!(sys_lib->fnGetVersion = (int(*)(unsigned short chip, m6_sys_ver *version))
        hal_symbol_load("m6_venc", sys_lib->handle, "MI_SYS_GetVersion")))
        return EXIT_FAILURE;

    if (!(sys_lib->fnInit = (int(*)(unsigned short chip))
        hal_symbol_load("m6_venc", sys_lib->handle, "MI_SYS_Init")))
        return EXIT_FAILURE;

    if (!(sys_lib->fnBind = (int(*)(unsigned short chip, m6_sys_bind *source, m6_sys_bind *dest, m6_sys_link *link))
        hal_symbol_load("m6_venc", sys_lib->handle, "MI_SYS_BindChnPort")))
        return EXIT_FAILURE;

    if (!(sys_lib->fnBindExt = (int(*)(unsigned short chip, m6_sys_bind *source, m6_sys_bind *dest, 
        unsigned int srcFps, unsigned int dstFps, m6_sys_link link, unsigned int linkParam))
        hal_symbol_load("m6_venc", sys_lib->handle, "MI_SYS_BindChnPort2")))
        return EXIT_FAILURE;

    if (!(sys_lib->fnSetOutputDepth = (int(*)(unsigned short chip, m6_sys_bind *bind,
        unsigned int usrDepth, unsigned int bufDepth))
        hal_symbol_load("m6_venc", sys_lib->handle, "MI_SYS_SetChnOutputPortDepth")))
        return EXIT_FAILURE;

    if (!(sys_lib->fnUnbind = (int(*)(unsigned short chip, m6_sys_bind *source, m6_sys_bind *dest))
        hal_symbol_load("m6_venc", sys_lib->handle, "MI_SYS_UnBindChnPort")))
        return EXIT_FAILURE;

    if (!(sys_lib->fnConfigPool = (int(*)(unsigned short chip, m6_sys_pool *config))
        hal_symbol_load("m6_venc", sys_lib->handle, "MI_SYS_ConfigPrivateMMAPool")))
        return EXIT_FAILURE;

    return EXIT_SUCCESS;
}

static void m6_sys_unload(m6_sys_impl *sys_lib) {
    if (sys_lib->handle) dlclose(sys_lib->handle);
    sys_lib->handle = NULL;
    if (sys_lib->handleCamOsWrapper) dlclose(sys_lib->handleCamOsWrapper);
    sys_lib->handleCamOsWrapper = NULL;
    memset(sys_lib, 0, sizeof(*sys_lib));
}