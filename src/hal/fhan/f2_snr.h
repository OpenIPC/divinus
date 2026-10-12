#pragma once

#include "f2_isp.h"

typedef struct {
    void *handle;
    char name[32];             /* e.g. "gc4653_mipi", matches the tuning-file prefix */
    f2_common_dim dim;         /* native output size */
    f2_isp_snrops *ops;        /* points at the vendor table inside the library */

    int (*fnCreate)(void);
    int (*fnDestroy)(void);
} f2_snr_impl;

static int f2_snr_load(f2_snr_impl *snr_lib, const char *name) {
    char file[64], sym[80], path[96];
    static const char *dirs[] = { "/usr/lib/sensors/%s", "/lib/sensors/%s", "%s" };

    snprintf(file, sizeof(file), "lib%s.so", name);
    snprintf(sym, sizeof(sym), "%s_sensor_if", name);

    /* The vendor sensor objects are not on the default linker path */
    for (unsigned i = 0; i < sizeof(dirs) / sizeof(dirs[0]) && !snr_lib->handle; i++) {
        snprintf(path, sizeof(path), dirs[i], file);
        snr_lib->handle = dlopen(path, RTLD_NOW | RTLD_GLOBAL);
    }
    if (!snr_lib->handle) {
        HAL_WARNING("f2_snr", "Sensor library %s not found\n", file);
        return EXIT_FAILURE;
    }

    if (!(snr_lib->ops = (f2_isp_snrops*)dlsym(snr_lib->handle, sym))) {
        HAL_WARNING("f2_snr", "Sensor table %s not found in %s\n", sym, file);
        return EXIT_FAILURE;
    }

    if (!(snr_lib->fnCreate = (int(*)(void))
        hal_symbol_load("f2_snr", snr_lib->handle, "Sensor_Create")))
        return EXIT_FAILURE;

    snr_lib->fnDestroy = (int(*)(void))dlsym(snr_lib->handle, "Sensor_Destroy");

    strncpy(snr_lib->name, name, sizeof(snr_lib->name) - 1);
    return EXIT_SUCCESS;
}

static void f2_snr_unload(f2_snr_impl *snr_lib) {
    if (snr_lib->handle) dlclose(snr_lib->handle);
    memset(snr_lib, 0, sizeof(*snr_lib));
}
