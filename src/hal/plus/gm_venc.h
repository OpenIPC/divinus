#pragma once

#include "gm_common.h"

// Considering the encoder modules we use (H.264 and JPEG)
#define GM_VENC_CHN_NUM 2

#define GM_VENC_SNAP_HEIGHT_MAX 576
#define GM_VENC_SNAP_WIDTH_MAX  720

typedef enum {
    GM_VENC_CKSUM_NONE,
    GM_VENC_CKSUM_ANY_CRC = 0x101,
    GM_VENC_CKSUM_ANY_SUM,
    GM_VENC_CKSUM_ANY_SUM32,
    GM_VENC_CKSUM_IDR_CRC = 0x201,
    GM_VENC_CKSUM_IDR_SUM,
    GM_VENC_CKSUM_IDR_SUM32
} gm_venc_cksum;

typedef enum {
    GM_VENC_FFWD_NONE,
    GM_VENC_FFWD_ONEFRAME = 2,
    GM_VENC_FFWD_THREEFRAMES = 4
} gm_venc_ffwd;

typedef enum {
    GM_VENC_H264CODE_DEFAULT,
    GM_VENC_H264CODE_CABAC,
    GM_VENC_H264CODE_CAVLC
} gm_venc_h264code;

typedef enum {
    GM_VENC_H264PROF_DEFAULT,
    GM_VENC_H264PROF_BASELINE = 66,
    GM_VENC_H264PROF_MAIN = 77,
    GM_VENC_H264PROF_HIGH = 100
} gm_venc_h264prof;

typedef enum {
    GM_VENC_H264PRES_DEFAULT,
    GM_VENC_H264PRES_PERFORMANCE,
    GM_VENC_H264PRES_BALANCED,
    GM_VENC_H264PRES_QUALITY
} gm_venc_h264pres;

typedef enum {
    GM_VENC_RATEMODE_CBR = 1,
    GM_VENC_RATEMODE_VBR = 2,
    GM_VENC_RATEMODE_ECBR = 3,
    GM_VENC_RATEMODE_EVBR = 4
} gm_venc_ratemode;

typedef struct {
    gm_venc_ratemode mode;
    int gop;
    int initQual;
    int minQual;
    int maxQual;
    int bitrate;
    int maxBitrate;
    int reserved[5];
} gm_venc_rate;

typedef struct {
    void *bind;
    int quality;
    char *buffer;
    unsigned int length;
    gm_common_dim dest;
    int reserved1[3];
    unsigned int timestamp;
    int reserved2[2];
} gm_venc_snap;

typedef struct  {
    int internal[8];
    int multiSlice;
    int frameOrField;
    int grayscaleOn;
    int reserved[5];
} gm_venc_h264_adv;

typedef struct {
    int internal[8];
    gm_common_dim dest;
    int framerate;
    gm_venc_rate rate;
    int reserved3[2];
    struct {
        gm_venc_h264prof profile:8;
        int level:8;
        gm_venc_h264pres preset:8;
        gm_venc_h264code coding:8;
    };
    struct {
        char ipOffset:8;
        char roiDeltaQp:8;
        char reserved1:8;
        char reserved2:8;
    };
    gm_venc_cksum cksumType;
    gm_venc_ffwd ffwdType;
    int reserved4[1];
} gm_venc_h264_cnf;

typedef struct {
    int internal[8];
    gm_common_dim dest;
    int framerate;
    int quality;
    gm_venc_ratemode mode;
    int bitrate;
    int maxBitrate;
    int reserved[2];
} gm_venc_mjpg_cnf;