#pragma once

#include "tx_common.h"
#include "tx_sys.h"

typedef enum {
    TX_OSD_COLOR_BLACK = 0xFF000000,
    TX_OSD_COLOR_BLUE  = 0xFF0000FF,
    TX_OSD_COLOR_GREEN = 0xFF00FF00,
    TX_OSD_COLOR_RED   = 0xFFFF0000,
    TX_OSD_COLOR_WHITE = 0xFFFFFFFF
} tx_osd_color;

typedef enum {
    TX_OSD_TYPE_INV,
    TX_OSD_TYPE_LINE,
    TX_OSD_TYPE_RECT,
    TX_OSD_TYPE_BITMAP,
    TX_OSD_TYPE_COVER,
    TX_OSD_TYPE_PIC,
    TX_OSD_TYPE_PIC_RMEM,
    TX_OSD_TYPE_END
} tx_osd_type;

typedef union {
    void *bitmap;
    unsigned int lineParams[2];
    unsigned int coverColor;
    void *picture;
} tx_osd_attr;

typedef struct {
	tx_osd_type	type;
	tx_common_rect rect;
	tx_common_pixfmt pixFmt;
	tx_osd_attr data;
} tx_osd_rgn;

typedef struct {
    int show;
    tx_common_pnt point;
    float scaleX, scaleY;
    int alphaOn;
    int fgAlpha;
    int bgAlpha;
    int layer;
} tx_osd_grp;

typedef struct {
    void *handle;
    
    int (*fnCreateRegion)(tx_osd_rgn *config);
    int (*fnDestroyRegion)(int handle);
    int (*fnGetRegionConfig)(int handle, tx_osd_rgn *config);
    int (*fnRegisterRegion)(int handle, int group, tx_osd_grp *config);
    int (*fnSetRegionConfig)(int handle, tx_osd_rgn *config);
    int (*fnUnregisterRegion)(int handle, int group);

    int (*fnAttachToGroup)(tx_sys_bind *source, tx_sys_bind *dest);
    int (*fnCreateGroup)(int group);
    int (*fnDestroyGroup)(int group);
    int (*fnGetGroupConfig)(int handle, int group, tx_osd_grp *config);
    int (*fnStartGroup)(int group);
    int (*fnStopGroup)(int group);
} tx_osd_impl;

static int tx_osd_load(tx_osd_impl *osd_lib) {
    if (!(osd_lib->handle = dlopen("libimp.so", RTLD_LAZY | RTLD_GLOBAL))) {
        fprintf(stderr, "[tx_osd] Failed to load library!\nError: %s\n", dlerror());
        return EXIT_FAILURE;
    }

    if (!(osd_lib->fnCreateRegion = (int(*)(tx_osd_rgn *config))
        dlsym(osd_lib->handle, "IMP_OSD_CreateRgn"))) {
        fprintf(stderr, "[tx_osd] Failed to acquire symbol IMP_OSD_CreateRgn!\n");
        return EXIT_FAILURE;
    }

    if (!(osd_lib->fnDestroyRegion = (int(*)(int handle))
        dlsym(osd_lib->handle, "IMP_OSD_DestroyRgn"))) {
        fprintf(stderr, "[tx_osd] Failed to acquire symbol IMP_OSD_DestroyRgn!\n");
        return EXIT_FAILURE;
    }

    if (!(osd_lib->fnGetRegionConfig = (int(*)(int handle, tx_osd_rgn *config))
        dlsym(osd_lib->handle, "IMP_OSD_GetRgnAttr"))) {
        fprintf(stderr, "[tx_osd] Failed to acquire symbol IMP_OSD_GetRgnAttr!\n");
        return EXIT_FAILURE;
    }

    if (!(osd_lib->fnRegisterRegion = (int(*)(int handle, int group, tx_osd_grp *config))
        dlsym(osd_lib->handle, "IMP_OSD_RegisterRgn"))) {
        fprintf(stderr, "[tx_osd] Failed to acquire symbol IMP_OSD_RegisterRgn!\n");
        return EXIT_FAILURE;
    }

    if (!(osd_lib->fnSetRegionConfig = (int(*)(int handle, tx_osd_rgn *config))
        dlsym(osd_lib->handle, "IMP_OSD_SetRgnAttr"))) {
        fprintf(stderr, "[tx_osd] Failed to acquire symbol IMP_OSD_SetRgnAttr!\n");
        return EXIT_FAILURE;
    }

    if (!(osd_lib->fnUnregisterRegion = (int(*)(int handle, int group))
        dlsym(osd_lib->handle, "IMP_OSD_UnRegisterRgn"))) {
        fprintf(stderr, "[tx_osd] Failed to acquire symbol IMP_OSD_UnRegisterRgn!\n");
        return EXIT_FAILURE;
    }

    if (!(osd_lib->fnAttachToGroup = (int(*)(tx_sys_bind *source, tx_sys_bind *dest))
        dlsym(osd_lib->handle, "IMP_OSD_AttachToGroup"))) {
        fprintf(stderr, "[tx_osd] Failed to acquire symbol IMP_OSD_AttachToGroup!\n");
        return EXIT_FAILURE;
    }

    if (!(osd_lib->fnCreateGroup = (int(*)(int group))
        dlsym(osd_lib->handle, "IMP_OSD_CreateGroup"))) {
        fprintf(stderr, "[tx_osd] Failed to acquire symbol IMP_OSD_CreateGroup!\n");
        return EXIT_FAILURE;
    }

    if (!(osd_lib->fnDestroyGroup = (int(*)(int group))
        dlsym(osd_lib->handle, "IMP_OSD_DestroyGroup"))) {
        fprintf(stderr, "[tx_osd] Failed to acquire symbol IMP_OSD_DestroyGroup!\n");
        return EXIT_FAILURE;
    }

    if (!(osd_lib->fnGetGroupConfig = (int(*)(int handle, int group, tx_osd_grp *config))
        dlsym(osd_lib->handle, "IMP_OSD_GetGrpRgnAttr"))) {
        fprintf(stderr, "[tx_osd] Failed to acquire symbol IMP_OSD_GetGrpRgnAttr!\n");
        return EXIT_FAILURE;
    }

    if (!(osd_lib->fnStartGroup = (int(*)(int group))
        dlsym(osd_lib->handle, "IMP_OSD_Start"))) {
        fprintf(stderr, "[tx_osd] Failed to acquire symbol IMP_OSD_Start!\n");
        return EXIT_FAILURE;
    }

    if (!(osd_lib->fnStopGroup = (int(*)(int group))
        dlsym(osd_lib->handle, "IMP_OSD_Stop"))) {
        fprintf(stderr, "[tx_osd] Failed to acquire symbol IMP_OSD_Stop!\n");
        return EXIT_FAILURE;
    }


    return EXIT_SUCCESS;
}

static void tx_osd_unload(tx_osd_impl *osd_lib) {
    if (osd_lib->handle) dlclose(osd_lib->handle);
    osd_lib->handle = NULL;
    memset(osd_lib, 0, sizeof(*osd_lib));
}