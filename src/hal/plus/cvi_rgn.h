#pragma once

#include "cvi_common.h"
#include "cvi_sys.h"

typedef enum {
    CVI_RGN_BLK_8,
    CVI_RGN_BLK_16,
    CVI_RGN_BLK_END
} cvi_rgn_blk;

typedef enum {
    CVI_RGN_COMPR_NONE,
    CVI_RGN_COMPR_SW,
    CVI_RGN_COMPR_HW,
    CVI_RGN_COMPR_END
} cvi_rgn_compr;

typedef enum {
    CVI_RGN_TYPE_OVERLAY,
    CVI_RGN_TYPE_COVER,
    CVI_RGN_TYPE_COVEREX,
    CVI_RGN_TYPE_OVERLAYEX,
    CVI_RGN_TYPE_MOSAIC,
    CVI_RGN_TYPE_END
} cvi_rgn_type;

typedef struct {
    cvi_common_pixfmt pixFmt;
    cvi_common_dim size;
    void *data;
} cvi_rgn_bmp;

typedef struct {
    cvi_rgn_compr mode;
    unsigned int estSize;
    unsigned int realSize;
} cvi_rgn_cmpinfo;

typedef struct {
    cvi_common_dim invColArea;
    unsigned int lumThresh;
    unsigned int highThanOn;
    char invColOn;
} cvi_rgn_inv;

typedef struct {
    cvi_common_pnt point;
    // Accepts values from 0-7
    unsigned int layer;
    cvi_rgn_inv invert;
} cvi_rgn_ovlc;

typedef struct {
    char solidOn;
    unsigned int lineThick;
    cvi_common_pnt point[4];
} cvi_rgn_qdr;

typedef struct {
    int quadRangleOn;
    union {
        cvi_common_rect rect;
        cvi_rgn_qdr quadR;
    };
    unsigned int color;
    unsigned int layer;
    int absOrRatioCoord;
} cvi_rgn_covc;

typedef struct {
    int quadRangleOn;
    union {
        cvi_common_rect rect;
        cvi_rgn_qdr quadR;
    };
    unsigned int color;
    unsigned int layer;
} cvi_rgn_covxc;

typedef struct {
    cvi_common_rect rect;
    cvi_rgn_blk blkSize;
    // Accepts values from 0-3
    unsigned int layer;
} cvi_rgn_mosc;

typedef struct {
    char show;
    cvi_rgn_type type;
    union {
        cvi_rgn_ovlc overlay;
        cvi_rgn_covc cover;
        cvi_rgn_covxc coverex;
        cvi_rgn_ovlc overlayex;
        cvi_rgn_mosc mosaic;
    };
} cvi_rgn_chn;

typedef struct {
    cvi_common_pixfmt pixFmt;
    unsigned int bgColor;
    cvi_common_dim size;
    unsigned int canvas;
    cvi_rgn_cmpinfo compInfo;
} cvi_rgn_ovl;

typedef struct {
    cvi_rgn_type type;
    union {
        cvi_rgn_ovl overlay;
        cvi_rgn_ovl overlayex;
    };
} cvi_rgn_cnf;

typedef struct {
    void *handle;

    int (*fnCreateRegion)(unsigned int handle, cvi_rgn_cnf *config);
    int (*fnDestroyRegion)(unsigned int handle);
    int (*fnGetRegionConfig)(unsigned int handle, cvi_rgn_cnf *config);
    int (*fnSetRegionConfig)(unsigned int handle, cvi_rgn_cnf *config);

    int (*fnAttachChannel)(unsigned int handle, cvi_sys_bind *dest, cvi_rgn_chn *config);
    int (*fnDetachChannel)(unsigned int handle, cvi_sys_bind *dest);
    int (*fnGetChannelConfig)(unsigned int handle, cvi_sys_bind *dest, cvi_rgn_chn *config);
    int (*fnSetChannelConfig)(unsigned int handle, cvi_sys_bind *dest, cvi_rgn_chn *config);

    int (*fnSetBitmap)(unsigned int handle, cvi_rgn_bmp *bitmap);
} cvi_rgn_impl;

static int cvi_rgn_load(cvi_rgn_impl *rgn_lib) {
    if (!(rgn_lib->handle = dlopen("libvpu.so", RTLD_LAZY | RTLD_GLOBAL)))
        HAL_ERROR("cvi_rgn", "Failed to load library!\nError: %s\n", dlerror());

    if (!(rgn_lib->fnCreateRegion = (int(*)(unsigned int handle, cvi_rgn_cnf *config))
        hal_symbol_load("cvi_rgn", rgn_lib->handle, "CVI_RGN_Create")))
        return EXIT_FAILURE;

    if (!(rgn_lib->fnDestroyRegion = (int(*)(unsigned int handle))
        hal_symbol_load("cvi_rgn", rgn_lib->handle, "CVI_RGN_Destroy")))
        return EXIT_FAILURE;

    if (!(rgn_lib->fnGetRegionConfig = (int(*)(unsigned int handle, cvi_rgn_cnf *config))
        hal_symbol_load("cvi_rgn", rgn_lib->handle, "CVI_RGN_GetAttr")))
        return EXIT_FAILURE;

    if (!(rgn_lib->fnSetRegionConfig = (int(*)(unsigned int handle, cvi_rgn_cnf *config))
        hal_symbol_load("cvi_rgn", rgn_lib->handle, "CVI_RGN_SetAttr")))
        return EXIT_FAILURE;

    if (!(rgn_lib->fnAttachChannel = (int(*)(unsigned int handle, cvi_sys_bind *dest, cvi_rgn_chn *config))
        hal_symbol_load("cvi_rgn", rgn_lib->handle, "CVI_RGN_AttachToChn")))
        return EXIT_FAILURE;

    if (!(rgn_lib->fnDetachChannel = (int(*)(unsigned int handle, cvi_sys_bind *dest))
        hal_symbol_load("cvi_rgn", rgn_lib->handle, "CVI_RGN_DetachFromChn")))
        return EXIT_FAILURE;

    if (!(rgn_lib->fnGetChannelConfig = (int(*)(unsigned int handle, cvi_sys_bind *dest, cvi_rgn_chn *config))
        hal_symbol_load("cvi_rgn", rgn_lib->handle, "CVI_RGN_GetDisplayAttr")))
        return EXIT_FAILURE;

    if (!(rgn_lib->fnSetChannelConfig = (int(*)(unsigned int handle, cvi_sys_bind *dest, cvi_rgn_chn *config))
        hal_symbol_load("cvi_rgn", rgn_lib->handle, "CVI_RGN_SetDisplayAttr")))
        return EXIT_FAILURE;

    if (!(rgn_lib->fnSetBitmap = (int(*)(unsigned int handle, cvi_rgn_bmp *bitmap))
        hal_symbol_load("cvi_rgn", rgn_lib->handle, "CVI_RGN_SetBitMap")))
        return EXIT_FAILURE;

    return EXIT_SUCCESS;
}

static void cvi_rgn_unload(cvi_rgn_impl *rgn_lib) {
    if (rgn_lib->handle) dlclose(rgn_lib->handle);
    rgn_lib->handle = NULL;
    memset(rgn_lib, 0, sizeof(*rgn_lib));
}