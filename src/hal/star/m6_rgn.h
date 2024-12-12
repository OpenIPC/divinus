#pragma once

#include "m6_common.h"
#include "m6_sys.h"

typedef enum {
    M6_RGN_PIXFMT_ARGB1555,
    M6_RGN_PIXFMT_ARGB4444,
    M6_RGN_PIXFMT_I2,
    M6_RGN_PIXFMT_I4,
    M6_RGN_PIXFMT_I8,
    M6_RGN_PIXFMT_RGB565,
    M6_RGN_PIXFMT_ARGB888,
    M6_RGN_PIXFMT_END
} m6_rgn_pixfmt;

typedef enum {
    M6_RGN_TYPE_OSD,
    M6_RGN_TYPE_COVER,
    M6_RGN_TYPE_END
} m6_rgn_type;

typedef struct {
    unsigned int width;
    unsigned int height;
} m6_rgn_size;

typedef struct {
    m6_rgn_pixfmt pixFmt;
    m6_rgn_size size;
    void *data;
} m6_rgn_bmp;

typedef struct {
    m6_rgn_type type;
    m6_rgn_pixfmt pixFmt;
    m6_rgn_size size;
} m6_rgn_cnf;

typedef struct {
    unsigned int layer;
    m6_rgn_size size;
    unsigned int color;
} m6_rgn_cov;

typedef struct {
    int invColOn;
    int lowThanThresh;
    unsigned int lumThresh;
    unsigned short divWidth;
    unsigned short divHeight;
} m6_rgn_inv;

typedef struct {
    unsigned int layer;
    int constAlphaOn;
    union {
        unsigned char bgFgAlpha[2];
        unsigned char constAlpha[2];
    };
    m6_rgn_inv invert;
} m6_rgn_osd;

typedef struct {
    unsigned int x;
    unsigned int y;
} m6_rgn_pnt;

typedef struct {
    int show;
    m6_rgn_pnt point;
    union {
        m6_rgn_cov cover;
        m6_rgn_osd osd;
    };
} m6_rgn_chn;

typedef struct {
    unsigned char alpha;
    unsigned char red;
    unsigned char green;
    unsigned char blue;
} m6_rgn_pale;

typedef struct {
    m6_rgn_pale element[256];
} m6_rgn_pal;

typedef struct {
    void *handle;

    int (*fnDeinit)(unsigned short chip);
    int (*fnInit)(unsigned short chip, m6_rgn_pal *palette);

    int (*fnCreateRegion)(unsigned short chip, unsigned int handle, m6_rgn_cnf *config);
    int (*fnDestroyRegion)(unsigned short chip, unsigned int handle);
    int (*fnGetRegionConfig)(unsigned short chip, unsigned int handle, m6_rgn_cnf *config);

    int (*fnAttachChannel)(unsigned short chip, unsigned int handle, m6_sys_bind *dest, m6_rgn_chn *config);
    int (*fnDetachChannel)(unsigned short chip, unsigned int handle, m6_sys_bind *dest);
    int (*fnGetChannelConfig)(unsigned short chip, unsigned int handle, m6_sys_bind *dest, m6_rgn_chn *config);
    int (*fnSetChannelConfig)(unsigned short chip, unsigned int handle, m6_sys_bind *dest, m6_rgn_chn *config);

    int (*fnSetBitmap)(unsigned short chip, unsigned int handle, m6_rgn_bmp *bitmap);
} m6_rgn_impl;

static int m6_rgn_load(m6_rgn_impl *rgn_lib) {
    if (!(rgn_lib->handle = dlopen("libmi_rgn.so", RTLD_LAZY | RTLD_GLOBAL)))
        HAL_ERROR("m6_rgn", "Failed to load library!\nError: %s\n", dlerror());


    if (!(rgn_lib->fnDeinit = (int(*)(unsigned short chip))
        hal_symbol_load("m6_rgn", rgn_lib->handle, "MI_RGN_DeInit")))
        return EXIT_FAILURE;

    if (!(rgn_lib->fnInit = (int(*)(unsigned short chip, m6_rgn_pal *palette))
        hal_symbol_load("m6_rgn", rgn_lib->handle, "MI_RGN_Init")))
        return EXIT_FAILURE;

    if (!(rgn_lib->fnCreateRegion = (int(*)(unsigned short chip, unsigned int handle, m6_rgn_cnf *config))
        hal_symbol_load("m6_rgn", rgn_lib->handle, "MI_RGN_Create")))
        return EXIT_FAILURE;

    if (!(rgn_lib->fnGetRegionConfig = (int(*)(unsigned short chip, unsigned int handle, m6_rgn_cnf *config))
        hal_symbol_load("m6_rgn", rgn_lib->handle, "MI_RGN_GetAttr")))
        return EXIT_FAILURE;

    if (!(rgn_lib->fnDestroyRegion = (int(*)(unsigned short chip, unsigned int handle))
        hal_symbol_load("m6_rgn", rgn_lib->handle, "MI_RGN_Destroy")))
        return EXIT_FAILURE;

    if (!(rgn_lib->fnAttachChannel = (int(*)(unsigned short chip, unsigned int handle, m6_sys_bind *dest, m6_rgn_chn *config))
        hal_symbol_load("m6_rgn", rgn_lib->handle, "MI_RGN_AttachToChn")))
        return EXIT_FAILURE;

    if (!(rgn_lib->fnDetachChannel = (int(*)(unsigned short chip, unsigned int handle, m6_sys_bind *dest))
        hal_symbol_load("m6_rgn", rgn_lib->handle, "MI_RGN_DetachFromChn")))
        return EXIT_FAILURE;

    if (!(rgn_lib->fnGetChannelConfig = (int(*)(unsigned short chip, unsigned int handle, m6_sys_bind *dest, m6_rgn_chn *config))
        hal_symbol_load("m6_rgn", rgn_lib->handle, "MI_RGN_GetDisplayAttr")))
        return EXIT_FAILURE;

    if (!(rgn_lib->fnSetChannelConfig = (int(*)(unsigned short chip, unsigned int handle, m6_sys_bind *dest, m6_rgn_chn *config))
        hal_symbol_load("m6_rgn", rgn_lib->handle, "MI_RGN_SetDisplayAttr")))
        return EXIT_FAILURE;

    if (!(rgn_lib->fnSetBitmap = (int(*)(unsigned short chip, unsigned int handle, m6_rgn_bmp *bitmap))
        hal_symbol_load("m6_rgn", rgn_lib->handle, "MI_RGN_SetBitMap")))
        return EXIT_FAILURE;

    return EXIT_SUCCESS;
}

static void m6_rgn_unload(m6_rgn_impl *rgn_lib) {
    if (rgn_lib->handle) dlclose(rgn_lib->handle);
    rgn_lib->handle = NULL;
    memset(rgn_lib, 0, sizeof(*rgn_lib));
}