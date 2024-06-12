#pragma once

#include "t31_common.h"

#define T31_SYS_API "1.0"

typedef enum {
    T31_SYS_DEV_FS,
    T31_SYS_DEV_ENC,
    T31_SYS_DEV_DEC,
    T31_SYS_DEV_IVS,
    T31_SYS_DEV_OSD,
    T31_SYS_DEV_FG1DIRECT,
    T31_SYS_DEV_RSVD,
    T31_SYS_DED_RSVD_END = 23,
    T31_SYS_DEV_END
} t31_sys_dev;

typedef struct {
    t31_sys_dev device;
    int group;
    int port;
} t31_sys_bind;

typedef struct {
    char version[64];
} t31_sys_ver;

typedef struct {
    void *handle, *handleAlog, *handleSysutils;
    
    int (*fnExit)(void);
    int (*fnGetChipName)(const char *chip);
    int (*fnGetVersion)(t31_sys_ver *version);
    int (*fnInit)(void);

    int (*fnBind)(t31_sys_bind *source, t31_sys_bind *dest);
    int (*fnUnbind)(t31_sys_bind *source, t31_sys_bind *dest);
} t31_sys_impl;

static int t31_sys_load(t31_sys_impl *sys_lib) {
    if (!(sys_lib->handleSysutils = dlopen("libsysutils.so", RTLD_LAZY | RTLD_GLOBAL)) ||
        !(sys_lib->handleAlog = dlopen("libalog.so", RTLD_LAZY | RTLD_GLOBAL)) ||
        !(sys_lib->handle = dlopen("libimp.so", RTLD_LAZY | RTLD_GLOBAL))) {
        fprintf(stderr, "[t31_sys] Failed to load library!\nError: %s\n", dlerror());
        return EXIT_FAILURE;
    }

    if (!(sys_lib->fnExit = (int(*)(void))
        dlsym(sys_lib->handle, "IMP_System_Exit"))) {
        fprintf(stderr, "[t31_sys] Failed to acquire symbol IMP_System_Exit!\n");
        return EXIT_FAILURE;
    }

    if (!(sys_lib->fnGetChipName = (int(*)(const char *chip))
        dlsym(sys_lib->handle, "IMP_System_GetCPUInfo"))) {
        fprintf(stderr, "[t31_sys] Failed to acquire symbol IMP_System_GetCPUInfo!\n");
        return EXIT_FAILURE;
    }

    if (!(sys_lib->fnGetVersion = (int(*)(t31_sys_ver *version))
        dlsym(sys_lib->handle, "IMP_System_GetVersion"))) {
        fprintf(stderr, "[t31_sys] Failed to acquire symbol IMP_System_GetVersion!\n");
        return EXIT_FAILURE;
    }

    if (!(sys_lib->fnInit = (int(*)(void))
        dlsym(sys_lib->handle, "IMP_System_Init"))) {
        fprintf(stderr, "[t31_sys] Failed to acquire symbol IMP_System_Init!\n");
        return EXIT_FAILURE;
    }

    if (!(sys_lib->fnBind = (int(*)(t31_sys_bind *source, t31_sys_bind *dest))
        dlsym(sys_lib->handle, "IMP_System_Bind"))) {
        fprintf(stderr, "[t31_sys] Failed to acquire symbol IMP_System_Bind!\n");
        return EXIT_FAILURE;
    }

    if (!(sys_lib->fnUnbind = (int(*)(t31_sys_bind *source, t31_sys_bind *dest))
        dlsym(sys_lib->handle, "IMP_System_UnBind"))) {
        fprintf(stderr, "[t31_sys] Failed to acquire symbol IMP_System_UnBind!\n");
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}

static void t31_sys_unload(t31_sys_impl *sys_lib) {
    if (sys_lib->handle) dlclose(sys_lib->handle);
    sys_lib->handle = NULL;
    if (sys_lib->handleAlog) dlclose(sys_lib->handleAlog);
    sys_lib->handleAlog = NULL;
    if (sys_lib->handleSysutils) dlclose(sys_lib->handleSysutils);
    sys_lib->handleSysutils = NULL;
    memset(sys_lib, 0, sizeof(*sys_lib));
}