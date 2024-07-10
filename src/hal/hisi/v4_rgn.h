#pragma once

#include "v4_common.h"
#include "v4_sys.h"

typedef enum {
    V4_RGN_BLK_8,
    V4_RGN_BLK_16,
    V4_RGN_BLK_32,
    V4_RGN_BLK_64,
    V4_RGN_BLK_END
} v4_rgn_blk;

typedef enum {
    V4_RGN_DEST_MAIN,
    V4_RGN_DEST_MPF0,
    V4_RGN_DEST_MPF1,
    V4_RGN_DEST_END
} v4_rgn_dest;

typedef enum {
    V4_RGN_TYPE_OVERLAY,
    V4_RGN_TYPE_COVER,
    V4_RGN_TYPE_COVEREX,
    V4_RGN_TYPE_OVERLAYEX,
    V4_RGN_TYPE_MOSAIC,
    V4_RGN_TYPE_END
} v4_rgn_type;

typedef struct {
    v4_common_pixfmt pixFmt;
    v4_common_dim size;
    void *data;
} v4_rgn_bmp;

typedef struct {
    v4_common_dim invColArea;
    unsigned int lumThresh;
    unsigned int highThanOn;
    int invColOn;
} v4_rgn_inv;

typedef struct {
    v4_common_pnt point;
    unsigned int fgAlpha;
    unsigned int bgAlpha;
    // Accepts values from 0-7
    unsigned int layer;
    int adsQualOn;
    int quality;
    int qualOff;
    v4_rgn_inv invert;
    v4_rgn_dest attachDest;
    unsigned short colorLut[2];
} v4_rgn_ovlc;

typedef struct {
    int solidOn;
    unsigned int lineThick;
    v4_common_pnt point[4];
} v4_rgn_qdr;

typedef struct {
    int quadRangleOn;
    union {
        v4_common_rect rect;
        v4_rgn_qdr quadR;
    };
    unsigned int color;
    unsigned int layer;
    int absOrRatioCoord;
} v4_rgn_covc;

typedef struct {
    v4_common_pnt point;
    unsigned int fgAlpha;
    unsigned int bgAlpha;
    // Accepts values from 0-15
    unsigned int layer;
    unsigned short colorLut[2];
} v4_rgn_ovlxc;

typedef struct {
    int quadRangleOn;
    union {
        v4_common_rect rect;
        v4_rgn_qdr quadR;
    };
    unsigned int color;
    unsigned int layer;
} v4_rgn_covxc;

typedef struct {
    v4_common_rect rect;
    v4_rgn_blk blkSize;
    // Accepts values from 0-3
    unsigned int layer;
} v4_rgn_mosc;

typedef struct {
    int show;
    v4_rgn_type type;
    union {
        v4_rgn_ovlc overlay;
        v4_rgn_covc cover;
        v4_rgn_covxc coverex;
        v4_rgn_ovlxc overlayex;
        v4_rgn_mosc mosaic;
    };
} v4_rgn_chn;

typedef struct {
    v4_common_pixfmt pixFmt;
    unsigned int bgColor;
    v4_common_dim size;
    unsigned int canvas;
} v4_rgn_ovl;

typedef struct {
    v4_rgn_type type;
    union {
        v4_rgn_ovl overlay;
        v4_rgn_ovl overlayex;
    };
} v4_rgn_cnf;

typedef struct {
    void *handle, *handleGoke;

    int (*fnCreateRegion)(unsigned int handle, v4_rgn_cnf *config);
    int (*fnDestroyRegion)(unsigned int handle);
    int (*fnGetRegionConfig)(unsigned int handle, v4_rgn_cnf *config);
    int (*fnSetRegionConfig)(unsigned int handle, v4_rgn_cnf *config);

    int (*fnAttachChannel)(unsigned int handle, v4_sys_bind *dest, v4_rgn_chn *config);
    int (*fnDetachChannel)(unsigned int handle, v4_sys_bind *dest);
    int (*fnGetChannelConfig)(unsigned int handle, v4_sys_bind *dest, v4_rgn_chn *config);
    int (*fnSetChannelConfig)(unsigned int handle, v4_sys_bind *dest, v4_rgn_chn *config);

    int (*fnSetBitmap)(unsigned int handle, v4_rgn_bmp *bitmap);
} v4_rgn_impl;

static int v4_rgn_load(v4_rgn_impl *rgn_lib) {
    if ( !(rgn_lib->handle = dlopen("libmpi.so", RTLD_LAZY | RTLD_GLOBAL)) &&

        (!(rgn_lib->handleGoke = dlopen("libgk_api.so", RTLD_LAZY | RTLD_GLOBAL)) ||
         !(rgn_lib->handle = dlopen("libhi_mpi.so", RTLD_LAZY | RTLD_GLOBAL))))
        HAL_ERROR("v4_rgn", "Failed to load library!\nError: %s\n", dlerror());

    if (!(rgn_lib->fnCreateRegion = (int(*)(unsigned int handle, v4_rgn_cnf *config))
        hal_symbol_load("v4_rgn", rgn_lib->handle, "HI_MPI_RGN_Create")))
        return EXIT_FAILURE;

    if (!(rgn_lib->fnDestroyRegion = (int(*)(unsigned int handle))
        hal_symbol_load("v4_rgn", rgn_lib->handle, "HI_MPI_RGN_Destroy")))
        return EXIT_FAILURE;

    if (!(rgn_lib->fnGetRegionConfig = (int(*)(unsigned int handle, v4_rgn_cnf *config))
        hal_symbol_load("v4_rgn", rgn_lib->handle, "HI_MPI_RGN_GetAttr")))
        return EXIT_FAILURE;

    if (!(rgn_lib->fnSetRegionConfig = (int(*)(unsigned int handle, v4_rgn_cnf *config))
        hal_symbol_load("v4_rgn", rgn_lib->handle, "HI_MPI_RGN_SetAttr")))
        return EXIT_FAILURE;

    if (!(rgn_lib->fnAttachChannel = (int(*)(unsigned int handle, v4_sys_bind *dest, v4_rgn_chn *config))
        hal_symbol_load("v4_rgn", rgn_lib->handle, "HI_MPI_RGN_AttachToChn")))
        return EXIT_FAILURE;

    if (!(rgn_lib->fnDetachChannel = (int(*)(unsigned int handle, v4_sys_bind *dest))
        hal_symbol_load("v4_rgn", rgn_lib->handle, "HI_MPI_RGN_DetachFromChn")))
        return EXIT_FAILURE;

    if (!(rgn_lib->fnGetChannelConfig = (int(*)(unsigned int handle, v4_sys_bind *dest, v4_rgn_chn *config))
        hal_symbol_load("v4_rgn", rgn_lib->handle, "HI_MPI_RGN_GetDisplayAttr")))
        return EXIT_FAILURE;

    if (!(rgn_lib->fnSetChannelConfig = (int(*)(unsigned int handle, v4_sys_bind *dest, v4_rgn_chn *config))
        hal_symbol_load("v4_rgn", rgn_lib->handle, "HI_MPI_RGN_SetDisplayAttr")))
        return EXIT_FAILURE;

    if (!(rgn_lib->fnSetBitmap = (int(*)(unsigned int handle, v4_rgn_bmp *bitmap))
        hal_symbol_load("v4_rgn", rgn_lib->handle, "HI_MPI_RGN_SetBitMap")))
        return EXIT_FAILURE;

    return EXIT_SUCCESS;
}

static void v4_rgn_unload(v4_rgn_impl *rgn_lib) {
    if (rgn_lib->handle) dlclose(rgn_lib->handle);
    rgn_lib->handle = NULL;
    if (rgn_lib->handleGoke) dlclose(rgn_lib->handleGoke);
    rgn_lib->handleGoke = NULL;
    memset(rgn_lib, 0, sizeof(*rgn_lib));
}