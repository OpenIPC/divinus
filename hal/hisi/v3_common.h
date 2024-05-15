#pragma once

#include <dlfcn.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../types.h"
#include "v3_isp.h"
#include "v3_sys.h"
#include "v3_vb.h"
#include "v3_venc.h"
#include "v3_vi.h"
#include "v3_vpss.h"

#define V3_ERROR(x) \
    do { \
        fprintf(stderr, "%s \033[31m%s\033[0m\n", "[v3_hal] (x)"); \
        return EXIT_FAILURE; \
    } while (0)

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
    V3_COMPR_TILE,
    V3_COMPR_LINE,
    V3_COMPR_FRAME,
    V3_COMPR_END
} v3_common_compr;

typedef enum {
    V3_HDR_SDR8,
    V3_HDR_SDR10,
    V3_HDR_HDR10,
    V3_HDR_HLG,
    V3_HDR_SLF,
    V3_HDR_XDR,
    V3_HDR_END
} v3_common_hdr;

typedef enum {
    V3_PIXFMT_RGB444,
    V3_PIXFMT_RGB555,
    V3_PIXFMT_RGB565,
    V3_PIXFMT_RGB888,
    V3_PIXFMT_BGR444,
    V3_PIXFMT_BGR555,
    V3_PIXFMT_BGR565,
    V3_PIXFMT_BGR888,
    V3_PIXFMT_ARGB1555,
    V3_PIXFMT_ARGB4444,
    V3_PIXFMT_ARGB8565,
    V3_PIXFMT_ARGB8888,
    V3_PIXFMT_ARGB2BPP,
    V3_PIXFMT_ABGR1555,
    V3_PIXFMT_ABGR4444,
    V3_PIXFMT_ABGR8565,
    V3_PIXFMT_ABGR8888,
    V3_PIXFMT_RGB_BAYER_8BPP,
    V3_PIXFMT_RGB_BAYER_10BPP,
    V3_PIXFMT_RGB_BAYER_12BPP,
    V3_PIXFMT_RGB_BAYER_14BPP,
    V3_PIXFMT_RGB_BAYER_16BPP,
    V3_PIXFMT_YVU422P,
    V3_PIXFMT_YVU420P,
    V3_PIXFMT_YVU444P,
    V3_PIXFMT_YVU422SP,
    V3_PIXFMT_YVU420SP,
    V3_PIXFMT_YVU444SP,
    V3_PIXFMT_YUV422SP,
    V3_PIXFMT_YUV420SP,
    V3_PIXFMT_YUV444SP,  
    V3_PIXFMT_YUV422_YUYV,
    V3_PIXFMT_YUV422_YVYU,
    V3_PIXFMT_YUV422_UYVY,
    V3_PIXFMT_YUV422_VYUY,
    V3_PIXFMT_YUV422_YYUV,
    V3_PIXFMT_YUV422_YYVU,
    V3_PIXFMT_YUV422_UVYY,
    V3_PIXFMT_YUV422_VUTT,
    V3_PIXFMT_YUV422_VY1UY0,
    V3_PIXFMT_YUV400,
    V3_PIXFMT_UV420,
    V3_PIXFMT_BGR888P,
    V3_PIXFMT_HSV888,
    V3_PIXFMT_HSV888P,
    V3_PIXFMT_LAB888,
    V3_PIXFMT_LAB888P,
    V3_PIXFMT_SBC1,
    V3_PIXFMT_SBC2,
    V3_PIXFMT_SBC2P,
    V3_PIXFMT_SBC3P,
    V3_PIXFMT_S16C1,
    V3_PIXFMT_U8C1,
    V3_PIXFMT_U16C1,
    V3_PIXFMT_S32C1,
    V3_PIXFMT_U32C1,
    V3_PIXFMT_U64C1,
    V3_PIXFMT_S64C1,
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
    V3_VIDFMT_TILE_64X16,
    V3_VIDFMT_TILE_16X8,
    V3_VIDFMT_LINEAR_DISCRETE,
    V3_VIDFMT_END
} v3_common_vidfmt;

typedef enum {
    V3_WDR_NONE,
    V3_WDR_BUILTIN,
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
    unsigned int width;
    unsigned int height;
} v3_common_rect;