#pragma once

#include "ak_common.h"

#define AK_SYS_REG_CHIP   0x08000000
#define AK_SYS_BOOT_REG   0x080080E8
#define AK_SYS_BOOT_MAGIC 0xAA000000

#define AK_SYS_CHIP_3918EV100 0x20120100
#define AK_SYS_CHIP_3918EV200 0x20150200
#define AK_SYS_CHIP_3918EV300 0x20160100
#define AK_SYS_CHIP_3918EV330 0x20160101

typedef struct {
    void *handle, *handleAkuio, *handleDrv, *handleIpcSrv, *handleThread;

    int   (*fnGetErrorNum)(void);
    char* (*fnGetErrorStr)(int error);
} ak_sys_impl;

static int ak_sys_load(ak_sys_impl *sys_lib) {
    if (!(sys_lib->handleThread = dlopen("libplat_thread.so", RTLD_LAZY | RTLD_GLOBAL)))
        HAL_ERROR("ak_sys", "Failed to load library!\nError: %s\n", dlerror());

    if (!(sys_lib->handle = dlopen("libplat_common.so", RTLD_LAZY | RTLD_GLOBAL)))
        HAL_ERROR("ak_sys", "Failed to load library!\nError: %s\n", dlerror());

    if (!(sys_lib->handleAkuio = dlopen("libakuio.so", RTLD_LAZY | RTLD_GLOBAL)))
        HAL_ERROR("ak_sys", "Failed to load library!\nError: %s\n", dlerror());

    if (!(sys_lib->handleDrv = dlopen("libplat_drv.so", RTLD_LAZY | RTLD_GLOBAL)))
        HAL_ERROR("ak_sys", "Failed to load library!\nError: %s\n", dlerror());

    if (!(sys_lib->handleIpcSrv = dlopen("libplat_ipcsrv.so", RTLD_LAZY | RTLD_GLOBAL)))
        HAL_ERROR("ak_sys", "Failed to load library!\nError: %s\n", dlerror());

    if (!(sys_lib->fnGetErrorNum = (int(*)(void))
        hal_symbol_load("ak_sys", sys_lib->handle, "ak_get_error_no")))
        return EXIT_FAILURE;

    if (!(sys_lib->fnGetErrorStr = (char*(*)(int error))
        hal_symbol_load("ak_sys", sys_lib->handle, "ak_get_error_str")))
        return EXIT_FAILURE;

    return EXIT_SUCCESS;
}

static void ak_sys_unload(ak_sys_impl *sys_lib) {
    if (sys_lib->handleIpcSrv) dlclose(sys_lib->handleIpcSrv);
    sys_lib->handleIpcSrv = NULL;
    if (sys_lib->handleDrv) dlclose(sys_lib->handleDrv);
    sys_lib->handleDrv = NULL;
    if (sys_lib->handleAkuio) dlclose(sys_lib->handleAkuio);
    sys_lib->handleAkuio = NULL;
    if (sys_lib->handle) dlclose(sys_lib->handle);
    sys_lib->handle = NULL;
    if (sys_lib->handleThread) dlclose(sys_lib->handleThread);
    sys_lib->handleThread = NULL;
    memset(sys_lib, 0, sizeof(*sys_lib));
}