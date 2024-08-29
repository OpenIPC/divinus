#pragma once

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../symbols.h"
#include "../types.h"

typedef enum {
    V1_BAYER_RG,
    V1_BAYER_GR,
    V1_BAYER_GB,
    V1_BAYER_BG,
    V1_BAYER_END
} v1_common_bayer;

typedef enum {
    V1_COMPR_NONE,
    V1_COMPR_SEG,
    V1_COMPR_SEG128,
    V1_COMPR_LINE,
    V1_COMPR_FRAME,
    V1_COMPR_END
} v1_common_compr;

typedef enum {
    V1_PIXFMT_1BPP,
    V1_PIXFMT_2BPP,
    V1_PIXFMT_4BPP,
    V1_PIXFMT_8BPP,
    V1_PIXFMT_RGB444,
    V1_PIXFMT_ARGB4444,
    V1_PIXFMT_RGB555,
    V1_PIXFMT_RGB565,
    V1_PIXFMT_ARGB1555,
    V1_PIXFMT_RGB888,
    V1_PIXFMT_ARGB8888,
    V1_PIXFMT_RGB888P,
    V1_PIXFMT_RGB_BAYER,
    V1_PIXFMT_YUV_A422,
    V1_PIXFMT_YUV_A444,
    V1_PIXFMT_YUV422P,
    V1_PIXFMT_YUV420P,
    V1_PIXFMT_YUV444P,
    V1_PIXFMT_YUV422SP,
    V1_PIXFMT_YUV420SP,
    V1_PIXFMT_YUV444SP,
    V1_PIXFMT_YUV422_UYVY,
    V1_PIXFMT_YUV422_YUYV,
    V1_PIXFMT_YUV422_VYUY,
    V1_PIXFMT_YCbCrP,
    V1_PIXFMT_RGB422,
    V1_PIXFMT_RGB420,
    V1_PIXFMT_END
} v1_common_pixfmt;

typedef enum {
    V1_PREC_8BPP,
    V1_PREC_10BPP,
    V1_PREC_12BPP,
    V1_PREC_14BPP,
    V1_PREC_16BPP,
    V1_PREC_END
} v1_common_prec;

typedef enum {
    V1_VIDFMT_LINEAR,
    V1_VIDFMT_TILE_256X16,
    V1_VIDFMT_TILE_64X16,
    V1_VIDFMT_END
} v1_common_vidfmt;

typedef enum {
    V1_WDR_NONE,
    V1_WDR_BUILTIN,
    V1_WDR_QUDRA,
    V1_WDR_2TO1_LINE,
    V1_WDR_2TO1_FRAME,
    V1_WDR_2TO1_FRAME_FULLRATE,
    V1_WDR_3TO1_LINE,
    V1_WDR_3TO1_FRAME,
    V1_WDR_3TO1_FRAME_FULLRATE,
    V1_WDR_4TO1_LINE,
    V1_WDR_4TO1_FRAME,
    V1_WDR_4TO1_FRAME_FULLRATE,
    V1_WDR_END
} v1_common_wdr;

typedef struct {
    unsigned int topWidth;
    unsigned int bottomWidth;
    unsigned int leftWidth;
    unsigned int rightWidth;
    unsigned int color;
} v1_common_bord;

typedef struct {
    unsigned int width;
    unsigned int height;
} v1_common_dim;

typedef struct {
    int x;
    int y;
} v1_common_pnt;

typedef struct {
    int x;
    int y;
    unsigned int width;
    unsigned int height;
} v1_common_rect;
