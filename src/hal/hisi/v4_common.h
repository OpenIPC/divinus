#pragma once

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../symbols.h"
#include "../types.h"

typedef enum {
    V4_BAYER_RG,
    V4_BAYER_GR,
    V4_BAYER_GB,
    V4_BAYER_BG,
    V4_BAYER_END
} v4_common_bayer;

typedef enum {
    V4_COMPR_NONE,
    V4_COMPR_SEG,
    V4_COMPR_TILE,
    V4_COMPR_LINE,
    V4_COMPR_FRAME,
    V4_COMPR_END
} v4_common_compr;

typedef enum {
    V4_HDR_SDR8,
    V4_HDR_SDR10,
    V4_HDR_HDR10,
    V4_HDR_HLG,
    V4_HDR_SLF,
    V4_HDR_XDR,
    V4_HDR_END
} v4_common_hdr;

typedef enum {
    V4_PIXFMT_RGB444,
    V4_PIXFMT_RGB555,
    V4_PIXFMT_RGB565,
    V4_PIXFMT_RGB888,
    V4_PIXFMT_BGR444,
    V4_PIXFMT_BGR555,
    V4_PIXFMT_BGR565,
    V4_PIXFMT_BGR888,
    V4_PIXFMT_ARGB1555,
    V4_PIXFMT_ARGB4444,
    V4_PIXFMT_ARGB8565,
    V4_PIXFMT_ARGB8888,
    V4_PIXFMT_ARGB2BPP,
    V4_PIXFMT_ABGR1555,
    V4_PIXFMT_ABGR4444,
    V4_PIXFMT_ABGR8565,
    V4_PIXFMT_ABGR8888,
    V4_PIXFMT_RGB_BAYER_8BPP,
    V4_PIXFMT_RGB_BAYER_10BPP,
    V4_PIXFMT_RGB_BAYER_12BPP,
    V4_PIXFMT_RGB_BAYER_14BPP,
    V4_PIXFMT_RGB_BAYER_16BPP,
    V4_PIXFMT_YVU422P,
    V4_PIXFMT_YVU420P,
    V4_PIXFMT_YVU444P,
    V4_PIXFMT_YVU422SP,
    V4_PIXFMT_YVU420SP,
    V4_PIXFMT_YVU444SP,
    V4_PIXFMT_YUV422SP,
    V4_PIXFMT_YUV420SP,
    V4_PIXFMT_YUV444SP,  
    V4_PIXFMT_YUV422_YUYV,
    V4_PIXFMT_YUV422_YVYU,
    V4_PIXFMT_YUV422_UYVY,
    V4_PIXFMT_YUV422_VYUY,
    V4_PIXFMT_YUV422_YYUV,
    V4_PIXFMT_YUV422_YYVU,
    V4_PIXFMT_YUV422_UVYY,
    V4_PIXFMT_YUV422_VUTT,
    V4_PIXFMT_YUV422_VY1UY0,
    V4_PIXFMT_YUV400,
    V4_PIXFMT_UV420,
    V4_PIXFMT_BGR888P,
    V4_PIXFMT_HSV888,
    V4_PIXFMT_HSV888P,
    V4_PIXFMT_LAB888,
    V4_PIXFMT_LAB888P,
    V4_PIXFMT_SBC1,
    V4_PIXFMT_SBC2,
    V4_PIXFMT_SBC2P,
    V4_PIXFMT_SBC3P,
    V4_PIXFMT_S16C1,
    V4_PIXFMT_U8C1,
    V4_PIXFMT_U16C1,
    V4_PIXFMT_S32C1,
    V4_PIXFMT_U32C1,
    V4_PIXFMT_U64C1,
    V4_PIXFMT_S64C1,
    V4_PIXFMT_END
} v4_common_pixfmt;

typedef enum {
    V4_PREC_8BPP,
    V4_PREC_10BPP,
    V4_PREC_12BPP,
    V4_PREC_14BPP,
    V4_PREC_16BPP,
    V4_PREC_YUV420_8BIT_NORMAL,
    V4_PREC_YUV420_8BIT_LEGACY,
    V4_PREC_YUV422_8BIT,
    V4_PREC_END
} v4_common_prec;

typedef enum {
    V4_VIDFMT_LINEAR,
    V4_VIDFMT_TILE_64X16,
    V4_VIDFMT_TILE_16X8,
    V4_VIDFMT_LINEAR_DISCRETE,
    V4_VIDFMT_END
} v4_common_vidfmt;

typedef enum {
    V4_WDR_NONE,
    V4_WDR_BUILTIN,
    V4_WDR_QUDRA,
    V4_WDR_2TO1_LINE,
    V4_WDR_2TO1_FRAME,
    V4_WDR_2TO1_FRAME_FULLRATE,
    V4_WDR_3TO1_LINE,
    V4_WDR_3TO1_FRAME,
    V4_WDR_3TO1_FRAME_FULLRATE,
    V4_WDR_4TO1_LINE,
    V4_WDR_4TO1_FRAME,
    V4_WDR_4TO1_FRAME_FULLRATE,
    V4_WDR_END
} v4_common_wdr;

typedef struct {
    unsigned int topWidth;
    unsigned int bottomWidth;
    unsigned int leftWidth;
    unsigned int rightWidth;
    unsigned int color;
} v4_common_bord;

typedef struct {
    unsigned int width;
    unsigned int height;
} v4_common_dim;

typedef struct {
    int x;
    int y;
} v4_common_pnt;

typedef struct {
    int x;
    int y;
    unsigned int width;
    unsigned int height;
} v4_common_rect;
