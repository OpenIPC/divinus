#pragma once

#include <dlfcn.h>
#include <stdio.h>

#include "macros.h"

static void inline *hal_symbol_load(const char *module, void *handle, const char *symbol) {
    void *function = dlsym(handle, symbol);
    if (!function) {
        HAL_DANGER(module, "Failed to acquire symbol %s!\n", symbol);
        return (void*)0;
    }
    return function;
}