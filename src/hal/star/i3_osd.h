#pragma once

#include "i3_common.h"
#include "i3_sys.h"

typedef struct {
    i3_common_pixfmt pixFmt;
    unsigned short width;
    unsigned short height;
    unsigned char *data;
} i3_osd_bmp;

typedef struct {
    void *handle;

    int (*fnCreateRegion)(void *handle, int channel, i3_osd_bmp *bitmap, i3_common_pnt *point, i3_common_rect *rect);
    int (*fnDestroyRegion)(void *handle);
    int (*fnUpdateRegion)(void *handle, i3_osd_bmp *bitmap, i3_common_pnt *point, i3_common_rect *rect);
} i3_osd_impl;

static int i3_osd_load(i3_osd_impl *osd_lib) {
    if (!(osd_lib->handle = dlopen("libmi.so", RTLD_LAZY | RTLD_GLOBAL)))
        HAL_ERROR("i3_osd", "Failed to load library!\nError: %s\n", dlerror());

    if (!(osd_lib->fnCreateRegion = (int(*)(void *handle, int channel, i3_osd_bmp *bitmap, i3_common_pnt *point, i3_common_rect *rect))
        hal_symbol_load("i3_osd", osd_lib->handle, "MI_OSD_CreateBitmapWidget")))
        return EXIT_FAILURE;

    if (!(osd_lib->fnDestroyRegion = (int(*)(void *handle))
        hal_symbol_load("i3_osd", osd_lib->handle, "MI_OSD_DestroyWidget")))
        return EXIT_FAILURE;

    if (!(osd_lib->fnUpdateRegion = (int(*)(void *handle, i3_osd_bmp *bitmap, i3_common_pnt *point, i3_common_rect *rect))
        hal_symbol_load("i3_osd", osd_lib->handle, "MI_OSD_UpdateBitmapWidget")))
        return EXIT_FAILURE;

    return EXIT_SUCCESS;
}

static void i3_osd_unload(i3_osd_impl *osd_lib) {
    if (osd_lib->handle) dlclose(osd_lib->handle);
    osd_lib->handle = NULL;
    memset(osd_lib, 0, sizeof(*osd_lib));
}