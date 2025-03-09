#pragma once

#include "ak_common.h"

typedef struct {
    void *handle, *handleAkuio, *handleDbg, *handleLog, *handleMem, *handleOsal, *handleThread;

    int   (*fnGetErrorNum)(void);
    char* (*fnGetErrorStr)(int error);
} ak_sys_impl;

static int ak_sys_load(ak_sys_impl *sys_lib) {
    if (!(sys_lib->handleOsal = dlopen("libplat_osal.so", RTLD_LAZY | RTLD_GLOBAL)))
        HAL_ERROR("ak_sys", "Failed to load library!\nError: %s\n", dlerror());

    if (!(sys_lib->handleThread = dlopen("libplat_thread.so", RTLD_LAZY | RTLD_GLOBAL)))
        HAL_ERROR("ak_sys", "Failed to load library!\nError: %s\n", dlerror());

    if (!(sys_lib->handleMem = dlopen("libplat_mem.so", RTLD_LAZY | RTLD_GLOBAL)))
        HAL_ERROR("ak_sys", "Failed to load library!\nError: %s\n", dlerror());

    if (!(sys_lib->handleLog = dlopen("libplat_log.so", RTLD_LAZY | RTLD_GLOBAL)))
        HAL_ERROR("ak_sys", "Failed to load library!\nError: %s\n", dlerror());

    if (!(sys_lib->handleDbg = dlopen("libplat_dbg.so", RTLD_LAZY | RTLD_GLOBAL)))
        HAL_ERROR("ak_sys", "Failed to load library!\nError: %s\n", dlerror());

    if (!(sys_lib->handle = dlopen("libplat_common.so", RTLD_LAZY | RTLD_GLOBAL)))
        HAL_ERROR("ak_sys", "Failed to load library!\nError: %s\n", dlerror());

    if (!(sys_lib->handleAkuio = dlopen("libakuio.so", RTLD_LAZY | RTLD_GLOBAL)))
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
    if (sys_lib->handleAkuio) dlclose(sys_lib->handleAkuio);
    sys_lib->handleAkuio = NULL;
    if (sys_lib->handle) dlclose(sys_lib->handle);
    sys_lib->handle = NULL;
    if (sys_lib->handleDbg) dlclose(sys_lib->handleDbg);
    sys_lib->handleDbg = NULL;
    if (sys_lib->handleLog) dlclose(sys_lib->handleLog);
    sys_lib->handleLog = NULL;
    if (sys_lib->handleMem) dlclose(sys_lib->handleMem);
    sys_lib->handleMem = NULL;
    if (sys_lib->handleThread) dlclose(sys_lib->handleThread);
    sys_lib->handleThread = NULL;
    if (sys_lib->handleOsal) dlclose(sys_lib->handleOsal);
    sys_lib->handleOsal = NULL;
    memset(sys_lib, 0, sizeof(*sys_lib));
}