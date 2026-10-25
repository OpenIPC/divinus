#pragma once

#include "f2_common.h"

/* FH_AC_Set_Config() payload (audio codec on the ARC coprocessor, /dev/f2_audio) */
typedef struct {
    unsigned int io;      /* 0/1 = capture, 2/3 = playback */
    unsigned int rate;
    unsigned int bit;     /* 16 or 24 */
    unsigned int fmt;     /* 0 = PCM, 1..5 = other formats (forces 16 bit) */
    unsigned int chnNum;
    unsigned int frmNum;  /* >= 80; vendor uses 320 at 8 kHz */
    unsigned int volume;  /* 0..100 */
} f2_aud_cnf;

typedef struct {
    unsigned int length;        /* filled with the number of bytes read */
    void *data;
} f2_aud_frm;

typedef struct {
    void *handle;

    int (*fnDeinit)(void);
    int (*fnDisable)(void);
    int (*fnEnable)(void);
    int (*fnGetFrame)(f2_aud_frm *frame, unsigned long long *timestamp);
    int (*fnInit)(void);
    int (*fnSetConfig)(f2_aud_cnf *config);
    int (*fnSetMicVolume)(unsigned int volume);
    int (*fnSetVolume)(unsigned int volume);
} f2_aud_impl;

static int f2_aud_load(f2_aud_impl *aud_lib) {
    if (!(aud_lib->handle = dlopen("libacw_mpi.so", RTLD_NOW | RTLD_GLOBAL)))
        HAL_ERROR("f2_aud", "Failed to load library!\nError: %s\n", dlerror());

    if (!(aud_lib->fnDeinit = (int(*)(void))
        hal_symbol_load("f2_aud", aud_lib->handle, "FH_AC_DeInit")))
        return EXIT_FAILURE;

    if (!(aud_lib->fnDisable = (int(*)(void))
        hal_symbol_load("f2_aud", aud_lib->handle, "FH_AC_AI_Disable")))
        return EXIT_FAILURE;

    if (!(aud_lib->fnEnable = (int(*)(void))
        hal_symbol_load("f2_aud", aud_lib->handle, "FH_AC_AI_Enable")))
        return EXIT_FAILURE;

    if (!(aud_lib->fnGetFrame = (int(*)(f2_aud_frm *frame, unsigned long long *timestamp))
        hal_symbol_load("f2_aud", aud_lib->handle, "FH_AC_AI_GetFrameWithPts")))
        return EXIT_FAILURE;

    if (!(aud_lib->fnInit = (int(*)(void))
        hal_symbol_load("f2_aud", aud_lib->handle, "FH_AC_Init")))
        return EXIT_FAILURE;

    if (!(aud_lib->fnSetConfig = (int(*)(f2_aud_cnf *config))
        hal_symbol_load("f2_aud", aud_lib->handle, "FH_AC_Set_Config")))
        return EXIT_FAILURE;

    if (!(aud_lib->fnSetMicVolume = (int(*)(unsigned int volume))
        hal_symbol_load("f2_aud", aud_lib->handle, "FH_AC_AI_MICIN_SetVol")))
        return EXIT_FAILURE;

    if (!(aud_lib->fnSetVolume = (int(*)(unsigned int volume))
        hal_symbol_load("f2_aud", aud_lib->handle, "FH_AC_AI_SetVol")))
        return EXIT_FAILURE;

    return EXIT_SUCCESS;
}

static void f2_aud_unload(f2_aud_impl *aud_lib) {
    if (aud_lib->handle) dlclose(aud_lib->handle);
    aud_lib->handle = NULL;
    memset(aud_lib, 0, sizeof(*aud_lib));
}
