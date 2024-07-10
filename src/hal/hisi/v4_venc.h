#pragma once

#include "v4_common.h"

#define V4_VENC_CHN_NUM 16

typedef enum {
    V4_VENC_CODEC_JPEG = 11,
    V4_VENC_CODEC_H264 = 96,
    V4_VENC_CODEC_H265 = 265,
    V4_VENC_CODEC_MJPG = 1002,
    V4_VENC_CODEC_END
} v4_venc_codec;

typedef enum {
    V4_VENC_GOPMODE_NORMALP,
    V4_VENC_GOPMODE_DUALP,
    V4_VENC_GOPMODE_SMARTP,
    V4_VENC_GOPMODE_ADVSMARTP,
    V4_VENC_GOPMODE_BIPREDB,
    V4_VENC_GOPMODE_LOWDELAYB,
    V4_VENC_GOPMODE_END
} v4_venc_gopmode;

typedef enum {
    V4_VENC_NALU_H264_BSLICE,
    V4_VENC_NALU_H264_PSLICE,
    V4_VENC_NALU_H264_ISLICE,
    V4_VENC_NALU_H264_IDRSLICE = 5,
    V4_VENC_NALU_H264_SEI,
    V4_VENC_NALU_H264_SPS,
    V4_VENC_NALU_H264_PPS,
    V4_VENC_NALU_H264_END
} v4_venc_nalu_h264;

typedef enum {
    V4_VENC_NALU_H265_BSLICE,
    V4_VENC_NALU_H265_PSLICE,
    V4_VENC_NALU_H265_ISLICE,
    V4_VENC_NALU_H265_IDRSLICE = 19,
    V4_VENC_NALU_H265_VPS = 32,
    V4_VENC_NALU_H265_SPS,
    V4_VENC_NALU_H265_PPS,
    V4_VENC_NALU_H265_SEI = 39,
    V4_VENC_NALU_H265_END
} v4_venc_nalu_h265;

typedef enum {
    V4_VENC_NALU_MJPG_ECS = 5,
    V4_VENC_NALU_MJPG_APP,
    V4_VENC_NALU_MJPG_VDO,
    V4_VENC_NALU_MJPG_PIC,
    V4_VENC_NALU_MJPG_DCF,
    V4_VENC_NALU_MJPG_DCFPIC,
    V4_VENC_NALU_MJPG_END
} v4_venc_nalu_mjpg;

typedef enum {
    V4_VENC_RATEMODE_H264CBR = 1,
    V4_VENC_RATEMODE_H264VBR,
    V4_VENC_RATEMODE_H264AVBR,
    V4_VENC_RATEMODE_H264QVBR,
    V4_VENC_RATEMODE_H264CVBR,
    V4_VENC_RATEMODE_H264QP,
    V4_VENC_RATEMODE_H264QPMAP,
    V4_VENC_RATEMODE_MJPGCBR,
    V4_VENC_RATEMODE_MJPGVBR,
    V4_VENC_RATEMODE_MJPGQP,
    V4_VENC_RATEMODE_H265CBR,
    V4_VENC_RATEMODE_H265VBR,
    V4_VENC_RATEMODE_H265AVBR,
    V4_VENC_RATEMODE_H265QVBR,
    V4_VENC_RATEMODE_H265CVBR,
    V4_VENC_RATEMODE_H265QP,
    V4_VENC_RATEMODE_H265QPMAP,
    V4_VENC_RATEMODE_END
} v4_venc_ratemode;

typedef struct {
    int rcnRefShareBufOn;
} v4_venc_attr_h26x;

typedef struct {

} v4_venc_attr_mjpg;

typedef struct {
    int dcfThumbs;
    unsigned char numThumbs;
    v4_common_dim sizeThumbs[2];
    int multiReceiveOn;
} v4_venc_attr_jpg;

typedef struct {
    v4_venc_codec codec;
    v4_common_dim maxPic;
    unsigned int bufSize;
    unsigned int profile;
    int byFrame;
    v4_common_dim pic;
    union {
        v4_venc_attr_h26x h264;
        v4_venc_attr_h26x h265;
        v4_venc_attr_mjpg  mjpg;
        v4_venc_attr_jpg  jpg;
        int prores[3];
    };
} v4_venc_attrib;

typedef struct {
    unsigned int gop;
    unsigned int statTime;
    unsigned int srcFps;
    unsigned int dstFps;
    unsigned int maxBitrate;
} v4_venc_rate_h26xbr;

typedef struct {
    unsigned int gop;
    unsigned int statTime;
    unsigned int srcFps;
    unsigned int dstFps;
    unsigned int maxBitrate;
    unsigned int shortTermStatTime;
    unsigned int longTermStatTime;
    unsigned int longTermMaxBitrate;
    unsigned int longTermMinBitrate;
} v4_venc_rate_h26xcvbr;

typedef struct {
    unsigned int gop;
    unsigned int srcFps;
    unsigned int dstFps;
    unsigned int interQual;
    unsigned int predQual;
    unsigned int bipredQual;
} v4_venc_rate_h26xqp;

typedef struct {
    unsigned int gop;
    unsigned int statTime;
    unsigned int srcFps;
    unsigned int dstFps;
} v4_venc_rate_h264qpmap;

typedef struct {
    unsigned int gop;
    unsigned int statTime;
    unsigned int srcFps;
    unsigned int dstFps;
    // Accepts values from 0-2 (mean QP, min QP, max QP)
    int qpMapMode;
} v4_venc_rate_h265qpmap;

typedef struct {
    unsigned int statTime;
    unsigned int srcFps;
    unsigned int dstFps;
    unsigned int maxBitrate;
} v4_venc_rate_mjpgbr;

typedef struct {
    unsigned int srcFps;
    unsigned int dstFps;
    unsigned int quality;
} v4_venc_rate_mjpgqp;

typedef struct {
    v4_venc_ratemode mode;
    union {
        v4_venc_rate_h26xbr h264Cbr;
        v4_venc_rate_h26xbr h264Vbr;
        v4_venc_rate_h26xbr h264Avbr;
        v4_venc_rate_h26xbr h264Qvbr;
        v4_venc_rate_h26xcvbr h264Cvbr;
        v4_venc_rate_h26xqp h264Qp;
        v4_venc_rate_h264qpmap h264QpMap;
        v4_venc_rate_mjpgbr mjpgCbr;
        v4_venc_rate_mjpgbr mjpgVbr;
        v4_venc_rate_mjpgqp mjpgQp;
        v4_venc_rate_h26xbr h265Cbr;
        v4_venc_rate_h26xbr h265Vbr;
        v4_venc_rate_h26xbr h265Avbr;
        v4_venc_rate_h26xbr h265Qvbr;
        v4_venc_rate_h26xcvbr h265Cvbr;
        v4_venc_rate_h26xqp h265Qp;
        v4_venc_rate_h265qpmap h265QpMap;
    };
} v4_venc_rate;

typedef struct {
    int ipQualDelta;
} v4_venc_gop_normalp;

typedef struct {
    unsigned int spInterv;
    int spQualDelta;
    int ipQualDelta;
} v4_venc_gop_dualp;

typedef struct {
    unsigned int bgInterv;
    int bgQualDelta;
    int viQualDelta;
} v4_venc_gop_smartp;

typedef struct {
    unsigned int bFrameNum;
    int bgQualDelta;
    int ipQualDelta;
} v4_venc_gop_bipredb;

typedef struct {
    v4_venc_gopmode mode;
    union {
        v4_venc_gop_normalp normalP;
        v4_venc_gop_dualp dualP;
        v4_venc_gop_smartp smartP;
        v4_venc_gop_smartp advSmartP;
        v4_venc_gop_bipredb bipredB;
    };
} v4_venc_gop;

typedef struct {
    v4_venc_attrib attrib;
    v4_venc_rate rate;
    v4_venc_gop gop;
} v4_venc_chn;

typedef struct {
    unsigned int quality;
    unsigned char qtLuma[64];
    unsigned char qtChromaBlue[64];
    unsigned char qtChromaRed[64];
    unsigned int mcuPerEcs;
} v4_venc_jpg;

typedef union {
    v4_venc_nalu_h264 h264Nalu;
    v4_venc_nalu_mjpg mjpgNalu;
    v4_venc_nalu_h265 h265Nalu;
    int proresNalu;
} v4_venc_nalu;

typedef struct {
    v4_venc_nalu packType;
    unsigned int offset;
    unsigned int length;
} v4_venc_packinfo;

typedef struct {
    unsigned long long addr;
    unsigned char __attribute__((aligned (4)))*data;
    unsigned int __attribute__((aligned (4)))length;
    unsigned long long timestamp;
    int endFrame;
    v4_venc_nalu naluType;
    unsigned int offset;
    unsigned int packNum;
    v4_venc_packinfo packetInfo[8];
} v4_venc_pack;

typedef struct {
    int grayscaleOn;
    unsigned int prio;
    unsigned int maxStrmCnt;
    unsigned int wakeFrmCnt;
    int cropOn;
    v4_common_rect crop;
    int srcFps;
    int dstFps;
} v4_venc_para;

typedef struct {
    unsigned int leftPics;
    unsigned int leftBytes;
    unsigned int leftFrames;
    unsigned int curPacks;
    unsigned int leftRecvPics;
    unsigned int leftEncPics;
    int jpegSnapEnd;
    int strmInfo[14];
} v4_venc_stat;

typedef struct {
    unsigned int size;
    unsigned int iMb16x16;
    unsigned int iMb8x8;
    unsigned int pMb16;
    unsigned int pMb8;
    unsigned int pMb4;
    unsigned int refType;
    unsigned int updAttrCnt;
    unsigned int startQual;
    unsigned int meanQual;
    int pSkip;
} v4_venc_strminfo_h264;

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
} v4_venc_strminfo_h265;

typedef struct {
    unsigned int size;
    unsigned int updAttrCnt;
    unsigned int quality;
} v4_venc_strminfo_mjpg;

typedef struct {
    unsigned int size;
    unsigned int updAttrCnt;
} v4_venc_strminfo_pres;

typedef struct {
    int sseOn;
    unsigned int sseVal;
} v4_venc_sseinfo;

typedef struct {
    unsigned int residualBitNum;
    unsigned int headBitNum;
    unsigned int madiVal;
    unsigned int madpVal;
    double psnrVal;
    unsigned int mseLcuCnt;
    unsigned int mseSum;
    v4_venc_sseinfo sseInfo[8];
    unsigned int qualHstgrm[52];
    unsigned int moveSceneNum;
    unsigned int moveSceneBits;
} v4_venc_strmadvinfo_h26x;

typedef struct {
    unsigned int reserved;
} v4_venc_strmadvinfo_mjpg;

typedef struct {

} v4_venc_strmadvinfo_pres;

typedef struct {
    v4_venc_pack __attribute__((aligned(4)))*packet;
    unsigned int __attribute__((aligned(4)))count;
    unsigned int sequence;
    union {
        v4_venc_strminfo_h264 h264Info;
        v4_venc_strminfo_mjpg mjpgInfo;
        v4_venc_strminfo_h265 h265Info;
        v4_venc_strminfo_pres proresInfo;
    };
    union {
        v4_venc_strmadvinfo_h26x h264aInfo;
        v4_venc_strmadvinfo_mjpg mjpgaInfo;
        v4_venc_strmadvinfo_h26x h265aInfo;
        v4_venc_strmadvinfo_pres proresaInfo;
    };
} v4_venc_strm;

typedef struct {
    void *handle, *handleGoke;

    int (*fnCreateChannel)(int channel, v4_venc_chn *config);
    int (*fnGetChannelConfig)(int channel, v4_venc_chn *config);
    int (*fnGetChannelParam)(int channel, v4_venc_para *config);
    int (*fnDestroyChannel)(int channel);
    int (*fnResetChannel)(int channel);
    int (*fnSetChannelConfig)(int channel, v4_venc_chn *config);
    int (*fnSetChannelParam)(int channel, v4_venc_para *config);
    int (*fnSetColorToGray)(int channel, int *active);

    int (*fnFreeDescriptor)(int channel);
    int (*fnGetDescriptor)(int channel);

    int (*fnGetJpegParam)(int channel, v4_venc_jpg *param);
    int (*fnSetJpegParam)(int channel, v4_venc_jpg *param);

    int (*fnFreeStream)(int channel, v4_venc_strm *stream);
    int (*fnGetStream)(int channel, v4_venc_strm *stream, unsigned int timeout);

    int (*fnQuery)(int channel, v4_venc_stat* stats);

    int (*fnRequestIdr)(int channel, int instant);
    int (*fnStartReceivingEx)(int channel, int *count);
    int (*fnStopReceiving)(int channel);
} v4_venc_impl;

static int v4_venc_load(v4_venc_impl *venc_lib) {
    if ( !(venc_lib->handle = dlopen("libmpi.so", RTLD_LAZY | RTLD_GLOBAL)) &&

        (!(venc_lib->handleGoke = dlopen("libgk_api.so", RTLD_LAZY | RTLD_GLOBAL)) ||
         !(venc_lib->handle = dlopen("libhi_mpi.so", RTLD_LAZY | RTLD_GLOBAL))))
        HAL_ERROR("v4_venc", "Failed to load library!\nError: %s\n", dlerror());

    if (!(venc_lib->fnCreateChannel = (int(*)(int channel, v4_venc_chn *config))
        hal_symbol_load("v4_venc", venc_lib->handle, "HI_MPI_VENC_CreateChn")))
        return EXIT_FAILURE;

    if (!(venc_lib->fnDestroyChannel = (int(*)(int channel))
        hal_symbol_load("v4_venc", venc_lib->handle, "HI_MPI_VENC_DestroyChn")))

    if (!(venc_lib->fnGetChannelConfig = (int(*)(int channel, v4_venc_chn *config))
        hal_symbol_load("v4_venc", venc_lib->handle, "HI_MPI_VENC_GetChnAttr")))
        return EXIT_FAILURE;

    if (!(venc_lib->fnGetChannelParam = (int(*)(int channel, v4_venc_para *config))
        hal_symbol_load("v4_venc", venc_lib->handle, "HI_MPI_VENC_GetChnParam")))
        return EXIT_FAILURE;

    if (!(venc_lib->fnResetChannel = (int(*)(int channel))
        hal_symbol_load("v4_venc", venc_lib->handle, "HI_MPI_VENC_ResetChn")))
        return EXIT_FAILURE;

    if (!(venc_lib->fnSetChannelConfig = (int(*)(int channel, v4_venc_chn *config))
        hal_symbol_load("v4_venc", venc_lib->handle, "HI_MPI_VENC_SetChnAttr")))
        return EXIT_FAILURE;

    if (!(venc_lib->fnSetChannelParam = (int(*)(int channel, v4_venc_para *config))
        hal_symbol_load("v4_venc", venc_lib->handle, "HI_MPI_VENC_SetChnParam")))
        return EXIT_FAILURE;

    if (!(venc_lib->fnFreeDescriptor = (int(*)(int channel))
        hal_symbol_load("v4_venc", venc_lib->handle, "HI_MPI_VENC_CloseFd")))
        return EXIT_FAILURE;

    if (!(venc_lib->fnGetDescriptor = (int(*)(int channel))
        hal_symbol_load("v4_venc", venc_lib->handle, "HI_MPI_VENC_GetFd")))
        return EXIT_FAILURE;

    if (!(venc_lib->fnGetJpegParam = (int(*)(int channel, v4_venc_jpg *param))
        hal_symbol_load("v4_venc", venc_lib->handle, "HI_MPI_VENC_GetJpegParam")))
        return EXIT_FAILURE;

    if (!(venc_lib->fnSetJpegParam = (int(*)(int channel, v4_venc_jpg *param))
        hal_symbol_load("v4_venc", venc_lib->handle, "HI_MPI_VENC_SetJpegParam")))
        return EXIT_FAILURE;

    if (!(venc_lib->fnFreeStream = (int(*)(int channel, v4_venc_strm *stream))
        hal_symbol_load("v4_venc", venc_lib->handle, "HI_MPI_VENC_ReleaseStream")))
        return EXIT_FAILURE;

    if (!(venc_lib->fnGetStream = (int(*)(int channel, v4_venc_strm *stream, unsigned int timeout))
        hal_symbol_load("v4_venc", venc_lib->handle, "HI_MPI_VENC_GetStream")))
        return EXIT_FAILURE;

    if (!(venc_lib->fnQuery = (int(*)(int channel, v4_venc_stat *stats))
        hal_symbol_load("v4_venc", venc_lib->handle, "HI_MPI_VENC_QueryStatus")))
        return EXIT_FAILURE;

    if (!(venc_lib->fnRequestIdr = (int(*)(int channel, int instant))
        hal_symbol_load("v4_venc", venc_lib->handle, "HI_MPI_VENC_RequestIDR")))
        return EXIT_FAILURE;

    if (!(venc_lib->fnStartReceivingEx = (int(*)(int channel, int *count))
        hal_symbol_load("v4_venc", venc_lib->handle, "HI_MPI_VENC_StartRecvFrame")))
        return EXIT_FAILURE;

    if (!(venc_lib->fnStopReceiving = (int(*)(int channel))
        hal_symbol_load("v4_venc", venc_lib->handle, "HI_MPI_VENC_StopRecvFrame")))
        return EXIT_FAILURE;

    return EXIT_SUCCESS;
}

static void v4_venc_unload(v4_venc_impl *venc_lib) {
    if (venc_lib->handle) dlclose(venc_lib->handle);
    venc_lib->handle = NULL;
    if (venc_lib->handleGoke) dlclose(venc_lib->handleGoke);
    venc_lib->handleGoke = NULL;
    memset(venc_lib, 0, sizeof(*venc_lib));
}