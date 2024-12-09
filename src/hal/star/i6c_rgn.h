#pragma once

#include "i6c_common.h"
#include "i6c_sys.h"

typedef enum {
    I6C_RGN_PIXFMT_ARGB1555,
    I6C_RGN_PIXFMT_ARGB4444,
    I6C_RGN_PIXFMT_I2,
    I6C_RGN_PIXFMT_I4,
    I6C_RGN_PIXFMT_I8,
    I6C_RGN_PIXFMT_RGB565,
    I6C_RGN_PIXFMT_ARGB888,
    I6C_RGN_PIXFMT_END
} i6c_rgn_pixfmt;

typedef enum {
    I6C_RGN_TYPE_OSD,
    I6C_RGN_TYPE_COVER,
    I6C_RGN_TYPE_END
} i6c_rgn_type;

typedef struct {
    unsigned int width;
    unsigned int height;
} i6c_rgn_size;

typedef struct {
    i6c_rgn_pixfmt pixFmt;
    i6c_rgn_size size;
    void *data;
} i6c_rgn_bmp;

typedef struct {
    i6c_rgn_type type;
    i6c_rgn_pixfmt pixFmt;
    i6c_rgn_size size;
} i6c_rgn_cnf;

typedef struct {
    unsigned int layer;
    i6c_rgn_size size;
    unsigned int color;
} i6c_rgn_cov;

typedef struct {
    int invColOn;
    int lowThanThresh;
    unsigned int lumThresh;
    unsigned short divWidth;
    unsigned short divHeight;
} i6c_rgn_inv;

typedef struct {
    unsigned int layer;
    int constAlphaOn;
    union {
        unsigned char bgFgAlpha[2];
        unsigned char constAlpha[2];
    };
    i6c_rgn_inv invert;
} i6c_rgn_osd;

typedef struct {
    unsigned int x;
    unsigned int y;
} i6c_rgn_pnt;

typedef struct {
    int show;
    i6c_rgn_pnt point;
    union {
        i6c_rgn_cov cover;
        i6c_rgn_osd osd;
    };
} i6c_rgn_chn;

typedef struct {
    unsigned char alpha;
    unsigned char red;
    unsigned char green;
    unsigned char blue;
} i6c_rgn_pale;

typedef struct {
    i6c_rgn_pale element[256];
} i6c_rgn_pal;

typedef struct {
    void *handle;

    int (*fnDeinit)(unsigned short chip);
    int (*fnInit)(unsigned short chip, i6c_rgn_pal *palette);

    int (*fnCreateRegion)(unsigned short chip, unsigned int handle, i6c_rgn_cnf *config);
    int (*fnDestroyRegion)(unsigned short chip, unsigned int handle);
    int (*fnGetRegionConfig)(unsigned short chip, unsigned int handle, i6c_rgn_cnf *config);

    int (*fnAttachChannel)(unsigned short chip, unsigned int handle, i6c_sys_bind *dest, i6c_rgn_chn *config);
    int (*fnDetachChannel)(unsigned short chip, unsigned int handle, i6c_sys_bind *dest);
    int (*fnGetChannelConfig)(unsigned short chip, unsigned int handle, i6c_sys_bind *dest, i6c_rgn_chn *config);
    int (*fnSetChannelConfig)(unsigned short chip, unsigned int handle, i6c_sys_bind *dest, i6c_rgn_chn *config);

    int (*fnSetBitmap)(unsigned short chip, unsigned int handle, i6c_rgn_bmp *bitmap);
} i6c_rgn_impl;

static int i6c_rgn_load(i6c_rgn_impl *rgn_lib) {
    if (!(rgn_lib->handle = dlopen("libmi_rgn.so", RTLD_LAZY | RTLD_GLOBAL)))
        HAL_ERROR("i6c_rgn", "Failed to load library!\nError: %s\n", dlerror());

    if (!(rgn_lib->fnDeinit = (int(*)(unsigned short chip))
        hal_symbol_load("i6c_rgn", rgn_lib->handle, "MI_RGN_DeInit")))
        return EXIT_FAILURE;

    if (!(rgn_lib->fnInit = (int(*)(unsigned short chip, i6c_rgn_pal *palette))
        hal_symbol_load("i6c_rgn", rgn_lib->handle, "MI_RGN_Init")))
        return EXIT_FAILURE;

    if (!(rgn_lib->fnCreateRegion = (int(*)(unsigned short chip, unsigned int handle, i6c_rgn_cnf *config))
        hal_symbol_load("i6c_rgn", rgn_lib->handle, "MI_RGN_Create")))
        return EXIT_FAILURE;

    if (!(rgn_lib->fnDestroyRegion = (int(*)(unsigned short chip, unsigned int handle))
        hal_symbol_load("i6c_rgn", rgn_lib->handle, "MI_RGN_Destroy")))
        return EXIT_FAILURE;

    if (!(rgn_lib->fnGetRegionConfig = (int(*)(unsigned short chip, unsigned int handle, i6c_rgn_cnf *config))
        hal_symbol_load("i6c_rgn", rgn_lib->handle, "MI_RGN_GetAttr")))
        return EXIT_FAILURE;

    if (!(rgn_lib->fnAttachChannel = (int(*)(unsigned short chip, unsigned int handle, i6c_sys_bind *dest, i6c_rgn_chn *config))
        hal_symbol_load("i6c_rgn", rgn_lib->handle, "MI_RGN_AttachToChn")))
        return EXIT_FAILURE;

    if (!(rgn_lib->fnDetachChannel = (int(*)(unsigned short chip, unsigned int handle, i6c_sys_bind *dest))
        hal_symbol_load("i6c_rgn", rgn_lib->handle, "MI_RGN_DetachFromChn")))
        return EXIT_FAILURE;

    if (!(rgn_lib->fnGetChannelConfig = (int(*)(unsigned short chip, unsigned int handle, i6c_sys_bind *dest, i6c_rgn_chn *config))
        hal_symbol_load("i6c_rgn", rgn_lib->handle, "MI_RGN_GetDisplayAttr")))
        return EXIT_FAILURE;

    if (!(rgn_lib->fnSetChannelConfig = (int(*)(unsigned short chip, unsigned int handle, i6c_sys_bind *dest, i6c_rgn_chn *config))
        hal_symbol_load("i6c_rgn", rgn_lib->handle, "MI_RGN_SetDisplayAttr")))
        return EXIT_FAILURE;

    if (!(rgn_lib->fnSetBitmap = (int(*)(unsigned short chip, unsigned int handle, i6c_rgn_bmp *bitmap))
        hal_symbol_load("i6c_rgn", rgn_lib->handle, "MI_RGN_SetBitMap")))
        return EXIT_FAILURE;

    return EXIT_SUCCESS;
}

static void i6c_rgn_unload(i6c_rgn_impl *rgn_lib) {
    if (rgn_lib->handle) dlclose(rgn_lib->handle);
    rgn_lib->handle = NULL;
    memset(rgn_lib, 0, sizeof(*rgn_lib));
}