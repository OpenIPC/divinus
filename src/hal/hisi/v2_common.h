#pragma once

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../symbols.h"
#include "../types.h"

typedef enum {
    V2_BAYER_RG,
    V2_BAYER_GR,
    V2_BAYER_GB,
    V2_BAYER_BG,
    V2_BAYER_END
} v2_common_bayer;

typedef enum {
    V2_COMPR_NONE,
    V2_COMPR_SEG,
    V2_COMPR_SEG128,
    V2_COMPR_LINE,
    V2_COMPR_FRAME,
    V2_COMPR_END
} v2_common_compr;

typedef enum {
    V2_PIXFMT_1BPP,
    V2_PIXFMT_2BPP,
    V2_PIXFMT_4BPP,
    V2_PIXFMT_8BPP,
    V2_PIXFMT_RGB444,
    V2_PIXFMT_ARGB4444,
    V2_PIXFMT_RGB555,
    V2_PIXFMT_RGB565,
    V2_PIXFMT_ARGB1555,
    V2_PIXFMT_RGB888,
    V2_PIXFMT_ARGB8888,
    V2_PIXFMT_RGB888P,
    V2_PIXFMT_RGB_BAYER_8BPP,
    V2_PIXFMT_RGB_BAYER_10BPP,
    V2_PIXFMT_RGB_BAYER_12BPP,
    V2_PIXFMT_RGB_BAYER_14BPP,
    V2_PIXFMT_RGB_BAYER_16BPP,
    V2_PIXFMT_YUV_A422,
    V2_PIXFMT_YUV_A444,
    V2_PIXFMT_YUV422P,
    V2_PIXFMT_YUV420P,
    V2_PIXFMT_YUV444P,
    V2_PIXFMT_YUV422SP,
    V2_PIXFMT_YUV420SP,
    V2_PIXFMT_YUV444SP,
    V2_PIXFMT_YUV422_UYVY,
    V2_PIXFMT_YUV422_YUYV,
    V2_PIXFMT_YUV422_VYUY,
    V2_PIXFMT_YCbCrP,
    V2_PIXFMT_YUV400,
    V2_PIXFMT_END
} v2_common_pixfmt;

typedef enum {
    V2_PREC_8BPP,
    V2_PREC_10BPP,
    V2_PREC_12BPP,
    V2_PREC_14BPP,
    V2_PREC_16BPP,
    V2_PREC_END
} v2_common_prec;

typedef enum {
    V2_VIDFMT_LINEAR,
    V2_VIDFMT_TILE_256X16,
    V2_VIDFMT_TILE_64X16,
    V2_VIDFMT_END
} v2_common_vidfmt;

typedef enum {
    V2_WDR_NONE,
    V2_WDR_BUILTIN,
    V2_WDR_QUDRA,
    V2_WDR_2TO1_LINE,
    V2_WDR_2TO1_FRAME,
    V2_WDR_2TO1_FRAME_FULLRATE,
    V2_WDR_3TO1_LINE,
    V2_WDR_3TO1_FRAME,
    V2_WDR_3TO1_FRAME_FULLRATE,
    V2_WDR_4TO1_LINE,
    V2_WDR_4TO1_FRAME,
    V2_WDR_4TO1_FRAME_FULLRATE,
    V2_WDR_END
} v2_common_wdr;

typedef struct {
    unsigned int topWidth;
    unsigned int bottomWidth;
    unsigned int leftWidth;
    unsigned int rightWidth;
    unsigned int color;
} v2_common_bord;

typedef struct {
    unsigned int width;
    unsigned int height;
} v2_common_dim;

typedef struct {
    int x;
    int y;
} v2_common_pnt;

typedef struct {
    int x;
    int y;
    unsigned int width;
    unsigned int height;
} v2_common_rect;
