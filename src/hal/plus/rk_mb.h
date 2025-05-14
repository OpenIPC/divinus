#pragma once

#include "rk_common.h"

typedef struct {
    void *handle;

    char* (*fnGetData)(void *block);

} rk_mb_impl;

static int rk_mb_load(rk_mb_impl *mb_lib) {
    if (!(mb_lib->handle = dlopen("librockit.so", RTLD_LAZY | RTLD_GLOBAL)))
        HAL_ERROR("rk_mb", "Failed to load library!\nError: %s\n", dlerror());

    if (!(mb_lib->fnGetData = (char*(*)(void *block))
        hal_symbol_load("rk_mb", mb_lib->handle, "RK_MPI_MB_Handle2VirAddr")))
        return EXIT_FAILURE;

    return EXIT_SUCCESS;
}

static void rk_mb_unload(rk_mb_impl *mb_lib) {
    if (mb_lib->handle) dlclose(mb_lib->handle);
    mb_lib->handle = NULL;
    memset(mb_lib, 0, sizeof(*mb_lib));
}