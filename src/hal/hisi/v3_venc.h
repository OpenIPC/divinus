#pragma once

#include "v3_common.h"

#define V3_VENC_CHN_NUM 16

typedef enum {
    V3_VENC_CODEC_JPEG = 26,
    V3_VENC_CODEC_H264 = 96,
    V3_VENC_CODEC_H265 = 265,
    V3_VENC_CODEC_MJPG = 1002,
    V3_VENC_CODEC_END
} v3_venc_codec;

typedef enum {
    V3_VENC_GOPMODE_NORMALP,
    V3_VENC_GOPMODE_DUALP,
    V3_VENC_GOPMODE_SMARTP,
    V3_VENC_GOPMODE_BIPREDB,
    V3_VENC_GOPMODE_LOWDELAYB,
    V3_VENC_GOPMODE_END
} v3_venc_gopmode;

typedef enum {
    V3_VENC_NALU_H264_BSLICE,
    V3_VENC_NALU_H264_PSLICE,
    V3_VENC_NALU_H264_ISLICE,
    V3_VENC_NALU_H264_IDRSLICE = 5,
    V3_VENC_NALU_H264_SEI,
    V3_VENC_NALU_H264_SPS,
    V3_VENC_NALU_H264_PPS,
    V3_VENC_NALU_H264_END
} v3_venc_nalu_h264;

typedef enum {
    V3_VENC_NALU_H265_BSLICE,
    V3_VENC_NALU_H265_PSLICE,
    V3_VENC_NALU_H265_ISLICE,
    V3_VENC_NALU_H265_IDRSLICE = 19,
    V3_VENC_NALU_H265_VPS = 32,
    V3_VENC_NALU_H265_SPS,
    V3_VENC_NALU_H265_PPS,
    V3_VENC_NALU_H265_SEI = 39,
    V3_VENC_NALU_H265_END
} v3_venc_nalu_h265;

typedef enum {
    V3_VENC_NALU_MJPG_ECS = 5,
    V3_VENC_NALU_MJPG_APP,
    V3_VENC_NALU_MJPG_VDO,
    V3_VENC_NALU_MJPG_PIC,
    V3_VENC_NALU_MJPG_END
} v3_venc_nalu_mjpg;

typedef enum {
    V3_VENC_RATEMODE_H264CBR = 1,
    V3_VENC_RATEMODE_H264VBR,
    V3_VENC_RATEMODE_H264AVBR,
    V3_VENC_RATEMODE_H264QVBR,
    V3_VENC_RATEMODE_H264QP,
    V3_VENC_RATEMODE_H264QPMAP,
    V3_VENC_RATEMODE_MJPGCBR,
    V3_VENC_RATEMODE_MJPGVBR,
    V3_VENC_RATEMODE_MJPGQP,
    V3_VENC_RATEMODE_H265CBR,
    V3_VENC_RATEMODE_H265VBR,
    V3_VENC_RATEMODE_H265AVBR,
    V3_VENC_RATEMODE_H265QVBR,
    V3_VENC_RATEMODE_H265QP,
    V3_VENC_RATEMODE_H265QPMAP,
    V3_VENC_RATEMODE_END
} v3_venc_ratemode;

typedef struct {
    v3_common_dim maxPic;
    unsigned int bufSize;
    unsigned int profile;
    int byFrame;
    v3_common_dim pic;
} v3_venc_attr_h26x;

typedef struct {
    v3_common_dim maxPic;
    unsigned int bufSize;
    int byFrame;
    v3_common_dim pic;
} v3_venc_attr_mjpg;

typedef struct {
    v3_common_dim maxPic;
    unsigned int bufSize;
    int byFrame;
    v3_common_dim pic;
    int dcfThumbs;
} v3_venc_attr_jpg;

typedef struct {
    v3_venc_codec codec;
    union {
        v3_venc_attr_h26x h264;
        v3_venc_attr_mjpg mjpg;
        v3_venc_attr_jpg  jpg;
        v3_venc_attr_h26x h265;
    };
} v3_venc_attrib;

typedef struct {
    unsigned int gop;
    unsigned int statTime;
    unsigned int srcFps;
    unsigned int dstFps;
    unsigned int bitrate;
    unsigned int avgLvl;
} v3_venc_rate_h26xcbr;

typedef struct {
    unsigned int gop;
    unsigned int statTime;
    unsigned int srcFps;
    unsigned int dstFps;
    unsigned int maxBitrate;
    unsigned int maxQual;
    unsigned int minQual;
    unsigned int minIQual;
} v3_venc_rate_h26xvbr;

typedef struct {
    unsigned int gop;
    unsigned int statTime;
    unsigned int srcFps;
    unsigned int dstFps;
    unsigned int bitrate;
} v3_venc_rate_h26xxvbr;

typedef struct {
    unsigned int gop;
    unsigned int srcFps;
    unsigned int dstFps;
    unsigned int interQual;
    unsigned int predQual;
    unsigned int bipredQual;
} v3_venc_rate_h26xqp;

typedef struct {
    unsigned int gop;
    unsigned int statTime;
    unsigned int srcFps;
    unsigned int dstFps;
    // Accepts values from 0-2 (mean QP, min QP, max QP)
    unsigned int qpMapMode;
    unsigned int predQual;
    unsigned int bipredQual;
} v3_venc_rate_h26xqpmap;

typedef struct {
    unsigned int statTime;
    unsigned int srcFps;
    unsigned int dstFps;
    unsigned int bitrate;
    unsigned int avgLvl;
} v3_venc_rate_mjpgcbr;

typedef struct {
    unsigned int statTime;
    unsigned int srcFps;
    unsigned int dstFps;
    unsigned int maxBitrate;
    unsigned int maxQual;
    unsigned int minQual;
} v3_venc_rate_mjpgvbr;

typedef struct {
    unsigned int srcFps;
    unsigned int dstFps;
    unsigned int quality;
} v3_venc_rate_mjpgqp;
typedef struct {
    v3_venc_ratemode mode;
    union {
        v3_venc_rate_h26xcbr h264Cbr;
        v3_venc_rate_h26xvbr h264Vbr;
        v3_venc_rate_h26xxvbr h264Avbr;
        v3_venc_rate_h26xxvbr h264Qvbr;
        v3_venc_rate_h26xqp h264Qp;
        v3_venc_rate_h26xqpmap h264QpMap;
        v3_venc_rate_mjpgcbr mjpgCbr;
        v3_venc_rate_mjpgvbr mjpgVbr;
        v3_venc_rate_mjpgqp mjpgQp;
        v3_venc_rate_h26xcbr h265Cbr;
        v3_venc_rate_h26xvbr h265Vbr;
        v3_venc_rate_h26xxvbr h265Avbr;
        v3_venc_rate_h26xxvbr h265Qvbr;
        v3_venc_rate_h26xqp h265Qp;
        v3_venc_rate_h26xqpmap h265QpMap;
    };
    void *extend;
} v3_venc_rate;

typedef struct {
    int ipQualDelta;
} v3_venc_gop_normalp;

typedef struct {
    unsigned int spInterv;
    int spQualDelta;
    int ipQualDelta;
} v3_venc_gop_dualp;

typedef struct {
    unsigned int bgInterv;
    int bgQualDelta;
    int viQualDelta;
} v3_venc_gop_smartp;

typedef struct {
    unsigned int bFrameNum;
    int bgQualDelta;
    int ipQualDelta;
} v3_venc_gop_bipredb;

typedef struct {
    v3_venc_gopmode mode;
    union {
        v3_venc_gop_normalp normalP;
        v3_venc_gop_dualp dualP;
        v3_venc_gop_smartp smartP;
        v3_venc_gop_bipredb bipredB;
    };
} v3_venc_gop;

typedef struct {
    v3_venc_attrib attrib;
    v3_venc_rate rate;
    v3_venc_gop gop;
} v3_venc_chn;

typedef struct {
    unsigned int quality;
    unsigned char qtLuma[64];
    unsigned char qtChromaBlue[64];
    unsigned char qtChromaRed[64];
    unsigned int mcuPerEcs;
} v3_venc_jpg;

typedef union {
    v3_venc_nalu_h264 h264Nalu;
    v3_venc_nalu_mjpg mjpgNalu;
    v3_venc_nalu_h265 h265Nalu;
} v3_venc_nalu;

typedef struct {
    v3_venc_nalu packType;
    unsigned int offset;
    unsigned int length;
} v3_venc_packinfo;

typedef struct {
    unsigned int addr;
    unsigned char *data;
    unsigned int length;
    unsigned long long timestamp;
    int endFrame;
    v3_venc_nalu naluType;
    unsigned int offset;
    unsigned int packNum;
    v3_venc_packinfo packetInfo[8];
} v3_venc_pack;

typedef struct {
    unsigned int leftPics;
    unsigned int leftBytes;
    unsigned int leftFrames;
    unsigned int curPacks;
    unsigned int leftRecvPics;
    unsigned int leftEncPics;
} v3_venc_stat;

typedef struct {
    unsigned int size;
    unsigned int iMb16x16;
    unsigned int iMb8x8;
    unsigned int pMb16;
    unsigned int pMb8;
    unsigned int pMb4;
    unsigned int refSliceType;
    unsigned int refType;
    unsigned int updAttrCnt;
    unsigned int startQual;
    unsigned int meanQual;
    int pSkip;
} v3_venc_strminfo_h264;

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
    unsigned int meanQual;
    int pSkip;
} v3_venc_strminfo_h265;

typedef struct {
    unsigned int size;
    unsigned int updAttrCnt;
    unsigned int quality;
} v3_venc_strminfo_mjpg;

typedef struct {
    int sseOn;
    unsigned int sseVal;
} v3_venc_sseinfo;

typedef struct {
    unsigned int residualBitNum;
    unsigned int headBitNum;
    unsigned int madiVal;
    unsigned int madpVal;
    double psnrVal;
    unsigned int mseLcuCnt;
    unsigned int mseSum;
    v3_venc_sseinfo sseInfo[8];
    unsigned int qualHstgrm[52];
} v3_venc_strmadvinfo_h26x;

typedef struct {
    unsigned int reserved;
} v3_venc_strmadvinfo_mjpg;

typedef struct {
    v3_venc_pack *packet;
    unsigned int count;
    unsigned int sequence;
    union {
        v3_venc_strminfo_h264 h264Info;
        v3_venc_strminfo_mjpg mjpgInfo;
        v3_venc_strminfo_h265 h265Info;
    };
    union {
        v3_venc_strmadvinfo_h26x h264aInfo;
        v3_venc_strmadvinfo_mjpg mjpgaInfo;
        v3_venc_strmadvinfo_h26x h265aInfo;
    };
} v3_venc_strm;

typedef struct {
    void *handle;

    int (*fnCreateChannel)(int channel, v3_venc_chn *config);
    int (*fnGetChannelConfig)(int channel, v3_venc_chn *config);
    int (*fnDestroyChannel)(int channel);
    int (*fnResetChannel)(int channel);
    int (*fnSetChannelConfig)(int channel, v3_venc_chn *config);
    int (*fnSetColorToGray)(int channel, int *active);

    int (*fnFreeDescriptor)(int channel);
    int (*fnGetDescriptor)(int channel);

    int (*fnGetJpegParam)(int channel, v3_venc_jpg *param);
    int (*fnSetJpegParam)(int channel, v3_venc_jpg *param);

    int (*fnFreeStream)(int channel, v3_venc_strm *stream);
    int (*fnGetStream)(int channel, v3_venc_strm *stream, unsigned int timeout);

    int (*fnQuery)(int channel, v3_venc_stat* stats);

    int (*fnRequestIdr)(int channel, int instant);
    int (*fnStartReceiving)(int channel);
    int (*fnStartReceivingEx)(int channel, int *count);
    int (*fnStopReceiving)(int channel);
} v3_venc_impl;

static int v3_venc_load(v3_venc_impl *venc_lib) {
    if (!(venc_lib->handle = dlopen("libmpi.so", RTLD_LAZY | RTLD_GLOBAL)))
        HAL_ERROR("v3_venc", "Failed to load library!\nError: %s\n", dlerror());

    if (!(venc_lib->fnCreateChannel = (int(*)(int channel, v3_venc_chn *config))
        hal_symbol_load("v3_venc", venc_lib->handle, "HI_MPI_VENC_CreateChn")))
        return EXIT_FAILURE;

    if (!(venc_lib->fnDestroyChannel = (int(*)(int channel))
        hal_symbol_load("v3_venc", venc_lib->handle, "HI_MPI_VENC_DestroyChn")))
        return EXIT_FAILURE;

    if (!(venc_lib->fnGetChannelConfig = (int(*)(int channel, v3_venc_chn *config))
        hal_symbol_load("v3_venc", venc_lib->handle, "HI_MPI_VENC_GetChnAttr")))
        return EXIT_FAILURE;

    if (!(venc_lib->fnResetChannel = (int(*)(int channel))
        hal_symbol_load("v3_venc", venc_lib->handle, "HI_MPI_VENC_ResetChn")))
        return EXIT_FAILURE;

    if (!(venc_lib->fnSetChannelConfig = (int(*)(int channel, v3_venc_chn *config))
        hal_symbol_load("v3_venc", venc_lib->handle, "HI_MPI_VENC_SetChnAttr")))
        return EXIT_FAILURE;

    if (!(venc_lib->fnSetColorToGray = (int(*)(int channel, int *active))
        hal_symbol_load("v3_venc", venc_lib->handle, "HI_MPI_VENC_SetColor2Grey")))
        return EXIT_FAILURE;

    if (!(venc_lib->fnFreeDescriptor = (int(*)(int channel))
        hal_symbol_load("v3_venc", venc_lib->handle, "HI_MPI_VENC_CloseFd")))
        return EXIT_FAILURE;

    if (!(venc_lib->fnGetDescriptor = (int(*)(int channel))
        hal_symbol_load("v3_venc", venc_lib->handle, "HI_MPI_VENC_GetFd")))
        return EXIT_FAILURE;

    if (!(venc_lib->fnGetJpegParam = (int(*)(int channel, v3_venc_jpg *param))
        hal_symbol_load("v3_venc", venc_lib->handle, "HI_MPI_VENC_GetJpegParam")))
        return EXIT_FAILURE;

    if (!(venc_lib->fnSetJpegParam = (int(*)(int channel, v3_venc_jpg *param))
        hal_symbol_load("v3_venc", venc_lib->handle, "HI_MPI_VENC_SetJpegParam")))
        return EXIT_FAILURE;

    if (!(venc_lib->fnFreeStream = (int(*)(int channel, v3_venc_strm *stream))
        hal_symbol_load("v3_venc", venc_lib->handle, "HI_MPI_VENC_ReleaseStream")))
        return EXIT_FAILURE;

    if (!(venc_lib->fnGetStream = (int(*)(int channel, v3_venc_strm *stream, unsigned int timeout))
        hal_symbol_load("v3_venc", venc_lib->handle, "HI_MPI_VENC_GetStream")))
        return EXIT_FAILURE;

    if (!(venc_lib->fnQuery = (int(*)(int channel, v3_venc_stat *stats))
        hal_symbol_load("v3_venc", venc_lib->handle, "HI_MPI_VENC_Query")))
        return EXIT_FAILURE;

    if (!(venc_lib->fnRequestIdr = (int(*)(int channel, int instant))
        hal_symbol_load("v3_venc", venc_lib->handle, "HI_MPI_VENC_RequestIDR")))
        return EXIT_FAILURE;

    if (!(venc_lib->fnStartReceiving = (int(*)(int channel))
        hal_symbol_load("v3_venc", venc_lib->handle, "HI_MPI_VENC_StartRecvPic")))
        return EXIT_FAILURE;

    if (!(venc_lib->fnStartReceivingEx = (int(*)(int channel, int *count))
        hal_symbol_load("v3_venc", venc_lib->handle, "HI_MPI_VENC_StartRecvPicEx")))
        return EXIT_FAILURE;

    if (!(venc_lib->fnStopReceiving = (int(*)(int channel))
        hal_symbol_load("v3_venc", venc_lib->handle, "HI_MPI_VENC_StopRecvPic")))
        return EXIT_FAILURE;

    return EXIT_SUCCESS;
}

static void v3_venc_unload(v3_venc_impl *venc_lib) {
    if (venc_lib->handle) dlclose(venc_lib->handle);
    venc_lib->handle = NULL;
    memset(venc_lib, 0, sizeof(*venc_lib));
}