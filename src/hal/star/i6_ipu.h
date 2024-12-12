#pragma once

#include "i6_common.h"

#define I6_IPU_MAXIODEPTH 3
#define I6_IPU_YUV420WALIGN 16
#define I6_IPU_YUV420HALIGN 2
#define I6_IPU_RGBWALIGN 16

typedef int (*i6_ipu_rdfn)(void *data, int offset, int size, char *ctx);

typedef enum {
    I6_IPU_FMT_U8,
    I6_IPU_FMT_NV12,
    I6_IPU_FMT_INT16,
    I6_IPU_FMT_INT32,
    I6_IPU_FMT_INT8,
    I6_IPU_FMT_FP32,
    I6_IPU_FMT_UNKNOWN,
    I6_IPU_FMT_ARGB8888,
    I6_IPU_FMT_ABGR8888
} i6_ipu_fmt;

typedef struct {
    unsigned int subnet;
    unsigned int usrDepth;
    unsigned int bufDepth;
} i6_ipu_chn;

typedef struct {
    unsigned int maxDynBufSize;
    unsigned int yuv420Walign;
    unsigned int yuv420Halign;
    unsigned int rgbWalign;
} i6_ipu_dev;

typedef struct {
    unsigned int dimension;
    i6_ipu_fmt format;
    unsigned int shape[8];
    char name[256];
    unsigned int innerMost;
    float scalar;
    long long zeroPnt;
    int alignedBufSize;
} i6_ipu_tend;

typedef struct {
    unsigned int inCnt;
    unsigned int outCnt;
    i6_ipu_tend inDesc[61];
    i6_ipu_tend outDesc[61];
} i6_ipu_tio;

typedef struct {
    void *data[2];
    unsigned long long phyAddr[2];
} i6_ipu_ten;

typedef struct {
    unsigned int count;
    i6_ipu_ten tensor[61];
} i6_ipu_tenv;

typedef struct {
    void *handle;

    int (*fnCreateDevice)(i6_ipu_dev *config, i6_ipu_rdfn readFunc, char *fwPath, unsigned int fwSize);
    int (*fnDestroyDevice)(void);

    int (*fnCreateChannel)(unsigned int *channel, i6_ipu_chn *config, i6_ipu_rdfn readFunc, char *netPath);
    int (*fnDestroyChannel)(unsigned int channel);

    int (*fnGetIOTensorDescription)(unsigned int channel, i6_ipu_tio *desc);
    int (*fnGetInputTensors)(unsigned int channel, i6_ipu_tenv *ins);
    int (*fnGetOutputTensors)(unsigned int channel, i6_ipu_tenv *outs);
    int (*fnSetInputTensors)(unsigned int channel, i6_ipu_tenv *ins);
    int (*fnSetOutputTensors)(unsigned int channel, i6_ipu_tenv *outs);

    int (*fnInvoke)(unsigned int channel, i6_ipu_tenv *ins, i6_ipu_tenv *outs);
} i6_ipu_impl;

static int i6_ipu_load(i6_ipu_impl *ipu_lib) {
    if (!(ipu_lib->handle = dlopen("libmi_ipu.so", RTLD_LAZY | RTLD_GLOBAL)))
        HAL_ERROR("i6_ipu", "Failed to load library!\nError: %s\n", dlerror());

    if (!(ipu_lib->fnCreateDevice = (int(*)(i6_ipu_dev *config, i6_ipu_rdfn readFunc, char *fwPath, unsigned int fwSize))
        hal_symbol_load("i6_ipu", ipu_lib->handle, "MI_IPU_CreateDevice")))
        return EXIT_FAILURE;

    if (!(ipu_lib->fnDestroyDevice = (int(*)(void))
        hal_symbol_load("i6_ipu", ipu_lib->handle, "MI_IPU_DestroyDevice")))
        return EXIT_FAILURE;

    if (!(ipu_lib->fnCreateChannel = (int(*)(unsigned int *channel, i6_ipu_chn *config, i6_ipu_rdfn readFunc, char *netPath))
        hal_symbol_load("i6_ipu", ipu_lib->handle, "MI_IPU_CreateCHN")))
        return EXIT_FAILURE;

    if (!(ipu_lib->fnDestroyChannel = (int(*)(unsigned int channel))
        hal_symbol_load("i6_ipu", ipu_lib->handle, "MI_IPU_DestroyCHN")))
        return EXIT_FAILURE;

    if (!(ipu_lib->fnGetIOTensorDescription = (int(*)(unsigned int channel, i6_ipu_tio *desc))
        hal_symbol_load("i6_ipu", ipu_lib->handle, "MI_IPU_GetInOutTensorDesc")))
        return EXIT_FAILURE;

    if (!(ipu_lib->fnGetInputTensors = (int(*)(unsigned int channel, i6_ipu_tenv *ins))
        hal_symbol_load("i6_ipu", ipu_lib->handle, "MI_IPU_GetInputTensors")))
        return EXIT_FAILURE;

    if (!(ipu_lib->fnGetOutputTensors = (int(*)(unsigned int channel, i6_ipu_tenv *outs))
        hal_symbol_load("i6_ipu", ipu_lib->handle, "MI_IPU_GetOutputTensors")))
        return EXIT_FAILURE;

    if (!(ipu_lib->fnSetInputTensors = (int(*)(unsigned int channel, i6_ipu_tenv *ins))
        hal_symbol_load("i6_ipu", ipu_lib->handle, "MI_IPU_PutInputTensors")))
        return EXIT_FAILURE;

    if (!(ipu_lib->fnSetOutputTensors = (int(*)(unsigned int channel, i6_ipu_tenv *outs))
        hal_symbol_load("i6_ipu", ipu_lib->handle, "MI_IPU_PutOutputTensors")))
        return EXIT_FAILURE;

    if (!(ipu_lib->fnInvoke = (int(*)(unsigned int channel, i6_ipu_tenv *ins, i6_ipu_tenv *outs))
        hal_symbol_load("i6_ipu", ipu_lib->handle, "MI_IPU_Invoke")))
        return EXIT_FAILURE;

    return EXIT_SUCCESS;
}

static void i6_ipu_unload(i6_ipu_impl *ipu_lib) {
    if (ipu_lib->handle) dlclose(ipu_lib->handle);
    ipu_lib->handle = NULL;
    memset(ipu_lib, 0, sizeof(*ipu_lib));
}