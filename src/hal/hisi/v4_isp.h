#pragma once

#include "v4_common.h"

extern void* (*fnIsp_Malloc)(unsigned long);
extern int   (*fnISP_AlgRegisterAcs)(int);
extern int   (*fnISP_AlgRegisterDehaze)(int);
extern int   (*fnISP_AlgRegisterDrc)(int);
extern int   (*fnISP_AlgRegisterLdci)(int);
extern int   (*fnMPI_ISP_IrAutoRunOnce)(int, void*);

typedef enum {
    V4_ISP_DIR_NORMAL,
    V4_ISP_DIR_MIRROR,
    V4_ISP_DIR_FLIP,
    V4_ISP_DIR_MIRROR_FLIP,
    V4_ISP_DIR_END
} v4_isp_dir;

typedef struct {
    int id;
    char libName[20];
} v4_isp_alg;

typedef struct {
    v4_common_rect capt;
    v4_common_dim size;
    float framerate;
    v4_common_bayer bayer;
    v4_common_wdr wdr;
    unsigned char mode;
} v4_isp_dev;

typedef struct {
    void *handleCalcFlick, *handle, *handleAcs, *handleDehaze, *handleDrc, *handleLdci, *handleIrAuto, *handleAwb, 
        *handleGokeAwb, *handleAe, *handleGokeAe,  *handleGoke;

    int (*fnExit)(int pipe);
    int (*fnInit)(int pipe);
    int (*fnMemInit)(int pipe);
    int (*fnRun)(int pipe);

    int (*fnSetDeviceConfig)(int pipe, v4_isp_dev *config);

    int (*fnRegisterAE)(int pipe, v4_isp_alg *library);
    int (*fnRegisterAWB)(int pipe, v4_isp_alg *library);
    int (*fnUnregisterAE)(int pipe, v4_isp_alg *library);
    int (*fnUnregisterAWB)(int pipe, v4_isp_alg *library);
} v4_isp_impl;

static int v4_isp_load(v4_isp_impl *isp_lib) {
    if ((isp_lib->handleCalcFlick = dlopen("lib_hicalcflicker.so", RTLD_LAZY | RTLD_GLOBAL)) &&
        (isp_lib->handle = dlopen("libisp.so", RTLD_LAZY | RTLD_GLOBAL)) &&
        (isp_lib->handleAe = dlopen("lib_hiae.so", RTLD_LAZY | RTLD_GLOBAL)) &&
        (isp_lib->handleAwb = dlopen("lib_hiawb.so", RTLD_LAZY | RTLD_GLOBAL)) &&
        (isp_lib->handleIrAuto = dlopen("lib_hiir_auto.so", RTLD_LAZY | RTLD_GLOBAL)) &&
        (isp_lib->handleLdci = dlopen("lib_hildci.so", RTLD_LAZY | RTLD_GLOBAL)) &&
        (isp_lib->handleDehaze = dlopen("lib_hidehaze.so", RTLD_LAZY | RTLD_GLOBAL)) &&
        (isp_lib->handleDrc = dlopen("lib_hidrc.so", RTLD_LAZY | RTLD_GLOBAL)) &&
        (isp_lib->handleAcs = dlopen("lib_hiacs.so", RTLD_LAZY | RTLD_GLOBAL)))
        goto loaded;

    if ((isp_lib->handleGoke = dlopen("libgk_isp.so", RTLD_LAZY | RTLD_GLOBAL)) &&
        (isp_lib->handleGokeAe = dlopen("libgk_ae.so", RTLD_LAZY | RTLD_GLOBAL)) &&
        (isp_lib->handleAe = dlopen("libhi_ae.so", RTLD_LAZY | RTLD_GLOBAL)) &&
        (isp_lib->handleGokeAwb = dlopen("libgk_awb.so", RTLD_LAZY | RTLD_GLOBAL)) &&
        (isp_lib->handleAwb = dlopen("libhi_awb.so", RTLD_LAZY | RTLD_GLOBAL)) &&
        (isp_lib->handleIrAuto = dlopen("libir_auto.so", RTLD_LAZY | RTLD_GLOBAL)) &&
        (isp_lib->handleLdci = dlopen("libldci.so", RTLD_LAZY | RTLD_GLOBAL)) &&
        (isp_lib->handleDrc = dlopen("libdrc.so", RTLD_LAZY | RTLD_GLOBAL)) &&
        (isp_lib->handleDehaze = dlopen("libdehaze.so", RTLD_LAZY | RTLD_GLOBAL)) &&
        (isp_lib->handle = dlopen("libhi_isp.so", RTLD_LAZY | RTLD_GLOBAL)))
        goto loaded;

    HAL_ERROR("v4_isp", "Failed to load library!\nError: %s\n", dlerror());

loaded:
    if (!isp_lib->handleGoke) {
        if (!(fnIsp_Malloc = (void*(*)(unsigned long))
            hal_symbol_load("v4_isp", isp_lib->handle, "isp_malloc")))
            return EXIT_FAILURE;
            
        if (!(fnISP_AlgRegisterAcs = (int(*)(int))
            hal_symbol_load("v4_isp", isp_lib->handleAcs, "isp_alg_register_acs")))
            return EXIT_FAILURE;

        if (!(fnISP_AlgRegisterDehaze = (int(*)(int))
            hal_symbol_load("v4_isp", isp_lib->handleDehaze, "isp_alg_register_dehaze")))
            return EXIT_FAILURE;

        if (!(fnISP_AlgRegisterDrc = (int(*)(int))
            hal_symbol_load("v4_isp", isp_lib->handleDrc, "isp_alg_register_drc")))
            return EXIT_FAILURE;

        if (!(fnISP_AlgRegisterLdci = (int(*)(int))
            hal_symbol_load("v4_isp", isp_lib->handleLdci, "isp_alg_register_ldci")))
            return EXIT_FAILURE;

        if (!(fnMPI_ISP_IrAutoRunOnce = (int(*)(int, void*))
            hal_symbol_load("v4_isp", isp_lib->handleIrAuto, "isp_ir_auto_run_once")))
            return EXIT_FAILURE;
    } else {
        if (!(fnISP_AlgRegisterDehaze = (int(*)(int))
            hal_symbol_load("v4_isp", isp_lib->handleDehaze, "ISP_AlgRegisterDehaze")))
            return EXIT_FAILURE;

        if (!(fnISP_AlgRegisterDrc = (int(*)(int))
            hal_symbol_load("v4_isp", isp_lib->handleDrc, "ISP_AlgRegisterDrc")))
            return EXIT_FAILURE;

        if (!(fnISP_AlgRegisterLdci = (int(*)(int))
            hal_symbol_load("v4_isp", isp_lib->handleLdci, "ISP_AlgRegisterLdci")))
            return EXIT_FAILURE;

        if (!(fnMPI_ISP_IrAutoRunOnce = (int(*)(int, void*))
            hal_symbol_load("v4_isp", isp_lib->handleIrAuto, "MPI_ISP_IrAutoRunOnce")))
            return EXIT_FAILURE;
    }

    if (!(isp_lib->fnExit = (int(*)(int pipe))
        hal_symbol_load("v4_isp", isp_lib->handle, "HI_MPI_ISP_Exit")))
        return EXIT_FAILURE;

    if (!(isp_lib->fnInit = (int(*)(int pipe))
        hal_symbol_load("v4_isp", isp_lib->handle, "HI_MPI_ISP_Init")))
        return EXIT_FAILURE;

    if (!(isp_lib->fnMemInit = (int(*)(int pipe))
        hal_symbol_load("v4_isp", isp_lib->handle, "HI_MPI_ISP_MemInit")))
        return EXIT_FAILURE;

    if (!(isp_lib->fnRun = (int(*)(int pipe))
        hal_symbol_load("v4_isp", isp_lib->handle, "HI_MPI_ISP_Run")))
        return EXIT_FAILURE;

    if (!(isp_lib->fnSetDeviceConfig = (int(*)(int pipe, v4_isp_dev *config))
        hal_symbol_load("v4_isp", isp_lib->handle, "HI_MPI_ISP_SetPubAttr")))
        return EXIT_FAILURE;

    if (!(isp_lib->fnRegisterAE = (int(*)(int pipe, v4_isp_alg *library))
        hal_symbol_load("v4_isp", isp_lib->handleAe, "HI_MPI_AE_Register")))
        return EXIT_FAILURE;

    if (!(isp_lib->fnRegisterAWB = (int(*)(int pipe, v4_isp_alg *library))
        hal_symbol_load("v4_isp", isp_lib->handleAwb, "HI_MPI_AWB_Register")))
        return EXIT_FAILURE;

    if (!(isp_lib->fnUnregisterAE = (int(*)(int pipe, v4_isp_alg *library))
        hal_symbol_load("v4_isp", isp_lib->handleAe, "HI_MPI_AE_UnRegister")))
        return EXIT_FAILURE;

    if (!(isp_lib->fnUnregisterAWB = (int(*)(int pipe, v4_isp_alg *library))
        hal_symbol_load("v4_isp", isp_lib->handleAwb, "HI_MPI_AWB_UnRegister")))
        return EXIT_FAILURE;

    return EXIT_SUCCESS;
}

static void v4_isp_unload(v4_isp_impl *isp_lib) {
    if (isp_lib->handleGoke) dlclose(isp_lib->handleGoke);
    isp_lib->handleGoke = NULL;
    if (isp_lib->handleGokeAe) dlclose(isp_lib->handleGokeAe);
    isp_lib->handleGokeAe = NULL;
    if (isp_lib->handleAe) dlclose(isp_lib->handleAe);
    isp_lib->handleAe = NULL;
    if (isp_lib->handleGokeAwb) dlclose(isp_lib->handleGokeAwb);
    isp_lib->handleGokeAwb = NULL;
    if (isp_lib->handleAwb) dlclose(isp_lib->handleAwb);
    isp_lib->handleAwb = NULL;
    if (isp_lib->handleIrAuto) dlclose(isp_lib->handleIrAuto);
    isp_lib->handleIrAuto = NULL;
    if (isp_lib->handleLdci) dlclose(isp_lib->handleLdci);
    isp_lib->handleLdci = NULL;
    if (isp_lib->handleDrc) dlclose(isp_lib->handleDrc);
    isp_lib->handleDrc = NULL;
    if (isp_lib->handleDehaze) dlclose(isp_lib->handleDehaze);
    isp_lib->handleDehaze = NULL;
    if (isp_lib->handleAcs) dlclose(isp_lib->handleAcs);
    isp_lib->handleAcs = NULL;
    if (isp_lib->handle) dlclose(isp_lib->handle);
    isp_lib->handle = NULL;
    if (isp_lib->handleCalcFlick) dlclose(isp_lib->handleCalcFlick);
    isp_lib->handleCalcFlick = NULL;
    memset(isp_lib, 0, sizeof(*isp_lib));
}
