#pragma once

#include "t31_common.h"

#define T31_SYS_API "1.0"

#define T31_SYS_REG_GEN  0x1300002C
#define T31_SYS_REG_TYPE 0x13540238

#define T31_SYS_GEN_T31  0x31
#define T31_SYS_TYPE_T31X  0x2222
#define T31_SYS_TYPE_T31L  0x3333
#define T31_SYS_TYPE_T31A  0x4444
#define T31_SYS_TYPE_T31ZL 0x5555
#define T31_SYS_TYPE_T31ZX 0x6666
#define T31_SYS_TYPE_T31AL 0xcccc
#define T31_SYS_TYPE_T31ZC 0xdddd
#define T31_SYS_TYPE_T31LC 0xeeee

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
        !(sys_lib->handle = dlopen("libimp.so", RTLD_LAZY | RTLD_GLOBAL)))
        HAL_ERROR("t31_sys", "Failed to load library!\nError: %s\n", dlerror());

    if (!(sys_lib->fnExit = (int(*)(void))
        hal_symbol_load("t31_sys", sys_lib->handle, "IMP_System_Exit")))
        return EXIT_FAILURE;

    if (!(sys_lib->fnGetChipName = (int(*)(const char *chip))
        hal_symbol_load("t31_sys", sys_lib->handle, "IMP_System_GetCPUInfo")))
        return EXIT_FAILURE;

    if (!(sys_lib->fnGetVersion = (int(*)(t31_sys_ver *version))
        hal_symbol_load("t31_sys", sys_lib->handle, "IMP_System_GetVersion")))
        return EXIT_FAILURE;

    if (!(sys_lib->fnInit = (int(*)(void))
        hal_symbol_load("t31_sys", sys_lib->handle, "IMP_System_Init")))
        return EXIT_FAILURE;

    if (!(sys_lib->fnBind = (int(*)(t31_sys_bind *source, t31_sys_bind *dest))
        hal_symbol_load("t31_sys", sys_lib->handle, "IMP_System_Bind")))
        return EXIT_FAILURE;

    if (!(sys_lib->fnUnbind = (int(*)(t31_sys_bind *source, t31_sys_bind *dest))
        hal_symbol_load("t31_sys", sys_lib->handle, "IMP_System_UnBind")))
        return EXIT_FAILURE;

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