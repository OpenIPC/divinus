#pragma once

#include "macros.h"

#ifndef ALIGN_BACK
#define ALIGN_BACK(x, a) (((x) / (a)) * (a))
#endif
#ifndef ALIGN_UP
#define ALIGN_UP(x, a) ((((x) + ((a)-1)) / a) * a)
#endif
#ifndef CEILING_2_POWER
#define CEILING_2_POWER(x, a) (((x) + ((a)-1)) & (~((a) - 1)))
#endif
#ifndef MIN
#define MIN(a, b) (((a) < (b)) ? (a) : (b))
#endif
#ifndef MAX
#define MAX(a, b) (((a) > (b)) ? (a) : (b))
#endif

typedef enum {
    HAL_PLATFORM_UNK,
    HAL_PLATFORM_AK,
    HAL_PLATFORM_CVI,
    HAL_PLATFORM_GM,
    HAL_PLATFORM_I3,
    HAL_PLATFORM_I6,
    HAL_PLATFORM_I6C,
    HAL_PLATFORM_I6F,
    HAL_PLATFORM_T31,
    HAL_PLATFORM_V1,
    HAL_PLATFORM_V2,
    HAL_PLATFORM_V3,
    HAL_PLATFORM_V4
} hal_platform;

typedef enum {
    OP_READ = 0b1,
    OP_WRITE = 0b10,
    OP_MODIFY = 0b11
} hal_register_op;

typedef enum {
    HAL_AUDCODEC_UNSPEC,
    HAL_AUDCODEC_MP3 = 0x69,
    HAL_AUDCODEC_NONE = 0xFF
} hal_audcodec;

typedef enum {
    HAL_VIDCODEC_UNSPEC,
    HAL_VIDCODEC_H264,
    HAL_VIDCODEC_H265,
    HAL_VIDCODEC_MJPG,
    HAL_VIDCODEC_JPG
} hal_vidcodec;

typedef enum {
    HAL_VIDMODE_CBR,
    HAL_VIDMODE_VBR,
    HAL_VIDMODE_QP,
    HAL_VIDMODE_ABR,
    HAL_VIDMODE_AVBR,
    HAL_VIDMODE_END
} hal_vidmode;

typedef enum {
    HAL_VIDPROFILE_BASELINE,
    HAL_VIDPROFILE_MAIN,
    HAL_VIDPROFILE_HIGH
} hal_vidprofile;

typedef struct {
    unsigned char *data[8];
    unsigned int length[8];
    unsigned int seq;
    unsigned int timestamp;
    char channelCnt;
    char interleavedOn;
} hal_audframe;

typedef struct {
    int fileDesc;
    char enable;
    char mainLoop;
    hal_vidcodec payload;
} hal_chnstate;

typedef struct {
    unsigned short width, height;
} hal_dim;

typedef struct {
    hal_dim dim;
    void *data;
} hal_bitmap;

typedef struct {
    unsigned char *data;
    unsigned int length;
    unsigned int jpegSize;
} hal_jpegdata;

typedef struct {
    unsigned short x, y, width, height;
} hal_rect;

typedef struct {
    unsigned short width, height;
    hal_vidcodec codec;
    hal_vidmode mode;
    hal_vidprofile profile;
    unsigned char gop, framerate, minQual, maxQual;
    unsigned short bitrate, maxBitrate;
} hal_vidconfig;

typedef struct {
    unsigned int length;
    unsigned int offset;
    int type;
} hal_vidnalu;

typedef struct {
    unsigned char *data;
    unsigned int length;
    int naluCnt;
    hal_vidnalu nalu[8];
    unsigned int offset;
    unsigned long long timestamp;
} hal_vidpack;

typedef struct {
	hal_vidpack *pack;
	unsigned int count;
	unsigned int seq;
} hal_vidstream;