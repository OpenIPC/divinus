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
} v3_rgn_covc;

typedef struct {
    v3_common_pnt point;
    unsigned int fgAlpha;
    unsigned int bgAlpha;
    // Accepts values from 0-15
    unsigned int layer;
} v3_rgn_ovlxc;

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
        v3_rgn_covc coverex;
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
    void *handle;

    int (*fnCreateRegion)(unsigned int handle, v3_rgn_cnf *config);
    int (*fnDestroyRegion)(unsigned int handle);
    int (*fnSetRegionConfig)(unsigned int handle, v3_rgn_cnf *config);

    int (*fnAttachChannel)(unsigned int handle, v3_sys_bind *dest, v3_rgn_chn *config);
    int (*fnDetachChannel)(unsigned int handle, v3_sys_bind *dest);
    int (*fnSetChannelConfig)(unsigned int handle, v3_sys_bind *dest, v3_rgn_chn *config);

    int (*fnSetBitmap)(unsigned int handle, v3_rgn_bmp *bitmap);
} v3_rgn_impl;

static int v3_rgn_load(v3_rgn_impl *rgn_lib) {
    if (!(rgn_lib->handle = dlopen("libmpi.so", RTLD_NOW | RTLD_GLOBAL))) {
        fprintf(stderr, "[v3_rgn] Failed to load library!\nError: %s\n", dlerror());
        return EXIT_FAILURE;
    }

    if (!(rgn_lib->fnCreateRegion = (int(*)(unsigned int handle, v3_rgn_cnf *config))
        dlsym(rgn_lib->handle, "HI_MPI_RGN_Create"))) {
        fprintf(stderr, "[v3_rgn] Failed to acquire symbol HI_MPI_RGN_Create!\n");
        return EXIT_FAILURE;
    }

    if (!(rgn_lib->fnDestroyRegion = (int(*)(unsigned int handle))
        dlsym(rgn_lib->handle, "HI_MPI_RGN_Destroy"))) {
        fprintf(stderr, "[v3_rgn] Failed to acquire symbol HI_MPI_RGN_Destroy!\n");
        return EXIT_FAILURE;
    }

    if (!(rgn_lib->fnSetRegionConfig = (int(*)(unsigned int handle, v3_rgn_cnf *config))
        dlsym(rgn_lib->handle, "HI_MPI_RGN_SetAttr"))) {
        fprintf(stderr, "[v3_rgn] Failed to acquire symbol HI_MPI_RGN_SetAttr!\n");
        return EXIT_FAILURE;
    }

    if (!(rgn_lib->fnAttachChannel = (int(*)(unsigned int handle, v3_sys_bind *dest, v3_rgn_chn *config))
        dlsym(rgn_lib->handle, "HI_MPI_RGN_AttachToChn"))) {
        fprintf(stderr, "[v3_rgn] Failed to acquire symbol HI_MPI_RGN_AttachToChn!\n");
        return EXIT_FAILURE;
    }

    if (!(rgn_lib->fnDetachChannel = (int(*)(unsigned int handle, v3_sys_bind *dest))
        dlsym(rgn_lib->handle, "HI_MPI_RGN_DetachFromChn"))) {
        fprintf(stderr, "[v3_rgn] Failed to acquire symbol HI_MPI_RGN_DetachFromChn!\n");
        return EXIT_FAILURE;
    }

    if (!(rgn_lib->fnSetChannelConfig = (int(*)(unsigned int handle, v3_sys_bind *dest, v3_rgn_chn *config))
        dlsym(rgn_lib->handle, "HI_MPI_RGN_SetDisplayAttr"))) {
        fprintf(stderr, "[v3_rgn] Failed to acquire symbol HI_MPI_RGN_SetDisplayAttr!\n");
        return EXIT_FAILURE;
    }

    if (!(rgn_lib->fnSetBitmap = (int(*)(unsigned int handle, v3_rgn_bmp *bitmap))
        dlsym(rgn_lib->handle, "HI_MPI_RGN_SetBitMap"))) {
        fprintf(stderr, "[v3_rgn] Failed to acquire symbol HI_MPI_RGN_SetBitMap!\n");
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}

static void v3_rgn_unload(v3_rgn_impl *rgn_lib) {
    if (rgn_lib->handle)
        dlclose(rgn_lib->handle = NULL);
    memset(rgn_lib, 0, sizeof(*rgn_lib));
}