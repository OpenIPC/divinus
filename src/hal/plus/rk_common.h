#pragma once

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../symbols.h"
#include "../types.h"

typedef enum {
    RK_BAYER_RG,
    RK_BAYER_GR,
    RK_BAYER_GB,
    RK_BAYER_BG,
    RK_BAYER_END
} rk_common_bayer;

typedef enum {
    RK_COMPR_NONE,
    RK_COMPR_AFBC_16x16,
    RK_COMPR_END
} rk_common_compr;

typedef enum {
    RK_HDR_SDR8,
    RK_HDR_SDR10,
    RK_HDR_HDR10,
    RK_HDR_HLG,
    RK_HDR_SLF,
    RK_HDR_XDR,
    RK_HDR_END
} rk_common_hdr;

typedef enum {
    RK_MIRR_NONE,
    RK_MIRR_HORIZ,
    RK_MIRR_VERT,
    RK_MIRR_BOTH,
    RK_MIRR_END
} rk_common_mirr;

typedef enum {
    RK_PIXFMT_YUV = 0x0,
    RK_PIXFMT_YUV420SP = RK_PIXFMT_YUV, 
    RK_PIXFMT_YUV420SP_10BIT,
    RK_PIXFMT_YUV422SP,
    RK_PIXFMT_YUV422SP_10BIT,
    RK_PIXFMT_YUV420P,
    RK_PIXFMT_YUV420P_VU,
    RK_PIXFMT_YUV420SP_VU,
    RK_PIXFMT_YUV422P,
    RK_PIXFMT_YUV422SP_VU,
    RK_PIXFMT_YUV422_YUYV,
    RK_PIXFMT_YUV422_UYVY,
    RK_PIXFMT_YUV400SP,
    RK_PIXFMT_YUV440SP,
    RK_PIXFMT_YUV411SP,
    RK_PIXFMT_YUV444,
    RK_PIXFMT_YUV444SP,
    RK_PIXFMT_YUV444P,
    RK_PIXFMT_YUV422_YVYU,
    RK_PIXFMT_YUV422_VYUY,
    RK_PIXFMT_YUV_END,

    RK_PIXFMT_RGB = 0x10000,
    RK_PIXFMT_RGB565 = RK_PIXFMT_RGB,
    RK_PIXFMT_BGR565,
    RK_PIXFMT_RGB555,
    RK_PIXFMT_BGR555,
    RK_PIXFMT_RGB444,
    RK_PIXFMT_BGR444,
    RK_PIXFMT_RGB888,
    RK_PIXFMT_BGR888,
    RK_PIXFMT_RGB101010,
    RK_PIXFMT_BGR101010,
    RK_PIXFMT_ARGB1555,
    RK_PIXFMT_ABGR1555,
    RK_PIXFMT_ARGB4444,
    RK_PIXFMT_ABGR4444,
    RK_PIXFMT_ARGB8565,
    RK_PIXFMT_ABGR8565,
    RK_PIXFMT_ARGB8888,
    RK_PIXFMT_ABGR8888,
    RK_PIXFMT_BGRA8888,
    RK_PIXFMT_RGBA8888,
    RK_PIXFMT_RGBA5551,
    RK_PIXFMT_BGRA5551,
    RK_PIXFMT_BGRA4444,
    RK_PIXFMT_RGBA4444,
    RK_PIXFMT_XBGR8888,
    RK_PIXFMT_RGB_END,

    RK_PIXFMT_2BPP,

    RK_PIXFMT_BAYER = 0x20000,
    RK_PIXFMT_BAYER_SBGGR_8BPP = RK_PIXFMT_BAYER,
    RK_PIXFMT_BAYER_SGBRG_8BPP,
    RK_PIXFMT_BAYER_SGRBG_8BPP,
    RK_PIXFMT_BAYER_SRGGB_8BPP,
    RK_PIXFMT_BAYER_SBGGR_10BPP,
    RK_PIXFMT_BAYER_SGBRG_10BPP,
    RK_PIXFMT_BAYER_SGRBG_10BPP,
    RK_PIXFMT_BAYER_SRGGB_10BPP,
    RK_PIXFMT_BAYER_SBGGR_12BPP,
    RK_PIXFMT_BAYER_SGBRG_12BPP,
    RK_PIXFMT_BAYER_SGRBG_12BPP,
    RK_PIXFMT_BAYER_SRGGB_12BPP,
    RK_PIXFMT_BAYER_14BPP,
    RK_PIXFMT_BAYER_SBGGR_16BPP,
    RK_PIXFMT_BAYER_END,
    RK_PIXFMT_END,
} rk_common_pixfmt;

typedef enum {
    RK_PREC_8BPP,
    RK_PREC_10BPP,
    RK_PREC_12BPP,
    RK_PREC_14BPP,
    RK_PREC_16BPP,
    RK_PREC_END
} rk_common_prec;

typedef enum {
    RK_VIDFMT_LINEAR,
    RK_VIDFMT_TILE_64X16,
    RK_VIDFMT_TILE_16X8,
    RK_VIDFMT_LINEAR_DISCRETE,
    RK_VIDFMT_END
} rk_common_vidfmt;

typedef enum {
    RK_WDR_NONE,
    RK_WDR_BUILTIN,
    RK_WDR_QUDRA,
    RK_WDR_2TO1_LINE,
    RK_WDR_2TO1_FRAME,
    RK_WDR_2TO1_FRAME_FULLRATE,
    RK_WDR_3TO1_LINE,
    RK_WDR_3TO1_FRAME,
    RK_WDR_3TO1_FRAME_FULLRATE,
    RK_WDR_4TO1_LINE,
    RK_WDR_4TO1_FRAME,
    RK_WDR_4TO1_FRAME_FULLRATE,
    RK_WDR_END
} rk_common_wdr;

typedef struct {
    unsigned int topWidth;
    unsigned int bottomWidth;
    unsigned int leftWidth;
    unsigned int rightWidth;
    unsigned int color;
} rk_common_bord;

typedef struct {
    unsigned int width;
    unsigned int height;
} rk_common_dim;

typedef struct {
    int x;
    int y;
} rk_common_pnt;

typedef struct {
    int x;
    int y;
    unsigned int width;
    unsigned int height;
} rk_common_rect;
