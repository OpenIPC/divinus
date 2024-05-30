#pragma once

#include "tx_common.h"

#define TX_SYS_API "1.0"

typedef enum {
    TX_SYS_DEV_FS,
    TX_SYS_DEV_ENC,
    TX_SYS_DEV_DEC,
    TX_SYS_DEV_IVS,
    TX_SYS_DEV_OSD,
    TX_SYS_DEV_FG1DIRECT,
    TX_SYS_DEV_RSVD,
    TX_SYS_DED_RSVD_END = 23,
    TX_SYS_DEV_END
} tx_sys_dev;

typedef struct {
    tx_sys_dev device;
    int group;
    int port;
} tx_sys_bind;

typedef struct {
    char version[64];
} tx_sys_ver;

typedef struct {
    void *handle;
    
    int (*fnExit)(void);
    int (*fnGetChipName)(const char *chip);
    int (*fnGetVersion)(tx_sys_ver *version);
    
    int (*fnInit)(void);

    int (*fnBind)(tx_sys_bind *source, tx_sys_bind *dest);
    int (*fnUnbind)(tx_sys_bind *source, tx_sys_bind *dest);
} tx_sys_impl;

static int tx_sys_load(tx_sys_impl *sys_lib) {
    if (!(sys_lib->handle = dlopen("libimp.so", RTLD_LAZY | RTLD_GLOBAL))) {
        fprintf(stderr, "[tx_sys] Failed to load library!\nError: %s\n", dlerror());
        return EXIT_FAILURE;
    }

    if (!(sys_lib->fnExit = (int(*)(void))
        dlsym(sys_lib->handle, "IMP_System_Exit"))) {
        fprintf(stderr, "[tx_sys] Failed to acquire symbol IMP_System_Exit!\n");
        return EXIT_FAILURE;
    }

    if (!(sys_lib->fnGetChipName = (int(*)(const char *chip))
        dlsym(sys_lib->handle, "IMP_System_GetCPUInfo"))) {
        fprintf(stderr, "[tx_sys] Failed to acquire symbol IMP_System_GetCPUInfo!\n");
        return EXIT_FAILURE;
    }

    if (!(sys_lib->fnGetVersion = (int(*)(tx_sys_ver *version))
        dlsym(sys_lib->handle, "IMP_System_GetVersion"))) {
        fprintf(stderr, "[tx_sys] Failed to acquire symbol IMP_System_GetVersion!\n");
        return EXIT_FAILURE;
    }

    if (!(sys_lib->fnInit = (int(*)(void))
        dlsym(sys_lib->handle, "IMP_System_Init"))) {
        fprintf(stderr, "[tx_sys] Failed to acquire symbol IMP_System_Init!\n");
        return EXIT_FAILURE;
    }

    if (!(sys_lib->fnBind = (int(*)(tx_sys_bind *source, tx_sys_bind *dest))
        dlsym(sys_lib->handle, "IMP_System_Bind"))) {
        fprintf(stderr, "[tx_sys] Failed to acquire symbol IMP_System_Bind!\n");
        return EXIT_FAILURE;
    }

    if (!(sys_lib->fnUnbind = (int(*)(tx_sys_bind *source, tx_sys_bind *dest))
        dlsym(sys_lib->handle, "IMP_System_UnBind"))) {
        fprintf(stderr, "[tx_sys] Failed to acquire symbol IMP_System_UnBind!\n");
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}

static void tx_sys_unload(tx_sys_impl *sys_lib) {
    if (sys_lib->handle) dlclose(sys_lib->handle);
    sys_lib->handle = NULL;
    memset(sys_lib, 0, sizeof(*sys_lib));
}