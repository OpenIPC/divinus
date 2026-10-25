#pragma once

#include "f1_common.h"

typedef struct {
    unsigned short srcRate;   /* input frames considered */
    unsigned short dstRate;   /* output frames kept */
} f1_vpss_framectrl;

typedef struct {
    unsigned int enable;
    unsigned int physAddr;
    unsigned int alpha;
    unsigned int reserved3, reserved4;
    unsigned int width, height;
    unsigned int x, y;
    unsigned int reserved9, reserved10;
    unsigned int stride;        /* bytes per line */
} f1_vpss_graph;

typedef struct {
    void *handle;

    int (*fnCloseChannel)(unsigned int channel);
    int (*fnDisable)(unsigned int group);
    int (*fnEnable)(unsigned int group);
    int (*fnFreezeVideo)(void);
    int (*fnGetChannelCapability)(unsigned int channel, void *cap);
    int (*fnGetChannelConfig)(unsigned int channel, f1_common_dim *dim);
    int (*fnGetFrameControl)(unsigned int channel, f1_vpss_framectrl *ctrl);
    int (*fnInitChannelMemory)(unsigned int channel, unsigned int width, unsigned int height);
    int (*fnOpenChannel)(unsigned int channel);
    int (*fnSetChannelConfig)(unsigned int channel, f1_common_dim *dim);
    int (*fnSetFrameControl)(unsigned int channel, f1_vpss_framectrl *ctrl);
    int (*fnSetGraph)(f1_vpss_graph *graph);
    int (*fnSetInputConfig)(f1_common_dim *dim);
    int (*fnSetOutputMode)(unsigned int channel, unsigned int mode);
    int (*fnUnfreezeVideo)(void);
} f1_vpss_impl;

static int f1_vpss_load(f1_vpss_impl *vpss_lib) {
    if (!(vpss_lib->handle = dlopen("libdsp.so", RTLD_NOW | RTLD_GLOBAL)))
        HAL_ERROR("f1_vpss", "Failed to load library!\nError: %s\n", dlerror());

    if (!(vpss_lib->fnCloseChannel = (int(*)(unsigned int channel))
        hal_symbol_load("f1_vpss", vpss_lib->handle, "FH_VPSS_CloseChn")))
        return EXIT_FAILURE;

    if (!(vpss_lib->fnDisable = (int(*)(unsigned int group))
        hal_symbol_load("f1_vpss", vpss_lib->handle, "FH_VPSS_Disable")))
        return EXIT_FAILURE;

    if (!(vpss_lib->fnEnable = (int(*)(unsigned int group))
        hal_symbol_load("f1_vpss", vpss_lib->handle, "FH_VPSS_Enable")))
        return EXIT_FAILURE;

    if (!(vpss_lib->fnFreezeVideo = (int(*)(void))
        hal_symbol_load("f1_vpss", vpss_lib->handle, "FH_VPSS_FreezeVideo")))
        return EXIT_FAILURE;

    if (!(vpss_lib->fnGetChannelConfig = (int(*)(unsigned int channel, f1_common_dim *dim))
        hal_symbol_load("f1_vpss", vpss_lib->handle, "FH_VPSS_GetChnAttr")))
        return EXIT_FAILURE;

    if (!(vpss_lib->fnGetFrameControl = (int(*)(unsigned int channel, f1_vpss_framectrl *ctrl))
        hal_symbol_load("f1_vpss", vpss_lib->handle, "FH_VPSS_GetFramectrl")))
        return EXIT_FAILURE;

    if (!(vpss_lib->fnOpenChannel = (int(*)(unsigned int channel))
        hal_symbol_load("f1_vpss", vpss_lib->handle, "FH_VPSS_OpenChn")))
        return EXIT_FAILURE;

    if (!(vpss_lib->fnSetChannelConfig = (int(*)(unsigned int channel, f1_common_dim *dim))
        hal_symbol_load("f1_vpss", vpss_lib->handle, "FH_VPSS_SetChnAttr")))
        return EXIT_FAILURE;

    if (!(vpss_lib->fnSetFrameControl = (int(*)(unsigned int channel, f1_vpss_framectrl *ctrl))
        hal_symbol_load("f1_vpss", vpss_lib->handle, "FH_VPSS_SetFramectrl")))
        return EXIT_FAILURE;

    if (!(vpss_lib->fnSetInputConfig = (int(*)(f1_common_dim *dim))
        hal_symbol_load("f1_vpss", vpss_lib->handle, "FH_VPSS_SetViAttr")))
        return EXIT_FAILURE;

    if (!(vpss_lib->fnSetOutputMode = (int(*)(unsigned int channel, unsigned int mode))
        hal_symbol_load("f1_vpss", vpss_lib->handle, "FH_VPSS_SetVOMode")))
        return EXIT_FAILURE;

    if (!(vpss_lib->fnUnfreezeVideo = (int(*)(void))
        hal_symbol_load("f1_vpss", vpss_lib->handle, "FH_VPSS_UnfreezeVideo")))
        return EXIT_FAILURE;

    vpss_lib->fnGetChannelCapability = (int(*)(unsigned int, void*))
        dlsym(vpss_lib->handle, "FH_VPSS_GetChnCapality");
    vpss_lib->fnInitChannelMemory = (int(*)(unsigned int, unsigned int, unsigned int))
        dlsym(vpss_lib->handle, "FH_VPSS_ChnInitMem");
    vpss_lib->fnSetGraph = (int(*)(f1_vpss_graph*))
        dlsym(vpss_lib->handle, "FH_VPSS_SetGraph");

    return EXIT_SUCCESS;
}

static void f1_vpss_unload(f1_vpss_impl *vpss_lib) {
    if (vpss_lib->handle) dlclose(vpss_lib->handle);
    vpss_lib->handle = NULL;
    memset(vpss_lib, 0, sizeof(*vpss_lib));
}
