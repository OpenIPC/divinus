#pragma once

#include "f2_common.h"

typedef struct {
    unsigned short srcFps;   /* input frames considered */
    unsigned short dstFps;   /* output frames kept */
} f2_vpss_framectrl;

/* FH_VPSS_SetGraph() payload */
typedef struct {
    unsigned int enable;
    unsigned int phyAddr;
    unsigned int field08;
    unsigned int width;
    unsigned int height;
    unsigned int stride;
    unsigned int alpha;
    unsigned int field1c;
    unsigned int field20;
    unsigned int field24;
    unsigned int field28;
    unsigned int reserved[13];
} f2_vpss_graph;

typedef struct {
    void *handle;

    int (*fnDisableChannel)(unsigned int channel);
    int (*fnDisable)(unsigned int group);
    int (*fnEnable)(unsigned int group);
    int (*fnFreezeVideo)(void);
    int (*fnGetChannelCapability)(unsigned int channel, void *cap);
    int (*fnGetChannelConfig)(unsigned int channel, f2_common_dim *dim);
    int (*fnGetFrameControl)(unsigned int channel, f2_vpss_framectrl *ctrl);
    int (*fnEnableChannel)(unsigned int channel);
    int (*fnSetChannelConfig)(unsigned int channel, f2_common_dim *dim);
    int (*fnSetFrameControl)(unsigned int channel, f2_vpss_framectrl *ctrl);
    int (*fnInitChannelMemory)(unsigned int channel, unsigned int alignment, unsigned int size);
    int (*fnSetInputConfig)(f2_common_dim *dim);
    int (*fnSetOutputMode)(unsigned int channel, unsigned int mode);
    int (*fnSetGraph)(f2_vpss_graph *graph);
    int (*fnUnfreezeVideo)(void);
} f2_vpss_impl;

static int f2_vpss_load(f2_vpss_impl *vpss_lib) {
    if (!(vpss_lib->handle = dlopen("libdsp.so", RTLD_NOW | RTLD_GLOBAL)))
        HAL_ERROR("f2_vpss", "Failed to load library!\nError: %s\n", dlerror());

    if (!(vpss_lib->fnDisableChannel = (int(*)(unsigned int))
        hal_symbol_load("f2_vpss", vpss_lib->handle, "FH_VPSS_CloseChn")))
        return EXIT_FAILURE;

    if (!(vpss_lib->fnDisable = (int(*)(unsigned int))
        hal_symbol_load("f2_vpss", vpss_lib->handle, "FH_VPSS_Disable")))
        return EXIT_FAILURE;

    if (!(vpss_lib->fnEnable = (int(*)(unsigned int))
        hal_symbol_load("f2_vpss", vpss_lib->handle, "FH_VPSS_Enable")))
        return EXIT_FAILURE;

    if (!(vpss_lib->fnFreezeVideo = (int(*)(void))
        hal_symbol_load("f2_vpss", vpss_lib->handle, "FH_VPSS_FreezeVideo")))
        return EXIT_FAILURE;

    vpss_lib->fnGetChannelCapability = (int(*)(unsigned int, void*))
        dlsym(vpss_lib->handle, "FH_VPSS_GetChnCapality");

    if (!(vpss_lib->fnGetChannelConfig = (int(*)(unsigned int, f2_common_dim *))
        hal_symbol_load("f2_vpss", vpss_lib->handle, "FH_VPSS_GetChnAttr")))
        return EXIT_FAILURE;

    if (!(vpss_lib->fnGetFrameControl = (int(*)(unsigned int, f2_vpss_framectrl *))
        hal_symbol_load("f2_vpss", vpss_lib->handle, "FH_VPSS_GetFramectrl")))
        return EXIT_FAILURE;

    if (!(vpss_lib->fnEnableChannel = (int(*)(unsigned int))
        hal_symbol_load("f2_vpss", vpss_lib->handle, "FH_VPSS_OpenChn")))
        return EXIT_FAILURE;

    if (!(vpss_lib->fnSetChannelConfig = (int(*)(unsigned int, f2_common_dim *))
        hal_symbol_load("f2_vpss", vpss_lib->handle, "FH_VPSS_SetChnAttr")))
        return EXIT_FAILURE;

    if (!(vpss_lib->fnSetFrameControl = (int(*)(unsigned int, f2_vpss_framectrl *))
        hal_symbol_load("f2_vpss", vpss_lib->handle, "FH_VPSS_SetFramectrl")))
        return EXIT_FAILURE;

    vpss_lib->fnInitChannelMemory = (int(*)(unsigned int, unsigned int, unsigned int))
        dlsym(vpss_lib->handle, "FH_VPSS_ChnInitMem");

    if (!(vpss_lib->fnSetInputConfig = (int(*)(f2_common_dim *))
        hal_symbol_load("f2_vpss", vpss_lib->handle, "FH_VPSS_SetViAttr")))
        return EXIT_FAILURE;

    if (!(vpss_lib->fnSetOutputMode = (int(*)(unsigned int, unsigned int))
        hal_symbol_load("f2_vpss", vpss_lib->handle, "FH_VPSS_SetVOMode")))
        return EXIT_FAILURE;

    vpss_lib->fnSetGraph = (int(*)(f2_vpss_graph*))dlsym(vpss_lib->handle, "FH_VPSS_SetGraph");

    if (!(vpss_lib->fnUnfreezeVideo = (int(*)(void))
        hal_symbol_load("f2_vpss", vpss_lib->handle, "FH_VPSS_UnfreezeVideo")))
        return EXIT_FAILURE;

    return EXIT_SUCCESS;
}

static void f2_vpss_unload(f2_vpss_impl *vpss_lib) {
    if (vpss_lib->handle) dlclose(vpss_lib->handle);
    vpss_lib->handle = NULL;
    memset(vpss_lib, 0, sizeof(*vpss_lib));
}
