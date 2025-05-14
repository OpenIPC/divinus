#pragma once

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../symbols.h"
#include "../types.h"

typedef enum {
    AW_BAYER_RG,
    AW_BAYER_GR,
    AW_BAYER_GB,
    AW_BAYER_BG,
    AW_BAYER_END
} aw_common_bayer;

typedef enum {
    AW_COMPR_NONE,
    AW_COMPR_SEG,
    AW_COMPR_SEG128,
    AW_COMPR_LINE,
    AW_COMPR_FRAME,
    AW_COMPR_END
} aw_common_compr;

typedef enum {
    AW_HDR_SDR8,
    AW_HDR_SDR10,
    AW_HDR_HDR10,
    AW_HDR_HLG,
    AW_HDR_SLF,
    AW_HDR_XDR,
    AW_HDR_END
} aw_common_hdr;

typedef enum {
    AW_MIRR_NONE,
    AW_MIRR_HORIZ,
    AW_MIRR_VERT,
    AW_MIRR_BOTH,
    AW_MIRR_END
} aw_common_mirr;

typedef enum {
    AW_PIXFMT_RGB_1BPP,
    AW_PIXFMT_RGB_2BPP,
    AW_PIXFMT_RGB_4BPP,
    AW_PIXFMT_RGB_8BPP,
    AW_PIXFMT_RGB444,
    AW_PIXFMT_RGB4444,
    AW_PIXFMT_RGB555,
    AW_PIXFMT_RGB565,
    AW_PIXFMT_RGB1555,
    AW_PIXFMT_RGB888,
    AW_PIXFMT_RGB8888,
    AW_PIXFMT_RGB888P,
    AW_PIXFMT_RGB_BAYER_8BPP,
    AW_PIXFMT_RGB_BAYER_10BPP,
    AW_PIXFMT_RGB_BAYER_12BPP,
    AW_PIXFMT_RGB_BAYER_14BPP,
    AW_PIXFMT_RGB_BAYER_16BPP,
    AW_PIXFMT_RGB_BAYER = AW_PIXFMT_RGB_BAYER_16BPP,
    AW_PIXFMT_YUV_A422,
    AW_PIXFMT_YUV_A444,
    AW_PIXFMT_YUV422P,
    AW_PIXFMT_YUV420P,
    AW_PIXFMT_YUV444P,
    AW_PIXFMT_YUV422SP,
    AW_PIXFMT_YUV420SP,
    AW_PIXFMT_YUV444SP,
    AW_PIXFMT_YUV_UYVY,
    AW_PIXFMT_YUV_YUYV,
    AW_PIXFMT_YUV_VYUY,
    AW_PIXFMT_YUV_YCbCr,
    AW_PIXFMT_SINGLE,
    AW_PIXFMT_YVU420P,
    AW_PIXFMT_YVU422SP,
    AW_PIXFMT_YVU420SP,
    AW_PIXFMT_YUV_AFBC,
    AW_PIXFMT_YUV_LBC_2_0X,
    AW_PIXFMT_YUV_LBC_2_5X,
    AW_PIXFMT_YUV_LBC_1_5X,
    AW_PIXFMT_YUV_LBC_1_0X,
    AW_PIXFMT_YUV_YVYU,
    AW_PIXFMT_RAW_SBGGR8,
    AW_PIXFMT_RAW_SGBRG8,
    AW_PIXFMT_RAW_SGRBG8,
    AW_PIXFMT_RAW_SRGGB8,
    AW_PIXFMT_RAW_SBGGR10,
    AW_PIXFMT_RAW_SGBRG10,
    AW_PIXFMT_RAW_SGRBG10,
    AW_PIXFMT_RAW_SRGGB10,
    AW_PIXFMT_RAW_SBGGR12,
    AW_PIXFMT_RAW_SGBRG12,
    AW_PIXFMT_RAW_SGRBG12,
    AW_PIXFMT_RAW_SRGGB12,
    AW_PIXFMT_BUF_NV21S = AW_PIXFMT_YVU420SP,
    AW_PIXFMT_BUF_NV21M = 0x0100,
    AW_PIXFMT_YUV_GREY,
    AW_PIXFMT_END,
} aw_common_pixfmt;

typedef enum {
    AW_PREC_8BPP,
    AW_PREC_10BPP,
    AW_PREC_12BPP,
    AW_PREC_14BPP,
    AW_PREC_16BPP,
    AW_PREC_END
} aw_common_prec;

typedef enum {
    AW_VIDFMT_LINEAR,
    AW_VIDFMT_TILE_64X16,
    AW_VIDFMT_TILE_16X8,
    AW_VIDFMT_LINEAR_DISCRETE,
    AW_VIDFMT_END
} aw_common_vidfmt;

typedef enum {
    AW_WDR_NONE,
    AW_WDR_BUILTIN,
    AW_WDR_QUDRA,
    AW_WDR_2TO1_LINE,
    AW_WDR_2TO1_FRAME,
    AW_WDR_2TO1_FRAME_FULLRATE,
    AW_WDR_3TO1_LINE,
    AW_WDR_3TO1_FRAME,
    AW_WDR_3TO1_FRAME_FULLRATE,
    AW_WDR_4TO1_LINE,
    AW_WDR_4TO1_FRAME,
    AW_WDR_4TO1_FRAME_FULLRATE,
    AW_WDR_END
} aw_common_wdr;

typedef struct {
    unsigned int topWidth;
    unsigned int bottomWidth;
    unsigned int leftWidth;
    unsigned int rightWidth;
    unsigned int color;
} aw_common_bord;

typedef struct {
    unsigned int width;
    unsigned int height;
} aw_common_dim;

typedef struct {
    int x;
    int y;
} aw_common_pnt;

typedef struct {
    int x;
    int y;
    unsigned int width;
    unsigned int height;
} aw_common_rect;
