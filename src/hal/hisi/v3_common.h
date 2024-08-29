#pragma once

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../symbols.h"
#include "../types.h"

typedef enum {
    V3_BAYER_RG,
    V3_BAYER_GR,
    V3_BAYER_GB,
    V3_BAYER_BG,
    V3_BAYER_END
} v3_common_bayer;

typedef enum {
    V3_COMPR_NONE,
    V3_COMPR_SEG,
    V3_COMPR_SEG128,
    V3_COMPR_LINE,
    V3_COMPR_FRAME,
    V3_COMPR_END
} v3_common_compr;

typedef enum {
    V3_PIXFMT_1BPP,
    V3_PIXFMT_2BPP,
    V3_PIXFMT_4BPP,
    V3_PIXFMT_8BPP,
    V3_PIXFMT_RGB444,
    V3_PIXFMT_ARGB4444,
    V3_PIXFMT_RGB555,
    V3_PIXFMT_RGB565,
    V3_PIXFMT_ARGB1555,
    V3_PIXFMT_RGB888,
    V3_PIXFMT_ARGB8888,
    V3_PIXFMT_RGB888P,
    V3_PIXFMT_RGB_BAYER_8BPP,
    V3_PIXFMT_RGB_BAYER_10BPP,
    V3_PIXFMT_RGB_BAYER_12BPP,
    V3_PIXFMT_RGB_BAYER_14BPP,
    V3_PIXFMT_RGB_BAYER_16BPP,
    V3_PIXFMT_YUV_A422,
    V3_PIXFMT_YUV_A444,
    V3_PIXFMT_YUV422P,
    V3_PIXFMT_YUV420P,
    V3_PIXFMT_YUV444P,
    V3_PIXFMT_YUV422SP,
    V3_PIXFMT_YUV420SP,
    V3_PIXFMT_YUV444SP,
    V3_PIXFMT_YUV422_UYVY,
    V3_PIXFMT_YUV422_YUYV,
    V3_PIXFMT_YUV422_VYUY,
    V3_PIXFMT_YCbCrP,
    V3_PIXFMT_YUV400,
    V3_PIXFMT_END
} v3_common_pixfmt;

typedef enum {
    V3_PREC_8BPP,
    V3_PREC_10BPP,
    V3_PREC_12BPP,
    V3_PREC_14BPP,
    V3_PREC_16BPP,
    V3_PREC_END
} v3_common_prec;

typedef enum {
    V3_VIDFMT_LINEAR,
    V3_VIDFMT_TILE_256X16,
    V3_VIDFMT_TILE_64X16,
    V3_VIDFMT_END
} v3_common_vidfmt;

typedef enum {
    V3_WDR_NONE,
    V3_WDR_BUILTIN,
    V3_WDR_QUDRA,
    V3_WDR_2TO1_LINE,
    V3_WDR_2TO1_FRAME,
    V3_WDR_2TO1_FRAME_FULLRATE,
    V3_WDR_3TO1_LINE,
    V3_WDR_3TO1_FRAME,
    V3_WDR_3TO1_FRAME_FULLRATE,
    V3_WDR_4TO1_LINE,
    V3_WDR_4TO1_FRAME,
    V3_WDR_4TO1_FRAME_FULLRATE,
    V3_WDR_END
} v3_common_wdr;

typedef struct {
    unsigned int topWidth;
    unsigned int bottomWidth;
    unsigned int leftWidth;
    unsigned int rightWidth;
    unsigned int color;
} v3_common_bord;

typedef struct {
    unsigned int width;
    unsigned int height;
} v3_common_dim;

typedef struct {
    int x;
    int y;
} v3_common_pnt;

typedef struct {
    int x;
    int y;
    unsigned int width;
    unsigned int height;
} v3_common_rect;
