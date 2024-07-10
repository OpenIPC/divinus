#pragma once

#include "v3_common.h"
#include "v3_sys.h"

typedef enum {
    V3_RGN_BLK_8,
    V3_RGN_BLK_16,
    V3_RGN_BLK_32,
    V3_RGN_BLK_64,
    V3_RGN_BLK_END
} v3_rgn_blk;

typedef enum {
    V3_RGN_TYPE_OVERLAY,
    V3_RGN_TYPE_COVER,
    V3_RGN_TYPE_COVEREX,
    V3_RGN_TYPE_OVERLAYEX,
    V3_RGN_TYPE_MOSAIC,
    V3_RGN_TYPE_END
} v3_rgn_type;

typedef struct {
    v3_common_pixfmt pixFmt;
    v3_common_dim size;
    void *data;
} v3_rgn_bmp;

typedef struct {
    v3_common_dim invColArea;
    unsigned int lumThresh;
    unsigned int highThanOn;
    int invColOn;
} v3_rgn_inv;

typedef struct {
    v3_common_pnt point;
    unsigned int fgAlpha;
    unsigned int bgAlpha;
    // Accepts values from 0-7
    unsigned int layer;
    int adsQualOn;
    int quality;
    int qualOff;
    v3_rgn_inv invert;
} v3_rgn_ovlc;

typedef struct {
    int solidOn;
    unsigned int lineThick;
    v3_common_pnt point[4];
} v3_rgn_qdr;

typedef struct {
    int quadRangleOn;
    union {
        v3_common_rect rect;
        v3_rgn_qdr quadR;
    };
    unsigned int color;
    unsigned int layer;
    int absOrRatioCoord;
} v3_rgn_covc;

typedef struct {
    v3_common_pnt point;
    unsigned int fgAlpha;
    unsigned int bgAlpha;
    // Accepts values from 0-15
    unsigned int layer;
} v3_rgn_ovlxc;

typedef struct {
    int quadRangleOn;
    union {
        v3_common_rect rect;
        v3_rgn_qdr quadR;
    };
    unsigned int color;
    unsigned int layer;
} v3_rgn_covxc;

typedef struct {
    v3_common_rect rect;
    v3_rgn_blk blkSize;
    // Accepts values from 0-3
    unsigned int layer;
} v3_rgn_mosc;

typedef struct {
    int show;
    v3_rgn_type type;
    union {
        v3_rgn_ovlc overlay;
        v3_rgn_covc cover;
        v3_rgn_covxc coverex;
        v3_rgn_ovlxc overlayex;
        v3_rgn_mosc mosaic;
    };
} v3_rgn_chn;

typedef struct {
    v3_common_pixfmt pixFmt;
    unsigned int bgColor;
    v3_common_dim size;
    unsigned int canvas;
} v3_rgn_ovl;

typedef struct {
    v3_rgn_type type;
    union {
        v3_rgn_ovl overlay;
        v3_rgn_ovl overlayex;
    };
} v3_rgn_cnf;

typedef struct {
    void *handle, *handleGoke;

    int (*fnCreateRegion)(unsigned int handle, v3_rgn_cnf *config);
    int (*fnDestroyRegion)(unsigned int handle);
    int (*fnGetRegionConfig)(unsigned int handle, v3_rgn_cnf *config);
    int (*fnSetRegionConfig)(unsigned int handle, v3_rgn_cnf *config);

    int (*fnAttachChannel)(unsigned int handle, v3_sys_bind *dest, v3_rgn_chn *config);
    int (*fnDetachChannel)(unsigned int handle, v3_sys_bind *dest);
    int (*fnGetChannelConfig)(unsigned int handle, v3_sys_bind *dest, v3_rgn_chn *config);
    int (*fnSetChannelConfig)(unsigned int handle, v3_sys_bind *dest, v3_rgn_chn *config);

    int (*fnSetBitmap)(unsigned int handle, v3_rgn_bmp *bitmap);
} v3_rgn_impl;

static int v3_rgn_load(v3_rgn_impl *rgn_lib) {
    if ( !(rgn_lib->handle = dlopen("libmpi.so", RTLD_LAZY | RTLD_GLOBAL)) &&

        (!(rgn_lib->handleGoke = dlopen("libgk_api.so", RTLD_LAZY | RTLD_GLOBAL)) ||
         !(rgn_lib->handle = dlopen("libhi_mpi.so", RTLD_LAZY | RTLD_GLOBAL))))
        HAL_ERROR("v3_rgn", "Failed to load library!\nError: %s\n", dlerror());

    if (!(rgn_lib->fnCreateRegion = (int(*)(unsigned int handle, v3_rgn_cnf *config))
        hal_symbol_load("v3_rgn", rgn_lib->handle, "HI_MPI_RGN_Create")))
        return EXIT_FAILURE;

    if (!(rgn_lib->fnDestroyRegion = (int(*)(unsigned int handle))
        hal_symbol_load("v3_rgn", rgn_lib->handle, "HI_MPI_RGN_Destroy")))
        return EXIT_FAILURE;

    if (!(rgn_lib->fnGetRegionConfig = (int(*)(unsigned int handle, v3_rgn_cnf *config))
        hal_symbol_load("v3_rgn", rgn_lib->handle, "HI_MPI_RGN_GetAttr")))
        return EXIT_FAILURE;

    if (!(rgn_lib->fnSetRegionConfig = (int(*)(unsigned int handle, v3_rgn_cnf *config))
        hal_symbol_load("v3_rgn", rgn_lib->handle, "HI_MPI_RGN_SetAttr")))
        return EXIT_FAILURE;

    if (!(rgn_lib->fnAttachChannel = (int(*)(unsigned int handle, v3_sys_bind *dest, v3_rgn_chn *config))
        hal_symbol_load("v3_rgn", rgn_lib->handle, "HI_MPI_RGN_AttachToChn")))
        return EXIT_FAILURE;

    if (!(rgn_lib->fnDetachChannel = (int(*)(unsigned int handle, v3_sys_bind *dest))
        hal_symbol_load("v3_rgn", rgn_lib->handle, "HI_MPI_RGN_DetachFromChn")))
        return EXIT_FAILURE;

    if (!(rgn_lib->fnGetChannelConfig = (int(*)(unsigned int handle, v3_sys_bind *dest, v3_rgn_chn *config))
        hal_symbol_load("v3_rgn", rgn_lib->handle, "HI_MPI_RGN_GetDisplayAttr")))
        return EXIT_FAILURE;

    if (!(rgn_lib->fnSetChannelConfig = (int(*)(unsigned int handle, v3_sys_bind *dest, v3_rgn_chn *config))
        hal_symbol_load("v3_rgn", rgn_lib->handle, "HI_MPI_RGN_SetDisplayAttr")))
        return EXIT_FAILURE;

    if (!(rgn_lib->fnSetBitmap = (int(*)(unsigned int handle, v3_rgn_bmp *bitmap))
        hal_symbol_load("v3_rgn", rgn_lib->handle, "HI_MPI_RGN_SetBitMap")))
        return EXIT_FAILURE;

    return EXIT_SUCCESS;
}

static void v3_rgn_unload(v3_rgn_impl *rgn_lib) {
    if (rgn_lib->handle) dlclose(rgn_lib->handle);
    rgn_lib->handle = NULL;
    if (rgn_lib->handleGoke) dlclose(rgn_lib->handleGoke);
    rgn_lib->handleGoke = NULL;
    memset(rgn_lib, 0, sizeof(*rgn_lib));
}