#pragma once

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../symbols.h"
#include "../types.h"

typedef enum {
    I6C_BAYER_RG,
    I6C_BAYER_GR,
    I6C_BAYER_BG,
    I6C_BAYER_GB,
    I6C_BAYER_R0,
    I6C_BAYER_G0,
    I6C_BAYER_B0,
    I6C_BAYER_G1,
    I6C_BAYER_G2,
    I6C_BAYER_I0,
    I6C_BAYER_G3,
    I6C_BAYER_I1,
    I6C_BAYER_END
} i6c_common_bayer;

typedef enum {
    I6C_COMPR_NONE,
    I6C_COMPR_SEG,
    I6C_COMPR_LINE,
    I6C_COMPR_FRAME,
    I6C_COMPR_8BIT,
    I6C_COMPR_6BIT,
    I6C_COMPR_IFC,
    I6C_COMPR_SFBC0,
    I6C_COMPR_SFBC1,
    I6C_COMPR_SFBC2,
    I6C_COMPR_END
} i6c_common_compr;

typedef enum {
    I6C_EDGE_SINGLE_UP,
    I6C_EDGE_SINGLE_DOWN,
    I6C_EDGE_DOUBLE,
    I6C_EDGE_END 
} i6c_common_edge;

typedef enum {
    I6C_HDR_OFF,
    I6C_HDR_VC,
    I6C_HDR_DOL,
    I6C_HDR_EMBED,
    I6C_HDR_LI,
    I6C_HDR_END
} i6c_common_hdr;

typedef enum {
    I6C_INTF_BT656,
    I6C_INTF_DIGITAL_CAMERA,
    I6C_INTF_BT1120_STANDARD,
    I6C_INTF_BT1120_INTERLEAVED,
    I6C_INTF_MIPI,
    I6C_INTF_LVDS,
    I6C_INTF_END
} i6c_common_intf;

typedef enum {
    I6C_PREC_8BPP,
    I6C_PREC_10BPP,
    I6C_PREC_12BPP,
    I6C_PREC_14BPP,
    I6C_PREC_16BPP,
    I6C_PREC_END
} i6c_common_prec;

typedef enum {
    I6C_PIXFMT_YUV422_YUYV,
    I6C_PIXFMT_ARGB8888,
    I6C_PIXFMT_ABGR8888,
    I6C_PIXFMT_BGRA8888,
    I6C_PIXFMT_RGB565,
    I6C_PIXFMT_ARGB1555,
    I6C_PIXFMT_ARGB4444,
    I6C_PIXFMT_I2,
    I6C_PIXFMT_I4,
    I6C_PIXFMT_I8,
    I6C_PIXFMT_YUV422SP,
    I6C_PIXFMT_YUV420SP,
    I6C_PIXFMT_YUV420SP_NV21,
    I6C_PIXFMT_YUV420_TILE,
    I6C_PIXFMT_YUV422_UYVY,
    I6C_PIXFMT_YUV422_YVYU,
    I6C_PIXFMT_YUV422_VYUY,
    I6C_PIXFMT_YUV422P,
    I6C_PIXFMT_YUV420P,
    I6C_PIXFMT_YUV420_FBC,
    I6C_PIXFMT_RGB_BAYER,
    I6C_PIXFMT_RGB_BAYER_END = 
        I6C_PIXFMT_RGB_BAYER + I6C_PREC_END * I6C_BAYER_END - 1,
    I6C_PIXFMT_RGB888,
    I6C_PIXFMT_BGR888,
    I6C_PIXFMT_GRAY8,
    I6C_PIXFMT_RGB101010,
    I6C_PIXFMT_RGB888P,
    I6C_PIXFMT_END
} i6c_common_pixfmt;

typedef struct {
    unsigned short width;
    unsigned short height;
} i6c_common_dim;

typedef struct {
    unsigned short x;
    unsigned short y;
    unsigned short width;
    unsigned short height;
} i6c_common_rect;