#pragma once

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../symbols.h"
#include "../types.h"

typedef enum {
    M6_BAYER_RG,
    M6_BAYER_GR,
    M6_BAYER_BG,
    M6_BAYER_GB,
    M6_BAYER_R0,
    M6_BAYER_G0,
    M6_BAYER_B0,
    M6_BAYER_G1,
    M6_BAYER_G2,
    M6_BAYER_I0,
    M6_BAYER_G3,
    M6_BAYER_I1,
    M6_BAYER_END
} m6_common_bayer;

typedef enum {
    M6_COMPR_NONE,
    M6_COMPR_SEG,
    M6_COMPR_LINE,
    M6_COMPR_FRAME,
    M6_COMPR_8BIT,
    M6_COMPR_END
} m6_common_compr;

typedef enum {
    M6_EDGE_SINGLE_UP,
    M6_EDGE_SINGLE_DOWN,
    M6_EDGE_DOUBLE,
    M6_EDGE_END 
} m6_common_edge;

typedef enum {
    M6_HDR_OFF,
    M6_HDR_VC,
    M6_HDR_DOL,
    M6_HDR_EMBED,
    M6_HDR_LI,
    M6_HDR_END
} m6_common_hdr;

typedef enum {
    M6_INTF_BT656,
    M6_INTF_DIGITAL_CAMERA,
    M6_INTF_BT1120_STANDARD,
    M6_INTF_BT1120_INTERLEAVED,
    M6_INTF_MIPI,
    M6_INTF_END
} m6_common_intf;

typedef enum {
    M6_PREC_8BPP,
    M6_PREC_10BPP,
    M6_PREC_12BPP,
    M6_PREC_14BPP,
    M6_PREC_16BPP,
    M6_PREC_END
} m6_common_prec;

typedef enum {
    M6_PIXFMT_YUV422_YUYV,
    M6_PIXFMT_ARGB8888,
    M6_PIXFMT_ABGR8888,
    M6_PIXFMT_BGRA8888,
    M6_PIXFMT_RGB565,
    M6_PIXFMT_ARGB1555,
    M6_PIXFMT_ARGB4444,
    M6_PIXFMT_I2,
    M6_PIXFMT_I4,
    M6_PIXFMT_I8,
    M6_PIXFMT_YUV422SP,
    M6_PIXFMT_YUV420SP,
    M6_PIXFMT_YUV420SP_NV21,
    M6_PIXFMT_YUV420_TILE,
    M6_PIXFMT_YUV422_UYVY,
    M6_PIXFMT_YUV422_YVYU,
    M6_PIXFMT_YUV422_VYUY,
    M6_PIXFMT_YUV422P,
    M6_PIXFMT_YUV420P,
    M6_PIXFMT_YUV420_FBC,
    M6_PIXFMT_RGB_BAYER,
    M6_PIXFMT_RGB_BAYER_END = 
        M6_PIXFMT_RGB_BAYER + M6_PREC_END * M6_BAYER_END - 1,
    M6_PIXFMT_RGB888,
    M6_PIXFMT_BGR888,
    M6_PIXFMT_GRAY8,
    M6_PIXFMT_RGB101010,
    M6_PIXFMT_END
} m6_common_pixfmt;

typedef struct {
    unsigned short width;
    unsigned short height;
} m6_common_dim;

typedef struct {
    unsigned short x;
    unsigned short y;
    unsigned short width;
    unsigned short height;
} m6_common_rect;