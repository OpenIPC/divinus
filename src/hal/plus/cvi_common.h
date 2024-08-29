#pragma once

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../symbols.h"
#include "../types.h"

#define CVI_VI_PIPE_NUM 4
// Non-Sophgo chips support up to 4 channels
#define CVI_VPSS_CHN_NUM 3
#define CVI_VPSS_GRP_NUM 16

typedef enum {
    CVI_BAYER_BG,
    CVI_BAYER_GB,
    CVI_BAYER_GR,
    CVI_BAYER_RG,
    CVI_BAYER_END
} cvi_common_bayer;

typedef enum {
    CVI_COMPR_NONE,
    CVI_COMPR_TILE,
    CVI_COMPR_LINE,
    CVI_COMPR_FRAME,
    CVI_COMPR_END
} cvi_common_compr;

typedef enum {
    CVI_HDR_SDR8,
    CVI_HDR_SDR10,
    CVI_HDR_HDR10,
    CVI_HDR_HLG,
    CVI_HDR_SLF,
    CVI_HDR_XDR,
    CVI_HDR_END
} cvi_common_hdr;

typedef enum {
    CVI_PIXFMT_RGB888,
    CVI_PIXFMT_BGR888,
    CVI_PIXFMT_RGB888P,
    CVI_PIXFMT_BGR888P,
    CVI_PIXFMT_ARGB1555,
    CVI_PIXFMT_ARGB4444,
    CVI_PIXFMT_ARGB8888,
    CVI_PIXFMT_RGB_BAYER_8BPP,
    CVI_PIXFMT_RGB_BAYER_10BPP,
    CVI_PIXFMT_RGB_BAYER_12BPP,
    CVI_PIXFMT_RGB_BAYER_14BPP,
    CVI_PIXFMT_RGB_BAYER_16BPP,
    CVI_PIXFMT_YUV422P,
    CVI_PIXFMT_YUV420P,
    CVI_PIXFMT_YUV444P,
    CVI_PIXFMT_YUV400,
    CVI_PIXFMT_HSV888,
    CVI_PIXFMT_HSV888P,
    CVI_PIXFMT_NV12,
    CVI_PIXFMT_NV21,
    CVI_PIXFMT_NV16,
    CVI_PIXFMT_NV61,  
    CVI_PIXFMT_YUV422_YUYV,
    CVI_PIXFMT_YUV422_UYVY,
    CVI_PIXFMT_YUV422_YVYU,
    CVI_PIXFMT_YUV422_VYUY,
    CVI_PIXFMT_FP32C1 = 32,
    CVI_PIXFMT_FP32C3P,
    CVI_PIXFMT_S32C1,
    CVI_PIXFMT_S32C3P,
    CVI_PIXFMT_U32C1,
    CVI_PIXFMT_U32C3P,
    CVI_PIXFMT_BF16C1,
    CVI_PIXFMT_BF16C3P,
    CVI_PIXFMT_S16C1,
    CVI_PIXFMT_S16C3P,
    CVI_PIXFMT_U16C1,
    CVI_PIXFMT_U16C3P,
    CVI_PIXFMT_S8C1,
    CVI_PIXFMT_S8C3P,
    CVI_PIXFMT_U8C1,
    CVI_PIXFMT_U8C3P,
    CVI_PIXFMT_8BITMODE = 48,
    CVI_PIXFMT_END
} cvi_common_pixfmt;

typedef enum {
    CVI_PREC_8BPP,
    CVI_PREC_10BPP,
    CVI_PREC_12BPP,
    CVI_PREC_14BPP,
    CVI_PREC_16BPP,
    CVI_PREC_END
} cvi_common_prec;

typedef enum {
    CVI_WDR_NONE,
    CVI_WDR_BUILTIN,
    CVI_WDR_QUDRA,
    CVI_WDR_2TO1_LINE,
    CVI_WDR_2TO1_FRAME,
    CVI_WDR_2TO1_FRAME_FULLRATE,
    CVI_WDR_3TO1_LINE,
    CVI_WDR_3TO1_FRAME,
    CVI_WDR_3TO1_FRAME_FULLRATE,
    CVI_WDR_4TO1_LINE,
    CVI_WDR_4TO1_FRAME,
    CVI_WDR_4TO1_FRAME_FULLRATE,
    CVI_WDR_END
} cvi_common_wdr;

typedef struct {
    unsigned int topWidth;
    unsigned int bottomWidth;
    unsigned int leftWidth;
    unsigned int rightWidth;
    unsigned int color;
} cvi_common_bord;

typedef struct {
    unsigned int width;
    unsigned int height;
} cvi_common_dim;

typedef struct {
    int x;
    int y;
} cvi_common_pnt;

typedef struct {
    int x;
    int y;
    unsigned int width;
    unsigned int height;
} cvi_common_rect;
