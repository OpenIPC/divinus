#pragma once

#include <dlfcn.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../types.h"
#include "i6f_isp.h"
#include "i6f_scl.h"
#include "i6f_snr.h"
#include "i6f_sys.h"
#include "i6f_venc.h"
#include "i6f_vif.h"

#define I6F_ERROR(x) \
    do { \
        fprintf(stderr, "%s \033[31m%s\033[0m\n", "[i6f_hal] (x)"); \
        return EXIT_FAILURE; \
    } while (0)

typedef enum {
    I6F_BAYER_RG,
    I6F_BAYER_GR,
    I6F_BAYER_BG,
    I6F_BAYER_GB,
    I6F_BAYER_R0,
    I6F_BAYER_G0,
    I6F_BAYER_B0,
    I6F_BAYER_G1,
    I6F_BAYER_G2,
    I6F_BAYER_I0,
    I6F_BAYER_G3,
    I6F_BAYER_I1,
    I6F_BAYER_END
} i6f_common_bayer;

typedef enum {
    I6F_COMPR_NONE,
    I6F_COMPR_SEG,
    I6F_COMPR_LINE,
    I6F_COMPR_FRAME,
    I6F_COMPR_8BIT,
    I6F_COMPR_END
} i6f_common_compr;

typedef enum {
    I6F_EDGE_SINGLE_UP,
    I6F_EDGE_SINGLE_DOWN,
    I6F_EDGE_DOUBLE,
    I6F_EDGE_END 
} i6f_common_edge;

typedef enum {
    I6F_HDR_OFF,
    I6F_HDR_VC,
    I6F_HDR_DOL,
    I6F_HDR_EMBED,
    I6F_HDR_LI,
    I6F_HDR_END
} i6f_common_hdr;

typedef enum {
    I6F_INTF_BT656,
    I6F_INTF_DIGITAL_CAMERA,
    I6F_INTF_BT1120_STANDARD,
    I6F_INTF_BT1120_INTERLEAVED,
    I6F_INTF_MIPI,
    I6F_INTF_END
} i6f_common_intf;

typedef enum {
    I6F_PIXFMT_YUV422_YUYV,
    I6F_PIXFMT_ARGB8888,
    I6F_PIXFMT_ABGR8888,
    I6F_PIXFMT_BGRA8888,
    I6F_PIXFMT_RGB565,
    I6F_PIXFMT_ARGB1555,
    I6F_PIXFMT_ARGB4444,
    I6F_PIXFMT_I2,
    I6F_PIXFMT_I4,
    I6F_PIXFMT_I8,
    I6F_PIXFMT_YUV422SP,
    I6F_PIXFMT_YUV420SP,
    I6F_PIXFMT_YUV420SP_NV21,
    I6F_PIXFMT_YUV420_TILE,
    I6F_PIXFMT_YUV422_UYVY,
    I6F_PIXFMT_YUV422_YVYU,
    I6F_PIXFMT_YUV422_VYUY,
    I6F_PIXFMT_YUV422P,
    I6F_PIXFMT_YUV420P,
    I6F_PIXFMT_YUV420_FBC,
    I6F_PIXFMT_RGB_BAYER,
    I6F_PIXFMT_RGB_BAYER_END = 
        I6F_PIXFMT_RGB_BAYER + I6F_PREC_END * I6F_BAYER_END - 1,
    I6F_PIXFMT_RGB888,
    I6F_PIXFMT_BGR888,
    I6F_PIXFMT_GRAY8,
    I6F_PIXFMT_RGB101010,
    I6F_PIXFMT_END
} i6f_common_pixfmt;

typedef enum {
    I6F_PREC_8BPP,
    I6F_PREC_10BPP,
    I6F_PREC_12BPP,
    I6F_PREC_14BPP,
    I6F_PREC_16BPP,
    I6F_PREC_END
} i6f_common_prec;

typedef struct {
    unsigned short width;
    unsigned short height;
} i6f_common_dim;

typedef struct {
    unsigned short x;
    unsigned short y;
    unsigned short width;
    unsigned short height;
} i6f_common_rect;