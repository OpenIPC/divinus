#pragma once

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../symbols.h"
#include "../types.h"

typedef enum {
    I6_BAYER_RG,
    I6_BAYER_GR,
    I6_BAYER_BG,
    I6_BAYER_GB,
    I6_BAYER_R0,
    I6_BAYER_G0,
    I6_BAYER_B0,
    I6_BAYER_G1,
    I6_BAYER_G2,
    I6_BAYER_I0,
    I6_BAYER_G3,
    I6_BAYER_I1,
    I6_BAYER_END
} i6_common_bayer;

typedef enum {
    I6_COMPR_NONE,
    I6_COMPR_SEG,
    I6_COMPR_LINE,
    I6_COMPR_FRAME,
    // Valid on infinity6e only
    I6_COMPR_8BIT,
    I6_COMPR_END
} i6_common_compr;

typedef enum {
    I6_EDGE_SINGLE_UP,
    I6_EDGE_SINGLE_DOWN,
    I6_EDGE_DOUBLE,
    I6_EDGE_END 
} i6_common_edge;

typedef enum {
    I6_HDR_OFF,
    I6_HDR_VC,
    I6_HDR_DOL,
    I6_HDR_EMBED,
    I6_HDR_LI,
    I6_HDR_END
} i6_common_hdr;

typedef enum {
    I6_INPUT_VUVU = 0,
    I6_INPUT_UVUV,
    I6_INPUT_UYVY = 0,
    I6_INPUT_VYUY,
    I6_INPUT_YUYV,
    I6_INPUT_YVYU,
    I6_INPUT_END
} i6_common_input;

typedef enum {
    I6_INTF_BT656,
    I6_INTF_DIGITAL_CAMERA,
    I6_INTF_BT1120_STANDARD,
    I6_INTF_BT1120_INTERLEAVED,
    I6_INTF_MIPI,
    I6_INTF_END
} i6_common_intf;

typedef enum {
    I6_PREC_8BPP,
    I6_PREC_10BPP,
    I6_PREC_12BPP,
    I6_PREC_14BPP,
    I6_PREC_16BPP,
    I6_PREC_END
} i6_common_prec;

typedef enum {
    I6_PIXFMT_YUV422_YUYV,
    I6_PIXFMT_ARGB8888,
    I6_PIXFMT_ABGR8888,
    I6_PIXFMT_BGRA8888,
    I6_PIXFMT_RGB565,
    I6_PIXFMT_ARGB1555,
    I6_PIXFMT_ARGB4444,
    I6_PIXFMT_I2,
    I6_PIXFMT_I4,
    I6_PIXFMT_I8,
    I6_PIXFMT_YUV422SP,
    I6_PIXFMT_YUV420SP,
    I6_PIXFMT_YUV420SP_NV21,
    I6_PIXFMT_YUV420_MST,
    I6_PIXFMT_YUV422_UYVY,
    I6_PIXFMT_YUV422_YVYU,
    I6_PIXFMT_YUV422_VYUY,
    I6_PIXFMT_YC420_MSTITLE1_H264,
    I6_PIXFMT_YC420_MSTITLE2_H265,
    I6_PIXFMT_YC420_MSTITLE3_H265,
    I6_PIXFMT_RGB_BAYER,
    I6_PIXFMT_RGB_BAYER_END = 
        I6_PIXFMT_RGB_BAYER + I6_PREC_END * I6_BAYER_END - 1,
    I6_PIXFMT_RGB888,
    I6_PIXFMT_BGR888,
    I6_PIXFMT_GRAY8,
    I6_PIXFMT_RGB101010,
    I6_PIXFMT_RGB888P,
    I6_PIXFMT_END
} i6_common_pixfmt;

typedef struct {
    unsigned short width;
    unsigned short height;
} i6_common_dim;

typedef struct {
    unsigned short x;
    unsigned short y;
    unsigned short width;
    unsigned short height;
} i6_common_rect;

typedef struct {
    int vsyncInv;
    int hsyncInv;
    int pixclkInv;
    unsigned int vsyncDelay;
    unsigned int hsyncDelay;
    unsigned int pixclkDelay;
} i6_common_sync;