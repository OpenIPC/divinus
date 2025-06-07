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
    t31_osd_type    type;
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
    if (!(osd_lib->handle = dlopen("libimp.so", RTLD_LAZY | RTLD_GLOBAL)))
        HAL_ERROR("t31_osd", "Failed to load library!\nError: %s\n", dlerror());

    if (!(osd_lib->fnCreateRegion = (int(*)(t31_osd_rgn *config))
        hal_symbol_load("t31_osd", osd_lib->handle, "IMP_OSD_CreateRgn")))
        return EXIT_FAILURE;

    if (!(osd_lib->fnDestroyRegion = (int(*)(int handle))
        hal_symbol_load("t31_osd", osd_lib->handle, "IMP_OSD_DestroyRgn")))
        return EXIT_FAILURE;

    if (!(osd_lib->fnGetRegionConfig = (int(*)(int handle, t31_osd_rgn *config))
        hal_symbol_load("t31_osd", osd_lib->handle, "IMP_OSD_GetRgnAttr")))
        return EXIT_FAILURE;

    if (!(osd_lib->fnRegisterRegion = (int(*)(int handle, int group, t31_osd_grp *config))
        hal_symbol_load("t31_osd", osd_lib->handle, "IMP_OSD_RegisterRgn")))
        return EXIT_FAILURE;

    if (!(osd_lib->fnSetRegionConfig = (int(*)(int handle, t31_osd_rgn *config))
        hal_symbol_load("t31_osd", osd_lib->handle, "IMP_OSD_SetRgnAttr")))
        return EXIT_FAILURE;

    if (!(osd_lib->fnUnregisterRegion = (int(*)(int handle, int group))
        hal_symbol_load("t31_osd", osd_lib->handle, "IMP_OSD_UnRegisterRgn")))
        return EXIT_FAILURE;

    if (!(osd_lib->fnAttachToGroup = (int(*)(t31_sys_bind *source, t31_sys_bind *dest))
        hal_symbol_load("t31_osd", osd_lib->handle, "IMP_OSD_AttachToGroup")))
        return EXIT_FAILURE;

    if (!(osd_lib->fnCreateGroup = (int(*)(int group))
        hal_symbol_load("t31_osd", osd_lib->handle, "IMP_OSD_CreateGroup")))
        return EXIT_FAILURE;

    if (!(osd_lib->fnDestroyGroup = (int(*)(int group))
        hal_symbol_load("t31_osd", osd_lib->handle, "IMP_OSD_DestroyGroup")))
        return EXIT_FAILURE;

    if (!(osd_lib->fnGetGroupConfig = (int(*)(int handle, int group, t31_osd_grp *config))
        hal_symbol_load("t31_osd", osd_lib->handle, "IMP_OSD_GetGrpRgnAttr")))
        return EXIT_FAILURE;

    if (!(osd_lib->fnStartGroup = (int(*)(int group))
        hal_symbol_load("t31_osd", osd_lib->handle, "IMP_OSD_Start")))
        return EXIT_FAILURE;

    if (!(osd_lib->fnStopGroup = (int(*)(int group))
        hal_symbol_load("t31_osd", osd_lib->handle, "IMP_OSD_Stop")))
        return EXIT_FAILURE;

    return EXIT_SUCCESS;
}

static void t31_osd_unload(t31_osd_impl *osd_lib) {
    if (osd_lib->handle) dlclose(osd_lib->handle);
    osd_lib->handle = NULL;
    memset(osd_lib, 0, sizeof(*osd_lib));
}