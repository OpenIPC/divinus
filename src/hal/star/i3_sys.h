#pragma once

#include "i3_common.h"

#define I3_SYS_API "1.0"

typedef enum {
    I3_SYS_MOD_AI,
    I3_SYS_MOD_AO,
    I3_SYS_MOD_AENC,
    I3_SYS_MOD_ADEC,
    I3_SYS_MOD_VI,
    I3_SYS_MOD_VENC,
    I3_SYS_MOD_END,
} i3_sys_mod;

typedef struct {
    i3_sys_mod module;
    unsigned int device;
    unsigned int channel;
} i3_sys_bind;

typedef struct {
    char version[64];
} i3_sys_ver;

typedef struct {
    void *handle;
    
    int (*fnExit)(void);
    int (*fnGetVersion)(i3_sys_ver *version);
    int (*fnInit)(void);
    int (*fnSetAlignment)(unsigned int *width);

    int (*fnBind)(i3_sys_bind *source, i3_sys_bind *dest);
    int (*fnUnbind)(i3_sys_bind *source, i3_sys_bind *dest);
} i3_sys_impl;

static int i3_sys_load(i3_sys_impl *sys_lib) {
    if (!(sys_lib->handle = dlopen("libmi.so", RTLD_LAZY | RTLD_GLOBAL)))
        HAL_ERROR("i3_sys", "Failed to load library!\nError: %s\n", dlerror());

    if (!(sys_lib->fnExit = (int(*)(void))
        hal_symbol_load("i3_sys", sys_lib->handle, "MI_SYS_Exit")))
        return EXIT_FAILURE;

    if (!(sys_lib->fnGetVersion = (int(*)(i3_sys_ver *version))
        hal_symbol_load("i3_sys", sys_lib->handle, "MI_SYS_GetVersion")))
        return EXIT_FAILURE;

    if (!(sys_lib->fnInit = (int(*)(void))
        hal_symbol_load("i3_sys", sys_lib->handle, "MI_SYS_Init")))
        return EXIT_FAILURE;

    if (!(sys_lib->fnSetAlignment = (int(*)(unsigned int *width))
        hal_symbol_load("i3_sys", sys_lib->handle, "MI_SYS_SetConf")))
        return EXIT_FAILURE;

    if (!(sys_lib->fnBind = (int(*)(i3_sys_bind *source, i3_sys_bind *dest))
        hal_symbol_load("i3_sys", sys_lib->handle, "MI_SYS_Bind")))
        return EXIT_FAILURE;

    if (!(sys_lib->fnUnbind = (int(*)(i3_sys_bind *source, i3_sys_bind *dest))
        hal_symbol_load("i3_sys", sys_lib->handle, "MI_SYS_UnBind")))
        return EXIT_FAILURE;

    return EXIT_SUCCESS;
}

static void i3_sys_unload(i3_sys_impl *sys_lib) {
    if (sys_lib->handle) dlclose(sys_lib->handle);
    sys_lib->handle = NULL;
    memset(sys_lib, 0, sizeof(*sys_lib));
}