#pragma once

#include "f2_common.h"

#define F2_OSD_MAX 8

/* FHAdv_Osd_SetLogo() payload */
typedef struct {
    unsigned int enable;
    unsigned int flags;
    unsigned int width;
    unsigned int height;
    unsigned int alpha;
    unsigned int rectX;
    unsigned int rectY;
    unsigned int rectWidth;
    unsigned int rectHeight;
    unsigned int bitmap;
    unsigned int reserved;
    unsigned int show;
    unsigned int reserved2[4];
} f2_osd_logo;

/* Overlay region bookkeeping: one slot per region divinus may create. */
typedef struct {
    char used;
    hal_rect rect;
    unsigned char alpha;
} f2_osd_rgn;

typedef struct {
    void *handle, *handleCommon;

    int (*fnInit)(void *, unsigned int, unsigned int, unsigned int);
    int (*fnSetLogo)(f2_osd_logo *logo);
    int (*fnAlloc)(void *, void *, const char *, const char *, unsigned int);
} f2_osd_impl;

static int f2_osd_load(f2_osd_impl *osd_lib, void *handleVmm) {
    if (!(osd_lib->handleCommon = dlopen("libadvapi.so", RTLD_NOW | RTLD_GLOBAL)) ||
        !(osd_lib->handle = dlopen("libadvapi_osd.so", RTLD_NOW | RTLD_GLOBAL)))
        HAL_ERROR("f2_osd", "Failed to load library!\nError: %s\n", dlerror());

    if (!(osd_lib->fnInit = (int(*)(void *, unsigned int, unsigned int, unsigned int))
        hal_symbol_load("f2_osd", osd_lib->handle, "FHAdv_Osd_Init")))
        return EXIT_FAILURE;

    if (!(osd_lib->fnSetLogo = (int(*)(f2_osd_logo *))
        hal_symbol_load("f2_osd", osd_lib->handle, "FHAdv_Osd_SetLogo")))
        return EXIT_FAILURE;

    if (!(osd_lib->fnAlloc = (int(*)(void *, void *, const char *, const char *, unsigned int))
        dlsym(handleVmm, "FH_SYS_VmmAlloc")))
        return EXIT_FAILURE;

    return EXIT_SUCCESS;
}

static void f2_osd_unload(f2_osd_impl *osd_lib) {
    if (osd_lib->handle) dlclose(osd_lib->handle);
    if (osd_lib->handleCommon) dlclose(osd_lib->handleCommon);
    memset(osd_lib, 0, sizeof(*osd_lib));
}
