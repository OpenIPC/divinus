#pragma once

#include "gm_common.h"

// 1MB should be plenty for an IDR frame on this platform
#define GM_VENC_BUF_SIZE (1 * 1024 * 1024)
// Considering the encoder modules we use (H.264 and JPEG)
#define GM_VENC_CHN_NUM 2

#define GM_VENC_SNAP_HEIGHT_MAX 576
#define GM_VENC_SNAP_WIDTH_MAX  720

enum {
    GM_POLL_READ = 1,
    GM_POLL_WRITE
};

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
    unsigned int type;
    unsigned int bsLength;
    unsigned int mvLength;
    unsigned int isKeyFrame;
} gm_venc_evt;

typedef struct {
    void *bind;
    unsigned int evType;
    gm_venc_evt event;
    int internal[4];
} gm_venc_fds;

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
    // Value must be 0x2694 for JPEG on cap0 w/ scaling
    unsigned int extra;
    int reserved[2];
} gm_venc_snap;

typedef struct {
    char *bsData;
    unsigned int bsLength;
    char *mdData;
    unsigned int mdLength;
    unsigned int bsSize;
    unsigned int mdSize;
    int isKeyFrame;
    unsigned int timestamp;
    unsigned int bsChanged;
    unsigned int checksum;
    int isRefFrame;
    unsigned int sliceOff[3];
    int reserved[5];
} gm_venc_pack;

typedef struct {
    void *bind;
    gm_venc_pack pack;
    int ret;
    int reserved[6];
    int internal[28];
} gm_venc_strm;

typedef struct {
    int internal[8];
    gm_common_dim dest;
    struct {
        int fpsDen:16;
        int fpsNum:16;
    };
    gm_venc_rate rate;
    int bFrameNum;
    int motionDataOn;
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
    int reserved[1];
} gm_venc_h264_cnf;

typedef struct {
    int internal[8];
    gm_common_dim dest;
    struct {
        int fpsNum:16;
        int fpsDen:16;
    };
    int quality;
    gm_venc_ratemode mode;
    int bitrate;
    int maxBitrate;
    int reserved[2];
} gm_venc_mjpg_cnf;