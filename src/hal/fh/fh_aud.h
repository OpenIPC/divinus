#pragma once

#include "fh_common.h"

/* FH_AC_Set_Config() payload (audio codec on the ARC coprocessor, /dev/fh_audio) */
typedef struct {
    unsigned int ioType;        /* 0/1 = capture, 2/3 = playback */
    unsigned int sampleRate;
    unsigned int bitWidth;      /* 16 or 24 */
    unsigned int encFormat;     /* 0 = PCM, 1..5 = other formats (forces 16 bit) */
    unsigned int channels;
    unsigned int frameSamples;  /* >= 80; vendor uses 320 at 8 kHz */
    unsigned int volume;        /* 0..100 */
} fh_aud_cnf;

typedef struct {
    unsigned int length;        /* filled with the number of bytes read */
    void *data;
} fh_aud_frm;

typedef struct {
    void *handle;

    int (*fnDeinit)(void);
    int (*fnDisable)(void);
    int (*fnEnable)(void);
    int (*fnGetFrame)(fh_aud_frm *frame, unsigned long long *timestamp);
    int (*fnInit)(void);
    int (*fnSetConfig)(fh_aud_cnf *config);
    int (*fnSetMicVolume)(unsigned int volume);
    int (*fnSetVolume)(unsigned int volume);
} fh_aud_impl;

static int fh_aud_load(fh_aud_impl *aud_lib) {
    if (!(aud_lib->handle = dlopen("libacw_mpi.so", RTLD_NOW | RTLD_GLOBAL)))
        HAL_ERROR("fh_aud", "Failed to load library!\nError: %s\n", dlerror());

    if (!(aud_lib->fnDeinit = (int(*)(void))
        hal_symbol_load("fh_aud", aud_lib->handle, "FH_AC_DeInit")))
        return EXIT_FAILURE;

    if (!(aud_lib->fnDisable = (int(*)(void))
        hal_symbol_load("fh_aud", aud_lib->handle, "FH_AC_AI_Disable")))
        return EXIT_FAILURE;

    if (!(aud_lib->fnEnable = (int(*)(void))
        hal_symbol_load("fh_aud", aud_lib->handle, "FH_AC_AI_Enable")))
        return EXIT_FAILURE;

    if (!(aud_lib->fnGetFrame = (int(*)(fh_aud_frm *frame, unsigned long long *timestamp))
        hal_symbol_load("fh_aud", aud_lib->handle, "FH_AC_AI_GetFrameWithPts")))
        return EXIT_FAILURE;

    if (!(aud_lib->fnInit = (int(*)(void))
        hal_symbol_load("fh_aud", aud_lib->handle, "FH_AC_Init")))
        return EXIT_FAILURE;

    if (!(aud_lib->fnSetConfig = (int(*)(fh_aud_cnf *config))
        hal_symbol_load("fh_aud", aud_lib->handle, "FH_AC_Set_Config")))
        return EXIT_FAILURE;

    if (!(aud_lib->fnSetMicVolume = (int(*)(unsigned int volume))
        hal_symbol_load("fh_aud", aud_lib->handle, "FH_AC_AI_MICIN_SetVol")))
        return EXIT_FAILURE;

    if (!(aud_lib->fnSetVolume = (int(*)(unsigned int volume))
        hal_symbol_load("fh_aud", aud_lib->handle, "FH_AC_AI_SetVol")))
        return EXIT_FAILURE;

    return EXIT_SUCCESS;
}

static void fh_aud_unload(fh_aud_impl *aud_lib) {
    if (aud_lib->handle) dlclose(aud_lib->handle);
    aud_lib->handle = NULL;
    memset(aud_lib, 0, sizeof(*aud_lib));
}
