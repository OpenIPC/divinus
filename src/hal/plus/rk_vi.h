#pragma once

#include "rk_common.h"

typedef enum {
    RK_VI_BUF_INTERNAL,
    RK_VI_BUF_EXTERNAL,
    RK_VI_BUF_CHNSHARE
} rk_vi_buf;

typedef enum {
    RK_VI_INTF_BT656,
    RK_VI_INTF_BT656_PACKED_YUV,
    RK_VI_INTF_BT601,
    RK_VI_INTF_DIGITAL_CAMERA,
    RK_VI_INTF_BT1120_STANDARD,
    RK_VI_INTF_BT1120_INTERLEAVED,
    RK_VI_INTF_MIPI,
    RK_VI_INTF_MIPI_YURK20_NORMAL,
    RK_VI_INTF_MIPI_YURK20_LEGACY,
    RK_VI_INTF_MIPI_YURK22,
    RK_VI_INTF_LVDS,
    RK_VI_INTF_HISPI,
    RK_VI_INTF_SLVS,
    RK_VI_INTF_END
} rk_vi_intf;

typedef enum {
    RK_VI_MODE_NORMAL,
    RK_VI_MODE_ISP_HDR2 = 0x10,
    RK_VI_MODE_ISP_HDR3 = 0x20
} rk_vi_mode;

typedef enum {
    RK_VI_RMEM_COMPACT,
    RK_VI_RMEM_LOALIGN,
    RK_VI_RMEM_HIALIGN
} rk_vi_rmem;

typedef enum {
    RK_VI_SEQ_VUVU,
    RK_VI_SEQ_UVUV,
    RK_VI_SEQ_UYVY,
    RK_VI_SEQ_VYUY,
    RK_VI_SEQ_YUYV,
    RK_VI_SEQ_YVYU,
    RK_VI_SEQ_END
} rk_vi_seq;

typedef enum {
    RK_VI_VCAP_VIDEO = 1,
    RK_VI_VCAP_VBI = 4,
    RK_VI_VCAP_VBI_SLICED = 6,
    RK_VI_VCAP_VIDEO_MPLANE = 9,
    RK_VI_VCAP_SDR = 11,
    RK_VI_VCAP_META = 13,
    // Deprecated
    RK_VI_VCAP_PRIVATE = 0x80
} rk_vi_vcap;

typedef enum {
    RK_VI_VMEM_MMAP = 1,
    RK_VI_VMEM_USERPTR,
    RK_VI_VMEM_OVERLAY,
    RK_VI_VMEM_DMABUF
} rk_vi_vmem;

typedef enum {
    RK_VI_WORK_1MULTIPLEX,
    RK_VI_WORK_2MULTIPLEX,
    RK_VI_WORK_4MULTIPLEX
} rk_vi_work;

typedef struct {
    unsigned int num;
    int pipeId[16];
    int offlineOn;
    int userStarted[16];
} rk_vi_bind;

typedef struct {
    unsigned int bufCount;
    unsigned int bufSize;
    rk_vi_vcap vidCap;
    rk_vi_vmem vidMem;
    char entityName[32];
    int v4l2Off;
    rk_common_dim maxSize;
    rk_common_rect window;
} rk_vi_oisp;

typedef struct {
    rk_common_dim size;
    rk_common_pixfmt pixFmt;
    rk_common_hdr dynRange;
    rk_common_vidfmt videoFmt;
    rk_common_compr compress;
    int mirror;
    int flip;
    unsigned int depth;
    int srcFps;
    int dstFps;
    rk_vi_buf bufType;
    rk_vi_oisp ispOpts;
    int shrChn;
} rk_vi_chn;

typedef struct {
    rk_vi_intf intf;
    rk_vi_work work;
    rk_vi_seq seq;
    int rgbModeOn;
    rk_common_dim maxSize;
    int dataRate2X;
    rk_common_pixfmt pixFmt;
    rk_vi_rmem rawMem;
    rk_vi_vmem vidMem;
    unsigned int bufCount;
    rk_vi_mode mode;
    rk_common_rect crop;
} rk_vi_dev;

typedef struct {
    int ispBypassOn;
    rk_common_dim maxSize;
    rk_common_pixfmt pixFmt;
    rk_common_compr compress;
    rk_common_prec prec;
    int srcFps;
    int dstFps;
    rk_vi_rmem rawMem;
    rk_vi_mode mode;
} rk_vi_pipe;

typedef struct {
    void *handle;

    int (*fnDisableDevice)(int device);
    int (*fnEnableDevice)(int device);
    int (*fnGetDeviceConfig)(int device, rk_vi_dev *config);
    int (*fnSetDeviceConfig)(int device, rk_vi_dev *config);

    int (*fnDisableChannel)(int pipe, int channel);
    int (*fnEnableChannel)(int pipe, int channel);
    int (*fnSetChannelConfig)(int pipe, int channel, rk_vi_chn *config);

    int (*fnBindPipe)(int device, rk_vi_bind *config);
    int (*fnCreatePipe)(int pipe, rk_vi_pipe *config);
    int (*fnDestroyPipe)(int pipe);
    int (*fnStartPipe)(int pipe);
    int (*fnStopPipe)(int pipe);
} rk_vi_impl;

static int rk_vi_load(rk_vi_impl *vi_lib) {
    if (!(vi_lib->handle = dlopen("librockit.so", RTLD_LAZY | RTLD_GLOBAL)))
        HAL_ERROR("rk_vi", "Failed to load library!\nError: %s\n", dlerror());

    if (!(vi_lib->fnDisableDevice = (int(*)(int device))
        hal_symbol_load("rk_vi", vi_lib->handle, "RK_MPI_VI_DisableDev")))
        return EXIT_FAILURE;

    if (!(vi_lib->fnEnableDevice = (int(*)(int device))
        hal_symbol_load("rk_vi", vi_lib->handle, "RK_MPI_VI_EnableDev")))
        return EXIT_FAILURE;

    if (!(vi_lib->fnGetDeviceConfig = (int(*)(int device, rk_vi_dev *config))
        hal_symbol_load("rk_vi", vi_lib->handle, "RK_MPI_VI_GetDevAttr")))
        return EXIT_FAILURE;

    if (!(vi_lib->fnSetDeviceConfig = (int(*)(int device, rk_vi_dev *config))
        hal_symbol_load("rk_vi", vi_lib->handle, "RK_MPI_VI_SetDevAttr")))
        return EXIT_FAILURE;

    if (!(vi_lib->fnDisableChannel = (int(*)(int pipe, int channel))
        hal_symbol_load("rk_vi", vi_lib->handle, "RK_MPI_VI_DisableChn")))
        return EXIT_FAILURE;

    if (!(vi_lib->fnEnableChannel = (int(*)(int pipe, int channel))
        hal_symbol_load("rk_vi", vi_lib->handle, "RK_MPI_VI_EnableChn")))
        return EXIT_FAILURE;

    if (!(vi_lib->fnSetChannelConfig = (int(*)(int pipe, int channel, rk_vi_chn *config))
        hal_symbol_load("rk_vi", vi_lib->handle, "RK_MPI_VI_SetChnAttr")))
        return EXIT_FAILURE;


    if (!(vi_lib->fnBindPipe = (int(*)(int device, rk_vi_bind *config))
        hal_symbol_load("rk_vi", vi_lib->handle, "RK_MPI_VI_SetDevBindPipe")))
        return EXIT_FAILURE;

    if (!(vi_lib->fnCreatePipe = (int(*)(int pipe, rk_vi_pipe *config))
        hal_symbol_load("rk_vi", vi_lib->handle, "RK_MPI_VI_CreatePipe")))
        return EXIT_FAILURE;

    if (!(vi_lib->fnDestroyPipe = (int(*)(int pipe))
        hal_symbol_load("rk_vi", vi_lib->handle, "RK_MPI_VI_DestroyPipe")))
        return EXIT_FAILURE;

    if (!(vi_lib->fnStartPipe = (int(*)(int pipe))
        hal_symbol_load("rk_vi", vi_lib->handle, "RK_MPI_VI_StartPipe")))
        return EXIT_FAILURE;

    if (!(vi_lib->fnStopPipe = (int(*)(int pipe))
        hal_symbol_load("rk_vi", vi_lib->handle, "RK_MPI_VI_StopPipe")))
        return EXIT_FAILURE;

    return EXIT_SUCCESS;
}

static void rk_vi_unload(rk_vi_impl *vi_lib) {
    if (vi_lib->handle) dlclose(vi_lib->handle);
    vi_lib->handle = NULL;
    memset(vi_lib, 0, sizeof(*vi_lib));
}