#pragma once

#include "v1_common.h"
#include "v1_sys.h"

typedef enum {
    V1_RGN_TYPE_OVERLAY,
    V1_RGN_TYPE_COVER,
    V1_RGN_TYPE_COVEREX,
    V1_RGN_TYPE_OVERLAYEX,
    V1_RGN_TYPE_END
} v1_rgn_type;

typedef struct {
    v1_common_pixfmt pixFmt;
    v1_common_dim size;
    void *data;
} v1_rgn_bmp;

typedef struct {
    v1_common_dim invColArea;
    unsigned int lumThresh;
    unsigned int highThanOn;
    int invColOn;
} v1_rgn_inv;

typedef struct {
    v1_common_pnt point;
    unsigned int fgAlpha;
    unsigned int bgAlpha;
    // Accepts values from 0-7
    unsigned int layer;
    int adsQualOn;
    int quality;
    int qualOff;
    v1_rgn_inv invert;
} v1_rgn_ovlc;

typedef struct {
    int solidOn;
    unsigned int lineThick;
    v1_common_pnt point[4];
} v1_rgn_qdr;

typedef struct {
    int quadRangleOn;
    union {
        v1_common_rect rect;
        v1_rgn_qdr quadR;
    };
    unsigned int color;
    unsigned int layer;
} v1_rgn_covc;

typedef struct {
    v1_common_pnt point;
    unsigned int fgAlpha;
    unsigned int bgAlpha;
    // Accepts values from 0-15
    unsigned int layer;
} v1_rgn_ovlxc;

typedef struct {
    int quadRangleOn;
    union {
        v1_common_rect rect;
        v1_rgn_qdr quadR;
    };
    unsigned int color;
    unsigned int layer;
} v1_rgn_covxc;

typedef struct {
    int show;
    v1_rgn_type type;
    union {
        v1_rgn_ovlc overlay;
        v1_rgn_covc cover;
        v1_rgn_covxc coverex;
        v1_rgn_ovlxc overlayex;
    };
} v1_rgn_chn;

typedef struct {
    v1_common_pixfmt pixFmt;
    unsigned int bgColor;
    v1_common_dim size;
    unsigned int canvas;
} v1_rgn_ovl;

typedef struct {
    v1_rgn_type type;
    union {
        v1_rgn_ovl overlay;
        v1_rgn_ovl overlayex;
    };
} v1_rgn_cnf;

typedef struct {
    void *handle;

    int (*fnCreateRegion)(unsigned int handle, v1_rgn_cnf *config);
    int (*fnDestroyRegion)(unsigned int handle);
    int (*fnGetRegionConfig)(unsigned int handle, v1_rgn_cnf *config);
    int (*fnSetRegionConfig)(unsigned int handle, v1_rgn_cnf *config);

    int (*fnAttachChannel)(unsigned int handle, v1_sys_bind *dest, v1_rgn_chn *config);
    int (*fnDetachChannel)(unsigned int handle, v1_sys_bind *dest);
    int (*fnGetChannelConfig)(unsigned int handle, v1_sys_bind *dest, v1_rgn_chn *config);
    int (*fnSetChannelConfig)(unsigned int handle, v1_sys_bind *dest, v1_rgn_chn *config);

    int (*fnSetBitmap)(unsigned int handle, v1_rgn_bmp *bitmap);
} v1_rgn_impl;

static int v1_rgn_load(v1_rgn_impl *rgn_lib) {
    if (!(rgn_lib->handle = dlopen("libmpi.so", RTLD_LAZY | RTLD_GLOBAL)))
        HAL_ERROR("v1_rgn", "Failed to load library!\nError: %s\n", dlerror());

    if (!(rgn_lib->fnCreateRegion = (int(*)(unsigned int handle, v1_rgn_cnf *config))
        hal_symbol_load("v1_rgn", rgn_lib->handle, "HI_MPI_RGN_Create")))
        return EXIT_FAILURE;

    if (!(rgn_lib->fnDestroyRegion = (int(*)(unsigned int handle))
        hal_symbol_load("v1_rgn", rgn_lib->handle, "HI_MPI_RGN_Destroy")))
        return EXIT_FAILURE;

    if (!(rgn_lib->fnGetRegionConfig = (int(*)(unsigned int handle, v1_rgn_cnf *config))
        hal_symbol_load("v1_rgn", rgn_lib->handle, "HI_MPI_RGN_GetAttr")))
        return EXIT_FAILURE;

    if (!(rgn_lib->fnSetRegionConfig = (int(*)(unsigned int handle, v1_rgn_cnf *config))
        hal_symbol_load("v1_rgn", rgn_lib->handle, "HI_MPI_RGN_SetAttr")))
        return EXIT_FAILURE;

    if (!(rgn_lib->fnAttachChannel = (int(*)(unsigned int handle, v1_sys_bind *dest, v1_rgn_chn *config))
        hal_symbol_load("v1_rgn", rgn_lib->handle, "HI_MPI_RGN_AttachToChn")))
        return EXIT_FAILURE;

    if (!(rgn_lib->fnDetachChannel = (int(*)(unsigned int handle, v1_sys_bind *dest))
        hal_symbol_load("v1_rgn", rgn_lib->handle, "HI_MPI_RGN_DetachFrmChn")))
        return EXIT_FAILURE;

    if (!(rgn_lib->fnGetChannelConfig = (int(*)(unsigned int handle, v1_sys_bind *dest, v1_rgn_chn *config))
        hal_symbol_load("v1_rgn", rgn_lib->handle, "HI_MPI_RGN_GetDisplayAttr")))
        return EXIT_FAILURE;

    if (!(rgn_lib->fnSetChannelConfig = (int(*)(unsigned int handle, v1_sys_bind *dest, v1_rgn_chn *config))
        hal_symbol_load("v1_rgn", rgn_lib->handle, "HI_MPI_RGN_SetDisplayAttr")))
        return EXIT_FAILURE;

    if (!(rgn_lib->fnSetBitmap = (int(*)(unsigned int handle, v1_rgn_bmp *bitmap))
        hal_symbol_load("v1_rgn", rgn_lib->handle, "HI_MPI_RGN_SetBitMap")))
        return EXIT_FAILURE;

    return EXIT_SUCCESS;
}

static void v1_rgn_unload(v1_rgn_impl *rgn_lib) {
    if (rgn_lib->handle) dlclose(rgn_lib->handle);
    rgn_lib->handle = NULL;
    memset(rgn_lib, 0, sizeof(*rgn_lib));
}