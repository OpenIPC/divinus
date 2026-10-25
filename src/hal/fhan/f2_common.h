#pragma once

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../symbols.h"
#include "../types.h"
#include "fh_common.h"

#define F2_VPSS_CHN_NUM 3
#define F2_VENC_CHN_NUM 8
#define F2_SNR_MAX 24
/* Sensor output format handed to the ISP (bayer/mode word). */
#define F2_SNR_FMT 0x203

/* Error codes returned by the MPI (negative) */
enum {
    F2_ERR_NODEV = -0x3e9,
    F2_ERR_PARAM = -0x3ed,
    F2_ERR_CHN   = -0x3f0,
};

typedef enum {
    F2_VENC_CODEC_JPEG  = 0x01,
    F2_VENC_CODEC_MJPEG = 0x02,
    F2_VENC_CODEC_H264  = 0x04,
    F2_VENC_CODEC_H264B = 0x08,   /* variant that also needs a BGM gop threshold */
    F2_VENC_CODEC_H265  = 0x10,
    F2_VENC_CODEC_H265B = 0x20
} f2_venc_codec;

typedef enum {
    F2_VENC_RATEMODE_H264VBR   = 3,
    F2_VENC_RATEMODE_H264CBR   = 4,
    F2_VENC_RATEMODE_H264FIXQP = 5,
    F2_VENC_RATEMODE_H264AVBR  = 6,
    F2_VENC_RATEMODE_H265VBR   = 7,
    F2_VENC_RATEMODE_H265CBR   = 8,
    F2_VENC_RATEMODE_END
} f2_venc_ratemode;

typedef struct {
    unsigned int width;
    unsigned int height;
} f2_common_dim;
