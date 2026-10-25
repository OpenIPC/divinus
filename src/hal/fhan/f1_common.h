#pragma once

#include "../symbols.h"
#include "../types.h"
#include "common.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/*
 * Fullhan FH8852/FH8856 (V100 generation, SDK V1.2.0 "OSDRV" libraries).
 *
 * The SDK ships as binary-only shared objects without headers; the interface
 * below was recovered from the libraries and from a vendor application that
 * statically links the same SDK. Only the subset divinus needs is declared.
 */

#define F1_VPSS_CHN_NUM 4
#define F1_VENC_CHN_NUM 8
#define F1_SNR_MAX 24

/* Sensor output format handed to the ISP, by frame rate. */
#define F1_SNR_FMT_15 0x601
#define F1_SNR_FMT_20 0x602
#define F1_SNR_FMT_25 0x603
#define F1_SNR_FMT_30 0x604

typedef enum {
    F1_VENC_CODEC_JPEG  = 0x01,
    F1_VENC_CODEC_MJPEG = 0x02,
    F1_VENC_CODEC_H264  = 0x04,
    F1_VENC_CODEC_H264B = 0x08,   /* variant that also needs a BGM gop threshold */
    F1_VENC_CODEC_H265  = 0x10,
    F1_VENC_CODEC_H265B = 0x20
} f1_venc_codec;

typedef enum {
    F1_VENC_RATEMODE_H264_CBR = 3,     /* reported as VBR by /proc/driver/enc but it is the working CBR path; mode 8 boot-loops */
    F1_VENC_RATEMODE_H264_VBR = 4,
    F1_VENC_RATEMODE_H264_FIXQP = 5,
    F1_VENC_RATEMODE_H264_AVBR = 6,
    F1_VENC_RATEMODE_H265_CBR = 7,
    F1_VENC_RATEMODE_H265_VBR = 8
} f1_venc_ratemode;

typedef struct {
    unsigned int width;
    unsigned int height;
} f1_common_dim;
