#pragma once

#include "rk_common.h"
#include "rk_sys.h"

typedef enum {
    RK_RGN_BLK_8,
    RK_RGN_BLK_16,
    RK_RGN_BLK_32,
    RK_RGN_BLK_64,
    RK_RGN_BLK_END
} rk_rgn_blk;

typedef enum {
    RK_RGN_TYPE_OVERLAY,
    RK_RGN_TYPE_COVER,
    RK_RGN_TYPE_MOSAIC,
    RK_RGN_TYPE_LINE,
    RK_RGN_TYPE_END
} rk_rgn_type;

typedef struct {
    rk_common_pixfmt pixFmt;
    rk_common_dim size;
    void *data;
} rk_rgn_bmp;

typedef struct {
    rk_common_pnt point;
    unsigned int fgAlpha;
    unsigned int bgAlpha;
    // Accepts values from 0-7
    unsigned int layer;
    int qpProtOn;
    int absQualOn;
    int forceIntraOn;
    int quality;
    unsigned int colorLut[2];
    rk_common_dim invColorArea;
    unsigned int lumThresh;
    int lumThreshHighOn;
    int invColorOn;
} rk_rgn_ovlc;

typedef struct {
    rk_common_rect rect;
    unsigned int color;
    unsigned int layer;
    int ratioCoordOn;
} rk_rgn_covc;

typedef struct {
    rk_common_rect rect;
    rk_rgn_blk blkSize;
    // Accepts values from 0-3
    unsigned int layer;
} rk_rgn_mosc;

typedef struct {
    unsigned int thick;
    unsigned int color;
    rk_common_pnt start;
    rk_common_pnt end;
} rk_rgn_linc;

typedef struct {
    int show;
    rk_rgn_type type;
    union {
        rk_rgn_ovlc overlay;
        rk_rgn_covc cover;
        rk_rgn_mosc mosaic;
        rk_rgn_linc line;
    };
} rk_rgn_chn;

typedef struct {
    rk_common_pixfmt pixFmt;
    rk_common_dim size;
    unsigned int canvas;
    unsigned int colorLutNum;
    // Only supported with BGRA8888
    unsigned int colorLut[256];
} rk_rgn_ovl;

typedef struct {
    rk_rgn_type type;
    rk_rgn_ovl overlay;
} rk_rgn_cnf;

typedef struct {
    void *handle;

    int (*fnCreateRegion)(unsigned int handle, rk_rgn_cnf *config);
    int (*fnDestroyRegion)(unsigned int handle);
    int (*fnGetRegionConfig)(unsigned int handle, rk_rgn_cnf *config);
    int (*fnSetRegionConfig)(unsigned int handle, rk_rgn_cnf *config);

    int (*fnAttachChannel)(unsigned int handle, rk_sys_bind *dest, rk_rgn_chn *config);
    int (*fnDetachChannel)(unsigned int handle, rk_sys_bind *dest);
    int (*fnGetChannelConfig)(unsigned int handle, rk_sys_bind *dest, rk_rgn_chn *config);
    int (*fnSetChannelConfig)(unsigned int handle, rk_sys_bind *dest, rk_rgn_chn *config);

    int (*fnSetBitmap)(unsigned int handle, rk_rgn_bmp *bitmap);
} rk_rgn_impl;

static int rk_rgn_load(rk_rgn_impl *rgn_lib) {
    if (!(rgn_lib->handle = dlopen("librockit.so", RTLD_LAZY | RTLD_GLOBAL)))
        HAL_ERROR("rk_rgn", "Failed to load library!\nError: %s\n", dlerror());

    if (!(rgn_lib->fnCreateRegion = (int(*)(unsigned int handle, rk_rgn_cnf *config))
        hal_symbol_load("rk_rgn", rgn_lib->handle, "RK_MPI_RGN_Create")))
        return EXIT_FAILURE;

    if (!(rgn_lib->fnDestroyRegion = (int(*)(unsigned int handle))
        hal_symbol_load("rk_rgn", rgn_lib->handle, "RK_MPI_RGN_Destroy")))
        return EXIT_FAILURE;

    if (!(rgn_lib->fnGetRegionConfig = (int(*)(unsigned int handle, rk_rgn_cnf *config))
        hal_symbol_load("rk_rgn", rgn_lib->handle, "RK_MPI_RGN_GetAttr")))
        return EXIT_FAILURE;

    if (!(rgn_lib->fnSetRegionConfig = (int(*)(unsigned int handle, rk_rgn_cnf *config))
        hal_symbol_load("rk_rgn", rgn_lib->handle, "RK_MPI_RGN_SetAttr")))
        return EXIT_FAILURE;

    if (!(rgn_lib->fnAttachChannel = (int(*)(unsigned int handle, rk_sys_bind *dest, rk_rgn_chn *config))
        hal_symbol_load("rk_rgn", rgn_lib->handle, "RK_MPI_RGN_AttachToChn")))
        return EXIT_FAILURE;

    if (!(rgn_lib->fnDetachChannel = (int(*)(unsigned int handle, rk_sys_bind *dest))
        hal_symbol_load("rk_rgn", rgn_lib->handle, "RK_MPI_RGN_DetachFromChn")))
        return EXIT_FAILURE;

    if (!(rgn_lib->fnGetChannelConfig = (int(*)(unsigned int handle, rk_sys_bind *dest, rk_rgn_chn *config))
        hal_symbol_load("rk_rgn", rgn_lib->handle, "RK_MPI_RGN_GetDisplayAttr")))
        return EXIT_FAILURE;

    if (!(rgn_lib->fnSetChannelConfig = (int(*)(unsigned int handle, rk_sys_bind *dest, rk_rgn_chn *config))
        hal_symbol_load("rk_rgn", rgn_lib->handle, "RK_MPI_RGN_SetDisplayAttr")))
        return EXIT_FAILURE;

    if (!(rgn_lib->fnSetBitmap = (int(*)(unsigned int handle, rk_rgn_bmp *bitmap))
        hal_symbol_load("rk_rgn", rgn_lib->handle, "RK_MPI_RGN_SetBitMap")))
        return EXIT_FAILURE;

    return EXIT_SUCCESS;
}

static void rk_rgn_unload(rk_rgn_impl *rgn_lib) {
    if (rgn_lib->handle) dlclose(rgn_lib->handle);
    rgn_lib->handle = NULL;
    memset(rgn_lib, 0, sizeof(*rgn_lib));
}