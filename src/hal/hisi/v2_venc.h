#pragma once

#include "v2_common.h"

#define V2_VENC_CHN_NUM 16

typedef enum {
    V2_VENC_CODEC_JPEG = 26,
    V2_VENC_CODEC_H264 = 96,
    V2_VENC_CODEC_H265 = 265,
    V2_VENC_CODEC_MJPG = 1002,
    V2_VENC_CODEC_END
} v2_venc_codec;

typedef enum {
    V2_VENC_NALU_H264_BSLICE,
    V2_VENC_NALU_H264_PSLICE,
    V2_VENC_NALU_H264_ISLICE,
    V2_VENC_NALU_H264_IDRSLICE = 5,
    V2_VENC_NALU_H264_SEI,
    V2_VENC_NALU_H264_SPS,
    V2_VENC_NALU_H264_PPS,
    V2_VENC_NALU_H264_END
} v2_venc_nalu_h264;

typedef enum {
    V2_VENC_NALU_H265_BSLICE,
    V2_VENC_NALU_H265_PSLICE,
    V2_VENC_NALU_H265_ISLICE,
    V2_VENC_NALU_H265_IDRSLICE = 19,
    V2_VENC_NALU_H265_VPS = 32,
    V2_VENC_NALU_H265_SPS,
    V2_VENC_NALU_H265_PPS,
    V2_VENC_NALU_H265_SEI = 39,
    V2_VENC_NALU_H265_END
} v2_venc_nalu_h265;

typedef enum {
    V2_VENC_NALU_MJPG_ECS = 5,
    V2_VENC_NALU_MJPG_APP,
    V2_VENC_NALU_MJPG_VDO,
    V2_VENC_NALU_MJPG_PIC,
    V2_VENC_NALU_MJPG_END
} v2_venc_nalu_mjpg;

typedef enum {
    V2_VENC_RATEMODE_H264CBR = 1,
    V2_VENC_RATEMODE_H264VBR,
    V2_VENC_RATEMODE_H264AVBR,
    V2_VENC_RATEMODE_H264ABR,
    V2_VENC_RATEMODE_H264QP,
    V2_VENC_RATEMODE_MJPGCBR,
    V2_VENC_RATEMODE_MJPGVBR,
    V2_VENC_RATEMODE_MJPGABR,
    V2_VENC_RATEMODE_MJPGQP,
    V2_VENC_RATEMODE_H265CBR = 14,
    V2_VENC_RATEMODE_H265VBR,
    V2_VENC_RATEMODE_H265AVBR,
    V2_VENC_RATEMODE_H265QP,
    V2_VENC_RATEMODE_END
} v2_venc_ratemode;

typedef struct {
    v2_common_dim maxPic;
    unsigned int bufSize;
    unsigned int profile;
    int byFrame;
    v2_common_dim pic;
    unsigned int bFrameNum;
    unsigned int refNum;
} v2_venc_attr_h26x;

typedef struct {
    v2_common_dim maxPic;
    unsigned int bufSize;
    int byFrame;
    v2_common_dim pic;
} v2_venc_attr_mjpg;

typedef struct {
    v2_common_dim maxPic;
    unsigned int bufSize;
    int byFrame;
    v2_common_dim pic;
    int dcfThumbs;
} v2_venc_attr_jpg;

typedef struct {
    v2_venc_codec codec;
    union {
        v2_venc_attr_h26x h264;
        v2_venc_attr_mjpg mjpg;
        v2_venc_attr_jpg  jpg;
        v2_venc_attr_h26x h265;
    };
} v2_venc_attrib;

typedef struct {
    unsigned int gop;
    unsigned int statTime;
    unsigned int srcFps;
    unsigned int dstFps;
    unsigned int bitrate;
    unsigned int avgLvl;
} v2_venc_rate_h26xcbr;

typedef struct {
    unsigned int gop;
    unsigned int statTime;
    unsigned int srcFps;
    unsigned int dstFps;
    unsigned int maxBitrate;
    unsigned int maxQual;
    unsigned int minQual;
} v2_venc_rate_h26xvbr;

typedef struct {
    unsigned int gop;
    unsigned int statTime;
    unsigned int srcFps;
    unsigned int dstFps;
    unsigned int maxBitrate;
} v2_venc_rate_h26xavbr;

typedef struct {
    unsigned int gop;
    unsigned int srcFps;
    unsigned int dstFps;
    unsigned int interQual;
    unsigned int predQual;
} v2_venc_rate_h26xqp;

typedef struct {
    unsigned int gop;
    unsigned int statTime;
    unsigned int srcFps;
    unsigned int dstFps;
    unsigned int avgBitrate;
    unsigned int maxBitrate;
} v2_venc_rate_h264abr;

typedef struct {
    unsigned int statTime;
    unsigned int srcFps;
    unsigned int dstFps;
    unsigned int bitrate;
    unsigned int avgLvl;
} v2_venc_rate_mjpgcbr;

typedef struct {
    unsigned int statTime;
    unsigned int srcFps;
    unsigned int dstFps;
    unsigned int maxBitrate;
    unsigned int maxQual;
    unsigned int minQual;
} v2_venc_rate_mjpgvbr;

typedef struct {
    unsigned int srcFps;
    unsigned int dstFps;
    unsigned int quality;
} v2_venc_rate_mjpgqp;
typedef struct {
    v2_venc_ratemode mode;
    union {
        v2_venc_rate_h26xcbr h264Cbr;
        v2_venc_rate_h26xvbr h264Vbr;
        v2_venc_rate_h26xavbr h264Avbr;
        v2_venc_rate_h26xqp h264Qp;
        v2_venc_rate_h264abr h264Abr;
        v2_venc_rate_mjpgcbr mjpgCbr;
        v2_venc_rate_mjpgqp mjpgQp;
        v2_venc_rate_mjpgvbr mjpgVbr;
        v2_venc_rate_h26xcbr h265Cbr;
        v2_venc_rate_h26xvbr h265Vbr;
        v2_venc_rate_h26xavbr h265Avbr;
        v2_venc_rate_h26xqp h265Qp;
    };
    void *extend;
} v2_venc_rate;

typedef struct {
    v2_venc_attrib attrib;
    v2_venc_rate rate;
} v2_venc_chn;

typedef struct {
    unsigned int quality;
    unsigned char qtLuma[64];
    unsigned char qtChromaBlue[64];
    unsigned char qtChromaRed[64];
    unsigned int mcuPerEcs;
} v2_venc_jpg;

typedef union {
    v2_venc_nalu_h264 h264Nalu;
    v2_venc_nalu_mjpg mjpgNalu;
    v2_venc_nalu_h265 h265Nalu;
} v2_venc_nalu;

typedef struct {
    v2_venc_nalu packType;
    unsigned int offset;
    unsigned int length;
} v2_venc_packinfo;

typedef struct {
    unsigned int addr;
    unsigned char *data;
    unsigned int length;
    unsigned long long timestamp;
    int endFrame;
    v2_venc_nalu naluType;
    unsigned int offset;
    unsigned int packNum;
    v2_venc_packinfo packetInfo[8];
} v2_venc_pack;

typedef struct {
    unsigned int leftPics;
    unsigned int leftBytes;
    unsigned int leftFrames;
    unsigned int curPacks;
    unsigned int leftRecvPics;
    unsigned int leftEncPics;
} v2_venc_stat;

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
    unsigned int startQual;
    int pSkipOn;
} v2_venc_strminfo_h264;

typedef struct {
    unsigned int size;
    unsigned int iCu64x64;
    unsigned int iCu32x32;
    unsigned int iCu16x16;
    unsigned int iCu8x8;
    unsigned int pCu32x32;
    unsigned int pCu16x16;
    unsigned int pCu8x8;
    unsigned int pCu4x4;
    unsigned int refType;
    unsigned int updAttrCnt;
    unsigned int startQual;
    int pSkipOn;
} v2_venc_strminfo_h265;

typedef struct {
    unsigned int size;
    unsigned int updAttrCnt;
    unsigned int quality;
} v2_venc_strminfo_mjpg;

typedef struct {
    v2_venc_pack *packet;
    unsigned int count;
    unsigned int sequence;
    union {
        v2_venc_strminfo_h264 h264Info;
        v2_venc_strminfo_mjpg mjpgInfo;
        v2_venc_strminfo_h265 h265Info;
    };
} v2_venc_strm;

typedef struct {
    void *handle;

    int (*fnCreateChannel)(int channel, v2_venc_chn *config);
    int (*fnGetChannelConfig)(int channel, v2_venc_chn *config);
    int (*fnDestroyChannel)(int channel);
    int (*fnResetChannel)(int channel);
    int (*fnSetChannelConfig)(int channel, v2_venc_chn *config);
    int (*fnSetColorToGray)(int channel, int *active);

    int (*fnFreeDescriptor)(int channel);
    int (*fnGetDescriptor)(int channel);

    int (*fnGetJpegParam)(int channel, v2_venc_jpg *param);
    int (*fnSetJpegParam)(int channel, v2_venc_jpg *param);

    int (*fnFreeStream)(int channel, v2_venc_strm *stream);
    int (*fnGetStream)(int channel, v2_venc_strm *stream, unsigned int timeout);

    int (*fnQuery)(int channel, v2_venc_stat* stats);

    int (*fnRequestIdr)(int channel, int instant);
    int (*fnStartReceiving)(int channel);
    int (*fnStartReceivingEx)(int channel, int *count);
    int (*fnStopReceiving)(int channel);
} v2_venc_impl;

static int v2_venc_load(v2_venc_impl *venc_lib) {
    if (!(venc_lib->handle = dlopen("libmpi.so", RTLD_LAZY | RTLD_GLOBAL)))
        HAL_ERROR("v2_venc", "Failed to load library!\nError: %s\n", dlerror());

    if (!(venc_lib->fnCreateChannel = (int(*)(int channel, v2_venc_chn *config))
        hal_symbol_load("v2_venc", venc_lib->handle, "HI_MPI_VENC_CreateChn")))
        return EXIT_FAILURE;

    if (!(venc_lib->fnDestroyChannel = (int(*)(int channel))
        hal_symbol_load("v2_venc", venc_lib->handle, "HI_MPI_VENC_DestroyChn")))
        return EXIT_FAILURE;

    if (!(venc_lib->fnGetChannelConfig = (int(*)(int channel, v2_venc_chn *config))
        hal_symbol_load("v2_venc", venc_lib->handle, "HI_MPI_VENC_GetChnAttr")))
        return EXIT_FAILURE;

    if (!(venc_lib->fnResetChannel = (int(*)(int channel))
        hal_symbol_load("v2_venc", venc_lib->handle, "HI_MPI_VENC_ResetChn")))
        return EXIT_FAILURE;

    if (!(venc_lib->fnSetChannelConfig = (int(*)(int channel, v2_venc_chn *config))
        hal_symbol_load("v2_venc", venc_lib->handle, "HI_MPI_VENC_SetChnAttr")))
        return EXIT_FAILURE;

    if (!(venc_lib->fnSetColorToGray = (int(*)(int channel, int *active))
        hal_symbol_load("v2_venc", venc_lib->handle, "HI_MPI_VENC_SetColor2Grey")))
        return EXIT_FAILURE;

    if (!(venc_lib->fnFreeDescriptor = (int(*)(int channel))
        hal_symbol_load("v2_venc", venc_lib->handle, "HI_MPI_VENC_CloseFd")))
        return EXIT_FAILURE;

    if (!(venc_lib->fnGetDescriptor = (int(*)(int channel))
        hal_symbol_load("v2_venc", venc_lib->handle, "HI_MPI_VENC_GetFd")))
        return EXIT_FAILURE;

    if (!(venc_lib->fnGetJpegParam = (int(*)(int channel, v2_venc_jpg *param))
        hal_symbol_load("v2_venc", venc_lib->handle, "HI_MPI_VENC_GetJpegParam")))
        return EXIT_FAILURE;

    if (!(venc_lib->fnSetJpegParam = (int(*)(int channel, v2_venc_jpg *param))
        hal_symbol_load("v2_venc", venc_lib->handle, "HI_MPI_VENC_SetJpegParam")))
        return EXIT_FAILURE;

    if (!(venc_lib->fnFreeStream = (int(*)(int channel, v2_venc_strm *stream))
        hal_symbol_load("v2_venc", venc_lib->handle, "HI_MPI_VENC_ReleaseStream")))
        return EXIT_FAILURE;

    if (!(venc_lib->fnGetStream = (int(*)(int channel, v2_venc_strm *stream, unsigned int timeout))
        hal_symbol_load("v2_venc", venc_lib->handle, "HI_MPI_VENC_GetStream")))
        return EXIT_FAILURE;

    if (!(venc_lib->fnQuery = (int(*)(int channel, v2_venc_stat *stats))
        hal_symbol_load("v2_venc", venc_lib->handle, "HI_MPI_VENC_Query")))
        return EXIT_FAILURE;

    if (!(venc_lib->fnRequestIdr = (int(*)(int channel, int instant))
        hal_symbol_load("v2_venc", venc_lib->handle, "HI_MPI_VENC_RequestIDR")))
        return EXIT_FAILURE;

    if (!(venc_lib->fnStartReceiving = (int(*)(int channel))
        hal_symbol_load("v2_venc", venc_lib->handle, "HI_MPI_VENC_StartRecvPic")))
        return EXIT_FAILURE;

    if (!(venc_lib->fnStartReceivingEx = (int(*)(int channel, int *count))
        hal_symbol_load("v2_venc", venc_lib->handle, "HI_MPI_VENC_StartRecvPicEx")))
        return EXIT_FAILURE;

    if (!(venc_lib->fnStopReceiving = (int(*)(int channel))
        hal_symbol_load("v2_venc", venc_lib->handle, "HI_MPI_VENC_StopRecvPic")))
        return EXIT_FAILURE;

    return EXIT_SUCCESS;
}

static void v2_venc_unload(v2_venc_impl *venc_lib) {
    if (venc_lib->handle) dlclose(venc_lib->handle);
    venc_lib->handle = NULL;
    memset(venc_lib, 0, sizeof(*venc_lib));
}