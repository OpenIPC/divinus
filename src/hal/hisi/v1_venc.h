#pragma once

#include "v1_common.h"

#define V1_VENC_CHN_NUM 16

typedef enum {
    V1_VENC_CODEC_JPEG = 26,
    V1_VENC_CODEC_H264 = 96,
    V1_VENC_CODEC_MJPG = 1002,
    V1_VENC_CODEC_END
} v1_venc_codec;

typedef enum {
    V1_VENC_NALU_H264_BSLICE,
    V1_VENC_NALU_H264_PSLICE,
    V1_VENC_NALU_H264_ISLICE,
    V1_VENC_NALU_H264_IDRSLICE = 5,
    V1_VENC_NALU_H264_SEI,
    V1_VENC_NALU_H264_SPS,
    V1_VENC_NALU_H264_PPS,
    V1_VENC_NALU_H264_END
} v1_venc_nalu_h264;

typedef enum {
    V1_VENC_NALU_MJPG_ECS = 5,
    V1_VENC_NALU_MJPG_APP,
    V1_VENC_NALU_MJPG_VDO,
    V1_VENC_NALU_MJPG_PIC,
    V1_VENC_NALU_MJPG_END
} v1_venc_nalu_mjpg;

typedef enum {
    V1_VENC_RATEMODE_H264CBR = 1,
    V1_VENC_RATEMODE_H264VBR,
    V1_VENC_RATEMODE_H264ABR,
    V1_VENC_RATEMODE_H264QP,
    V1_VENC_RATEMODE_MJPGCBR,
    V1_VENC_RATEMODE_MJPGVBR,
    V1_VENC_RATEMODE_MJPGABR,
    V1_VENC_RATEMODE_MJPGQP,
    V1_VENC_RATEMODE_H264CBRv2 = 13,
    V1_VENC_RATEMODE_H264VBRv2,
    V1_VENC_RATEMODE_END
} v1_venc_ratemode;

typedef struct {
    v1_common_dim maxPic;
    unsigned int bufSize;
    unsigned int profile;
    int byFrame;
    int fieldOn;
    int mainStrmOn;
    unsigned int priority;
    int fieldOrFrame;
    v1_common_dim pic;
} v1_venc_attr_h264;

typedef struct {
    v1_common_dim maxPic;
    unsigned int bufSize;
    int byFrame;
    int mainStrmOn;
    int fieldOrFrame;
    unsigned int priority;
    v1_common_dim pic;
} v1_venc_attr_mjpg;

typedef struct {
    v1_common_dim maxPic;
    unsigned int bufSize;
    int byFrame;
    int fieldOrFrame;
    unsigned int priority;
    v1_common_dim pic;
} v1_venc_attr_jpg;

typedef struct {
    v1_venc_codec codec;
    union {
        v1_venc_attr_h264 h264;
        v1_venc_attr_mjpg mjpg;
        v1_venc_attr_jpg  jpg;
    };
} v1_venc_attrib;

typedef struct {
    unsigned int gop;
    unsigned int statTime;
    unsigned int srcFps;
    unsigned int dstFps;
    unsigned int bitrate;
    unsigned int avgLvl;
} v1_venc_rate_h264cbr;

typedef struct {
    unsigned int gop;
    unsigned int statTime;
    unsigned int srcFps;
    unsigned int dstFps;
    unsigned int maxBitrate;
    unsigned int maxQual;
    unsigned int minQual;
} v1_venc_rate_h264vbr;

typedef struct {
    unsigned int gop;
    unsigned int srcFps;
    unsigned int dstFps;
    unsigned int interQual;
    unsigned int predQual;
} v1_venc_rate_h264qp;

typedef struct {
    unsigned int gop;
    unsigned int statTime;
    unsigned int srcFps;
    unsigned int dstFps;
    unsigned int avgBitrate;
    unsigned int maxBitrate;
} v1_venc_rate_h264abr;

typedef struct {
    unsigned int statTime;
    unsigned int srcFps;
    unsigned int dstFps;
    unsigned int bitrate;
    unsigned int avgLvl;
} v1_venc_rate_mjpgcbr;

typedef struct {
    unsigned int statTime;
    unsigned int srcFps;
    unsigned int dstFps;
    unsigned int maxBitrate;
    unsigned int maxQual;
    unsigned int minQual;
} v1_venc_rate_mjpgvbr;

typedef struct {
    unsigned int srcFps;
    unsigned int dstFps;
    unsigned int quality;
} v1_venc_rate_mjpgqp;

typedef struct {
    v1_venc_ratemode mode;
    union {
        v1_venc_rate_h264cbr h264Cbr;
        v1_venc_rate_h264vbr h264Vbr;
        v1_venc_rate_h264qp h264Qp;
        v1_venc_rate_h264abr h264Abr;
        v1_venc_rate_mjpgcbr mjpgCbr;
        v1_venc_rate_mjpgqp mjpgQp;
        v1_venc_rate_mjpgvbr mjpgVbr;
    };
    void *extend;
} v1_venc_rate;

typedef struct {
    v1_venc_attrib attrib;
    v1_venc_rate rate;
} v1_venc_chn;

typedef struct {
    unsigned int quality;
    unsigned char qtLuma[64];
    unsigned char qtChromaBlue[64];
    unsigned char qtChromaRed[64];
    unsigned int mcuPerEcs;
} v1_venc_jpg;

typedef union {
    v1_venc_nalu_h264 h264Nalu;
    v1_venc_nalu_mjpg mjpgNalu;
} v1_venc_nalu;

typedef struct {
    unsigned int addr[2];
    unsigned char *data[2];
    unsigned int length[2];
    unsigned long long timestamp;
    int endField;
    int endFrame;
    v1_venc_nalu naluType;
    unsigned int offset;
} v1_venc_pack;

typedef struct {
    unsigned int leftPics;
    unsigned int leftBytes;
    unsigned int leftFrames;
    unsigned int curPacks;
    unsigned int leftRecvPics;
    unsigned int leftEncPics;
} v1_venc_stat;

typedef struct {
    unsigned int size;
    unsigned int pSkip;
    unsigned int ipcmMb;
    unsigned int iMb16x8;
    unsigned int iMb16x16;
    unsigned int iMb8x16;
    unsigned int iMb8x8;
    unsigned int pMb16;
    unsigned int pMb8;
    unsigned int pMb4;
    unsigned int refSliceType;
    unsigned int refType;
    unsigned int updAttrCnt;
} v1_venc_strminfo_h264;

typedef struct {
    unsigned int size;
    unsigned int updAttrCnt;
    unsigned int quality;
} v1_venc_strminfo_mjpg;

typedef struct {
    v1_venc_pack *packet;
    unsigned int count;
    unsigned int sequence;
    union {
        v1_venc_strminfo_h264 h264Info;
        v1_venc_strminfo_mjpg mjpgInfo;
    };
} v1_venc_strm;

typedef struct {
    void *handle;

    int (*fnCreateGroup)(int group);
    int (*fnDestroyGroup)(int group);
    int (*fnRegisterChannel)(int group, int channel);
    int (*fnUnregisterChannel)(int channel);

    int (*fnCreateChannel)(int channel, v1_venc_chn *config);
    int (*fnGetChannelConfig)(int channel, v1_venc_chn *config);
    int (*fnDestroyChannel)(int channel);
    int (*fnSetChannelConfig)(int channel, v1_venc_chn *config);
    int (*fnSetColorToGray)(int channel, int *active);

    int (*fnGetDescriptor)(int channel);

    int (*fnGetJpegParam)(int channel, v1_venc_jpg *param);
    int (*fnSetJpegParam)(int channel, v1_venc_jpg *param);

    int (*fnFreeStream)(int channel, v1_venc_strm *stream);
    int (*fnGetStream)(int channel, v1_venc_strm *stream, int blockingOn);

    int (*fnQuery)(int channel, v1_venc_stat* stats);

    int (*fnRequestIdr)(int channel, int instant);
    int (*fnStartReceiving)(int channel);
    int (*fnStartReceivingEx)(int channel, int *count);
    int (*fnStopReceiving)(int channel);
} v1_venc_impl;

static int v1_venc_load(v1_venc_impl *venc_lib) {
    if (!(venc_lib->handle = dlopen("libmpi.so", RTLD_LAZY | RTLD_GLOBAL)))
        HAL_ERROR("v1_venc", "Failed to load library!\nError: %s\n", dlerror());

    if (!(venc_lib->fnCreateGroup = (int(*)(int group))
        hal_symbol_load("v1_venc", venc_lib->handle, "HI_MPI_VENC_CreateGroup")))
        return EXIT_FAILURE;

    if (!(venc_lib->fnDestroyGroup = (int(*)(int group))
        hal_symbol_load("v1_venc", venc_lib->handle, "HI_MPI_VENC_DestroyGroup")))
        return EXIT_FAILURE;

    if (!(venc_lib->fnRegisterChannel = (int(*)(int group, int channel))
        hal_symbol_load("v1_venc", venc_lib->handle, "HI_MPI_VENC_RegisterChn")))
        return EXIT_FAILURE;

    if (!(venc_lib->fnUnregisterChannel = (int(*)(int channel))
        hal_symbol_load("v1_venc", venc_lib->handle, "HI_MPI_VENC_UnRegisterChn")))
        return EXIT_FAILURE;

    if (!(venc_lib->fnCreateChannel = (int(*)(int channel, v1_venc_chn *config))
        hal_symbol_load("v1_venc", venc_lib->handle, "HI_MPI_VENC_CreateChn")))
        return EXIT_FAILURE;

    if (!(venc_lib->fnDestroyChannel = (int(*)(int channel))
        hal_symbol_load("v1_venc", venc_lib->handle, "HI_MPI_VENC_DestroyChn")))
        return EXIT_FAILURE;

    if (!(venc_lib->fnGetChannelConfig = (int(*)(int channel, v1_venc_chn *config))
        hal_symbol_load("v1_venc", venc_lib->handle, "HI_MPI_VENC_GetChnAttr")))
        return EXIT_FAILURE;

    if (!(venc_lib->fnSetChannelConfig = (int(*)(int channel, v1_venc_chn *config))
        hal_symbol_load("v1_venc", venc_lib->handle, "HI_MPI_VENC_SetChnAttr")))
        return EXIT_FAILURE;

    if (!(venc_lib->fnSetColorToGray = (int(*)(int channel, int *active))
        hal_symbol_load("v1_venc", venc_lib->handle, "HI_MPI_VENC_SetColor2GreyConf")))
        return EXIT_FAILURE;

    if (!(venc_lib->fnGetDescriptor = (int(*)(int channel))
        hal_symbol_load("v1_venc", venc_lib->handle, "HI_MPI_VENC_GetFd")))
        return EXIT_FAILURE;

    if (!(venc_lib->fnGetJpegParam = (int(*)(int channel, v1_venc_jpg *param))
        hal_symbol_load("v1_venc", venc_lib->handle, "HI_MPI_VENC_GetJpegParam")))
        return EXIT_FAILURE;

    if (!(venc_lib->fnSetJpegParam = (int(*)(int channel, v1_venc_jpg *param))
        hal_symbol_load("v1_venc", venc_lib->handle, "HI_MPI_VENC_SetJpegParam")))
        return EXIT_FAILURE;

    if (!(venc_lib->fnFreeStream = (int(*)(int channel, v1_venc_strm *stream))
        hal_symbol_load("v1_venc", venc_lib->handle, "HI_MPI_VENC_ReleaseStream")))
        return EXIT_FAILURE;

    if (!(venc_lib->fnGetStream = (int(*)(int channel, v1_venc_strm *stream, int blockingOn))
        hal_symbol_load("v1_venc", venc_lib->handle, "HI_MPI_VENC_GetStream")))
        return EXIT_FAILURE;

    if (!(venc_lib->fnQuery = (int(*)(int channel, v1_venc_stat *stats))
        hal_symbol_load("v1_venc", venc_lib->handle, "HI_MPI_VENC_Query")))
        return EXIT_FAILURE;

    if (!(venc_lib->fnRequestIdr = (int(*)(int channel, int instant))
        hal_symbol_load("v1_venc", venc_lib->handle, "HI_MPI_VENC_RequestIDR")))
        return EXIT_FAILURE;

    if (!(venc_lib->fnStartReceiving = (int(*)(int channel))
        hal_symbol_load("v1_venc", venc_lib->handle, "HI_MPI_VENC_StartRecvPic")))
        return EXIT_FAILURE;

    if (!(venc_lib->fnStartReceivingEx = (int(*)(int channel, int *count))
        hal_symbol_load("v1_venc", venc_lib->handle, "HI_MPI_VENC_StartRecvPicEx")))
        return EXIT_FAILURE;

    if (!(venc_lib->fnStopReceiving = (int(*)(int channel))
        hal_symbol_load("v1_venc", venc_lib->handle, "HI_MPI_VENC_StopRecvPic")))
        return EXIT_FAILURE;

    return EXIT_SUCCESS;
}

static void v1_venc_unload(v1_venc_impl *venc_lib) {
    if (venc_lib->handle) dlclose(venc_lib->handle);
    venc_lib->handle = NULL;
    memset(venc_lib, 0, sizeof(*venc_lib));
}
