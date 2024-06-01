#pragma once

#include "t31_common.h"
#include "t31_sys.h"

typedef enum {
    T31_OSD_COLOR_BLACK = 0xFF000000,
    T31_OSD_COLOR_BLUE  = 0xFF0000FF,
    T31_OSD_COLOR_GREEN = 0xFF00FF00,
    T31_OSD_COLOR_RED   = 0xFFFF0000,
    T31_OSD_COLOR_WHITE = 0xFFFFFFFF
} t31_osd_color;

typedef enum {
    T31_OSD_TYPE_INV,
    T31_OSD_TYPE_LINE,
    T31_OSD_TYPE_RECT,
    T31_OSD_TYPE_BITMAP,
    T31_OSD_TYPE_COVER,
    T31_OSD_TYPE_PIC,
    T31_OSD_TYPE_PIC_RMEM,
    T31_OSD_TYPE_END
} t31_osd_type;

typedef union {
    void *bitmap;
    unsigned int lineParams[2];
    unsigned int coverColor;
    void *picture;
} t31_osd_attr;

typedef struct {
	t31_osd_type	type;
	t31_common_rect rect;
	t31_common_pixfmt pixFmt;
	t31_osd_attr data;
} t31_osd_rgn;

typedef struct {
    int show;
    t31_common_pnt point;
    float scaleX, scaleY;
    int alphaOn;
    int fgAlpha;
    int bgAlpha;
    int layer;
} t31_osd_grp;

typedef struct {
    void *handle;
    
    int (*fnCreateRegion)(t31_osd_rgn *config);
    int (*fnDestroyRegion)(int handle);
    int (*fnGetRegionConfig)(int handle, t31_osd_rgn *config);
    int (*fnRegisterRegion)(int handle, int group, t31_osd_grp *config);
    int (*fnSetRegionConfig)(int handle, t31_osd_rgn *config);
    int (*fnUnregisterRegion)(int handle, int group);

    int (*fnAttachToGroup)(t31_sys_bind *source, t31_sys_bind *dest);
    int (*fnCreateGroup)(int group);
    int (*fnDestroyGroup)(int group);
    int (*fnGetGroupConfig)(int handle, int group, t31_osd_grp *config);
    int (*fnStartGroup)(int group);
    int (*fnStopGroup)(int group);
} t31_osd_impl;

static int t31_osd_load(t31_osd_impl *osd_lib) {
    if (!(osd_lib->handle = dlopen("libimp.so", RTLD_LAZY | RTLD_GLOBAL))) {
        fprintf(stderr, "[t31_osd] Failed to load library!\nError: %s\n", dlerror());
        return EXIT_FAILURE;
    }

    if (!(osd_lib->fnCreateRegion = (int(*)(t31_osd_rgn *config))
        dlsym(osd_lib->handle, "IMP_OSD_CreateRgn"))) {
        fprintf(stderr, "[t31_osd] Failed to acquire symbol IMP_OSD_CreateRgn!\n");
        return EXIT_FAILURE;
    }

    if (!(osd_lib->fnDestroyRegion = (int(*)(int handle))
        dlsym(osd_lib->handle, "IMP_OSD_DestroyRgn"))) {
        fprintf(stderr, "[t31_osd] Failed to acquire symbol IMP_OSD_DestroyRgn!\n");
        return EXIT_FAILURE;
    }

    if (!(osd_lib->fnGetRegionConfig = (int(*)(int handle, t31_osd_rgn *config))
        dlsym(osd_lib->handle, "IMP_OSD_GetRgnAttr"))) {
        fprintf(stderr, "[t31_osd] Failed to acquire symbol IMP_OSD_GetRgnAttr!\n");
        return EXIT_FAILURE;
    }

    if (!(osd_lib->fnRegisterRegion = (int(*)(int handle, int group, t31_osd_grp *config))
        dlsym(osd_lib->handle, "IMP_OSD_RegisterRgn"))) {
        fprintf(stderr, "[t31_osd] Failed to acquire symbol IMP_OSD_RegisterRgn!\n");
        return EXIT_FAILURE;
    }

    if (!(osd_lib->fnSetRegionConfig = (int(*)(int handle, t31_osd_rgn *config))
        dlsym(osd_lib->handle, "IMP_OSD_SetRgnAttr"))) {
        fprintf(stderr, "[t31_osd] Failed to acquire symbol IMP_OSD_SetRgnAttr!\n");
        return EXIT_FAILURE;
    }

    if (!(osd_lib->fnUnregisterRegion = (int(*)(int handle, int group))
        dlsym(osd_lib->handle, "IMP_OSD_UnRegisterRgn"))) {
        fprintf(stderr, "[t31_osd] Failed to acquire symbol IMP_OSD_UnRegisterRgn!\n");
        return EXIT_FAILURE;
    }

    if (!(osd_lib->fnAttachToGroup = (int(*)(t31_sys_bind *source, t31_sys_bind *dest))
        dlsym(osd_lib->handle, "IMP_OSD_AttachToGroup"))) {
        fprintf(stderr, "[t31_osd] Failed to acquire symbol IMP_OSD_AttachToGroup!\n");
        return EXIT_FAILURE;
    }

    if (!(osd_lib->fnCreateGroup = (int(*)(int group))
        dlsym(osd_lib->handle, "IMP_OSD_CreateGroup"))) {
        fprintf(stderr, "[t31_osd] Failed to acquire symbol IMP_OSD_CreateGroup!\n");
        return EXIT_FAILURE;
    }

    if (!(osd_lib->fnDestroyGroup = (int(*)(int group))
        dlsym(osd_lib->handle, "IMP_OSD_DestroyGroup"))) {
        fprintf(stderr, "[t31_osd] Failed to acquire symbol IMP_OSD_DestroyGroup!\n");
        return EXIT_FAILURE;
    }

    if (!(osd_lib->fnGetGroupConfig = (int(*)(int handle, int group, t31_osd_grp *config))
        dlsym(osd_lib->handle, "IMP_OSD_GetGrpRgnAttr"))) {
        fprintf(stderr, "[t31_osd] Failed to acquire symbol IMP_OSD_GetGrpRgnAttr!\n");
        return EXIT_FAILURE;
    }

    if (!(osd_lib->fnStartGroup = (int(*)(int group))
        dlsym(osd_lib->handle, "IMP_OSD_Start"))) {
        fprintf(stderr, "[t31_osd] Failed to acquire symbol IMP_OSD_Start!\n");
        return EXIT_FAILURE;
    }

    if (!(osd_lib->fnStopGroup = (int(*)(int group))
        dlsym(osd_lib->handle, "IMP_OSD_Stop"))) {
        fprintf(stderr, "[t31_osd] Failed to acquire symbol IMP_OSD_Stop!\n");
        return EXIT_FAILURE;
    }


    return EXIT_SUCCESS;
}

static void t31_osd_unload(t31_osd_impl *osd_lib) {
    if (osd_lib->handle) dlclose(osd_lib->handle);
    osd_lib->handle = NULL;
    memset(osd_lib, 0, sizeof(*osd_lib));
}