#pragma once

#include "cvi_common.h"

#define CVI_VENC_CHN_NUM 16

typedef enum {
    CVI_VENC_CODEC_JPEG = 26,
    CVI_VENC_CODEC_H264 = 96,
    CVI_VENC_CODEC_H265 = 265,
    CVI_VENC_CODEC_MJPG = 1002,
    CVI_VENC_CODEC_END
} cvi_venc_codec;

typedef enum {
    CVI_VENC_GOPMODE_NORMALP,
    CVI_VENC_GOPMODE_DUALP,
    CVI_VENC_GOPMODE_SMARTP,
    CVI_VENC_GOPMODE_ADVSMARTP,
    CVI_VENC_GOPMODE_BIPREDB,
    CVI_VENC_GOPMODE_LOWDELAYB,
    CVI_VENC_GOPMODE_END
} cvi_venc_gopmode;

typedef enum {
    CVI_VENC_NALU_H264_BSLICE,
    CVI_VENC_NALU_H264_PSLICE,
    CVI_VENC_NALU_H264_ISLICE,
    CVI_VENC_NALU_H264_IDRSLICE = 5,
    CVI_VENC_NALU_H264_SEI,
    CVI_VENC_NALU_H264_SPS,
    CVI_VENC_NALU_H264_PPS,
    CVI_VENC_NALU_H264_END
} cvi_venc_nalu_h264;

typedef enum {
    CVI_VENC_NALU_H265_BSLICE,
    CVI_VENC_NALU_H265_PSLICE,
    CVI_VENC_NALU_H265_ISLICE,
    CVI_VENC_NALU_H265_IDRSLICE = 19,
    CVI_VENC_NALU_H265_VPS = 32,
    CVI_VENC_NALU_H265_SPS,
    CVI_VENC_NALU_H265_PPS,
    CVI_VENC_NALU_H265_SEI = 39,
    CVI_VENC_NALU_H265_END
} cvi_venc_nalu_h265;

typedef enum {
    CVI_VENC_NALU_MJPG_ECS = 5,
    CVI_VENC_NALU_MJPG_APP,
    CVI_VENC_NALU_MJPG_VDO,
    CVI_VENC_NALU_MJPG_PIC,
    CVI_VENC_NALU_MJPG_DCF,
    CVI_VENC_NALU_MJPG_DCFPIC,
    CVI_VENC_NALU_MJPG_END
} cvi_venc_nalu_mjpg;

typedef enum {
    CVI_VENC_RATEMODE_H264CBR = 1,
    CVI_VENC_RATEMODE_H264VBR,
    CVI_VENC_RATEMODE_H264AVBR,
    CVI_VENC_RATEMODE_H264QVBR,
    CVI_VENC_RATEMODE_H264QP,
    CVI_VENC_RATEMODE_H264QPMAP,
    CVI_VENC_RATEMODE_H264UBR,
    CVI_VENC_RATEMODE_MJPGCBR,
    CVI_VENC_RATEMODE_MJPGVBR,
    CVI_VENC_RATEMODE_MJPGQP,
    CVI_VENC_RATEMODE_H265CBR,
    CVI_VENC_RATEMODE_H265VBR,
    CVI_VENC_RATEMODE_H265AVBR,
    CVI_VENC_RATEMODE_H265QVBR,
    CVI_VENC_RATEMODE_H265QP,
    CVI_VENC_RATEMODE_H265QPMAP,
    CVI_VENC_RATEMODE_H265UBR,
    CVI_VENC_RATEMODE_END
} cvi_venc_ratemode;

typedef struct {
    char rcnRefShareBufOn;
    char singleBufLumaOn;
} cvi_venc_attr_h264;

typedef struct {
    char rcnRefShareBufOn;
} cvi_venc_attr_h265;

typedef struct {
    char dcfThumbs;
    unsigned char numThumbs;
    cvi_common_dim sizeThumbs[2];
    int multiReceiveOn;
    int shit;
} cvi_venc_attr_jpg;

typedef struct {
    cvi_venc_codec codec;
    cvi_common_dim maxPic;
    unsigned int bufSize;
    unsigned int profile;
    char byFrame;
    cvi_common_dim pic;
    char singleCoreOn;
    char esBufQueueOn;
    char isolateFrmOn;
    union {
        cvi_venc_attr_h264 h264;
        cvi_venc_attr_h265 h265;
        cvi_venc_attr_jpg  jpg;
        int prores[3];
    };
} cvi_venc_attrib;

typedef struct {
    unsigned int gop;
    unsigned int statTime;
    unsigned int srcFps;
    unsigned int dstFps;
    unsigned int maxBitrate;
    char variFpsOn;
} cvi_venc_rate_h26xbr;

typedef struct {
    unsigned int gop;
    unsigned int srcFps;
    unsigned int dstFps;
    unsigned int interQual;
    unsigned int predQual;
    unsigned int bipredQual;
    char variFpsOn;
} cvi_venc_rate_h26xqp;

typedef struct {
    unsigned int gop;
    unsigned int statTime;
    unsigned int srcFps;
    unsigned int dstFps;
    char variFpsOn;
} cvi_venc_rate_h264qpmap;

typedef struct {
    unsigned int gop;
    unsigned int statTime;
    unsigned int srcFps;
    unsigned int dstFps;
    // Accepts values from 0-2 (mean QP, min QP, max QP)
    int qpMapMode;
    char variFpsOn;
} cvi_venc_rate_h265qpmap;

typedef struct {
    unsigned int statTime;
    unsigned int srcFps;
    unsigned int dstFps;
    unsigned int maxBitrate;
    char variFpsOn;
} cvi_venc_rate_mjpgbr;

typedef struct {
    unsigned int srcFps;
    unsigned int dstFps;
    unsigned int quality;
    char variFpsOn;
} cvi_venc_rate_mjpgqp;

typedef struct {
    cvi_venc_ratemode mode;
    union {
        cvi_venc_rate_h26xbr h264Cbr;
        cvi_venc_rate_h26xbr h264Vbr;
        cvi_venc_rate_h26xbr h264Avbr;
        cvi_venc_rate_h26xbr h264Qvbr;
        cvi_venc_rate_h26xqp h264Qp;
        cvi_venc_rate_h264qpmap h264QpMap;
        cvi_venc_rate_h26xbr h264Ubr;
        cvi_venc_rate_mjpgbr mjpgCbr;
        cvi_venc_rate_mjpgbr mjpgVbr;
        cvi_venc_rate_mjpgqp mjpgQp;
        cvi_venc_rate_h26xbr h265Cbr;
        cvi_venc_rate_h26xbr h265Vbr;
        cvi_venc_rate_h26xbr h265Avbr;
        cvi_venc_rate_h26xbr h265Qvbr;
        cvi_venc_rate_h26xqp h265Qp;
        cvi_venc_rate_h265qpmap h265QpMap;
        cvi_venc_rate_h26xbr h265Ubr;
    };
} cvi_venc_rate;

typedef struct {
    int ipQualDelta;
} cvi_venc_gop_normalp;

typedef struct {
    unsigned int spInterv;
    int spQualDelta;
    int ipQualDelta;
} cvi_venc_gop_dualp;

typedef struct {
    unsigned int bgInterv;
    int bgQualDelta;
    int viQualDelta;
} cvi_venc_gop_smartp;

typedef struct {
    unsigned int bFrameNum;
    int bgQualDelta;
    int ipQualDelta;
} cvi_venc_gop_bipredb;

typedef struct {
    cvi_venc_gopmode mode;
    union {
        cvi_venc_gop_normalp normalP;
        cvi_venc_gop_dualp dualP;
        cvi_venc_gop_smartp smartP;
        cvi_venc_gop_smartp advSmartP;
        cvi_venc_gop_bipredb bipredB;
    };
} cvi_venc_gop;

typedef struct {
    cvi_venc_attrib attrib;
    cvi_venc_rate rate;
    cvi_venc_gop gop;
} cvi_venc_chn;

typedef struct {
    unsigned int quality;
    unsigned char qtLuma[64];
    unsigned char qtChromaBlue[64];
    unsigned char qtChromaRed[64];
    unsigned int mcuPerEcs;
} cvi_venc_jpg;

typedef union {
    cvi_venc_nalu_h264 h264Nalu;
    cvi_venc_nalu_mjpg mjpgNalu;
    cvi_venc_nalu_h265 h265Nalu;
    int proresNalu;
} cvi_venc_nalu;

typedef struct {
    cvi_venc_nalu packType;
    unsigned int offset;
    unsigned int length;
} cvi_venc_packinfo;

typedef struct {
    unsigned long long addr;
    unsigned char __attribute__((aligned (4)))*data;
    unsigned int __attribute__((aligned (4)))length;
    unsigned long long timestamp;
    char endFrame;
    cvi_venc_nalu naluType;
    unsigned int offset;
    unsigned int packNum;
    cvi_venc_packinfo packetInfo[8];
} cvi_venc_pack;

typedef struct {
    char grayscaleOn;
    unsigned int prio;
    unsigned int maxStrmCnt;
    unsigned int wakeFrmCnt;
    char cropOn;
    cvi_common_rect crop;
    int srcFps;
    int dstFps;
} cvi_venc_para;

typedef struct {
    unsigned int leftPics;
    unsigned int leftBytes;
    unsigned int leftFrames;
    unsigned int curPacks;
    unsigned int leftRecvPics;
    unsigned int leftEncPics;
    char jpegSnapEnd;
    char strmInfo[53];
} cvi_venc_stat;

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
    char pSkip;
} cvi_venc_strminfo_h264;

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
    char pSkip;
} cvi_venc_strminfo_h265;

typedef struct {
    unsigned int size;
    unsigned int updAttrCnt;
    unsigned int quality;
} cvi_venc_strminfo_mjpg;

typedef struct {
    unsigned int size;
    unsigned int updAttrCnt;
} cvi_venc_strminfo_pres;

typedef struct {
    char sseOn;
    unsigned int sseVal;
} cvi_venc_sseinfo;

typedef struct {
    unsigned int residualBitNum;
    unsigned int headBitNum;
    unsigned int madiVal;
    unsigned int madpVal;
    double psnrVal;
    unsigned int mseLcuCnt;
    unsigned int mseSum;
    cvi_venc_sseinfo sseInfo[8];
    unsigned int qualHstgrm[52];
    unsigned int moveSceneNum;
    unsigned int moveSceneBits;
} cvi_venc_strmadvinfo_h26x;

typedef struct {
    unsigned int reserved;
} cvi_venc_strmadvinfo_mjpg;

typedef struct {
    unsigned int reserved;
} cvi_venc_strmadvinfo_pres;

typedef struct {
    cvi_venc_pack __attribute__((aligned(4)))*packet;
    unsigned int __attribute__((aligned(4)))count;
    unsigned int sequence;
    union {
        cvi_venc_strminfo_h264 h264Info;
        cvi_venc_strminfo_mjpg mjpgInfo;
        cvi_venc_strminfo_h265 h265Info;
        cvi_venc_strminfo_pres proresInfo;
    };
    union {
        cvi_venc_strmadvinfo_h26x h264aInfo;
        cvi_venc_strmadvinfo_mjpg mjpgaInfo;
        cvi_venc_strmadvinfo_h26x h265aInfo;
        cvi_venc_strmadvinfo_pres proresaInfo;
    };
} cvi_venc_strm;

typedef struct {
    void *handle;

    int (*fnCreateChannel)(int channel, cvi_venc_chn *config);
    int (*fnGetChannelConfig)(int channel, cvi_venc_chn *config);
    int (*fnGetChannelParam)(int channel, cvi_venc_para *config);
    int (*fnDestroyChannel)(int channel);
    int (*fnResetChannel)(int channel);
    int (*fnSetChannelConfig)(int channel, cvi_venc_chn *config);
    int (*fnSetChannelParam)(int channel, cvi_venc_para *config);
    int (*fnSetColorToGray)(int channel, int *active);

    int (*fnFreeDescriptor)(int channel);
    int (*fnGetDescriptor)(int channel);

    int (*fnGetJpegParam)(int channel, cvi_venc_jpg *param);
    int (*fnSetJpegParam)(int channel, cvi_venc_jpg *param);

    int (*fnFreeStream)(int channel, cvi_venc_strm *stream);
    int (*fnGetStream)(int channel, cvi_venc_strm *stream, int millis);

    int (*fnQuery)(int channel, cvi_venc_stat* stats);

    int (*fnRequestIdr)(int channel, char instant);
    int (*fnStartReceivingEx)(int channel, int *count);
    int (*fnStopReceiving)(int channel);
} cvi_venc_impl;

static int cvi_venc_load(cvi_venc_impl *venc_lib) {
    if (!(venc_lib->handle = dlopen("libvenc.so", RTLD_LAZY | RTLD_GLOBAL)))
        HAL_ERROR("cvi_venc", "Failed to load library!\nError: %s\n", dlerror());

    if (!(venc_lib->fnCreateChannel = (int(*)(int channel, cvi_venc_chn *config))
        hal_symbol_load("cvi_venc", venc_lib->handle, "CVI_VENC_CreateChn")))
        return EXIT_FAILURE;

    if (!(venc_lib->fnDestroyChannel = (int(*)(int channel))
        hal_symbol_load("cvi_venc", venc_lib->handle, "CVI_VENC_DestroyChn")))

    if (!(venc_lib->fnGetChannelConfig = (int(*)(int channel, cvi_venc_chn *config))
        hal_symbol_load("cvi_venc", venc_lib->handle, "CVI_VENC_GetChnAttr")))
        return EXIT_FAILURE;

    if (!(venc_lib->fnGetChannelParam = (int(*)(int channel, cvi_venc_para *config))
        hal_symbol_load("cvi_venc", venc_lib->handle, "CVI_VENC_GetChnParam")))
        return EXIT_FAILURE;

    if (!(venc_lib->fnResetChannel = (int(*)(int channel))
        hal_symbol_load("cvi_venc", venc_lib->handle, "CVI_VENC_ResetChn")))
        return EXIT_FAILURE;

    if (!(venc_lib->fnSetChannelConfig = (int(*)(int channel, cvi_venc_chn *config))
        hal_symbol_load("cvi_venc", venc_lib->handle, "CVI_VENC_SetChnAttr")))
        return EXIT_FAILURE;

    if (!(venc_lib->fnSetChannelParam = (int(*)(int channel, cvi_venc_para *config))
        hal_symbol_load("cvi_venc", venc_lib->handle, "CVI_VENC_SetChnParam")))
        return EXIT_FAILURE;

    if (!(venc_lib->fnFreeDescriptor = (int(*)(int channel))
        hal_symbol_load("cvi_venc", venc_lib->handle, "CVI_VENC_CloseFd")))
        return EXIT_FAILURE;

    if (!(venc_lib->fnGetDescriptor = (int(*)(int channel))
        hal_symbol_load("cvi_venc", venc_lib->handle, "CVI_VENC_GetFd")))
        return EXIT_FAILURE;

    if (!(venc_lib->fnGetJpegParam = (int(*)(int channel, cvi_venc_jpg *param))
        hal_symbol_load("cvi_venc", venc_lib->handle, "CVI_VENC_GetJpegParam")))
        return EXIT_FAILURE;

    if (!(venc_lib->fnSetJpegParam = (int(*)(int channel, cvi_venc_jpg *param))
        hal_symbol_load("cvi_venc", venc_lib->handle, "CVI_VENC_SetJpegParam")))
        return EXIT_FAILURE;

    if (!(venc_lib->fnFreeStream = (int(*)(int channel, cvi_venc_strm *stream))
        hal_symbol_load("cvi_venc", venc_lib->handle, "CVI_VENC_ReleaseStream")))
        return EXIT_FAILURE;

    if (!(venc_lib->fnGetStream = (int(*)(int channel, cvi_venc_strm *stream, int millis))
        hal_symbol_load("cvi_venc", venc_lib->handle, "CVI_VENC_GetStream")))
        return EXIT_FAILURE;

    if (!(venc_lib->fnQuery = (int(*)(int channel, cvi_venc_stat *stats))
        hal_symbol_load("cvi_venc", venc_lib->handle, "CVI_VENC_QueryStatus")))
        return EXIT_FAILURE;

    if (!(venc_lib->fnRequestIdr = (int(*)(int channel, char instant))
        hal_symbol_load("cvi_venc", venc_lib->handle, "CVI_VENC_RequestIDR")))
        return EXIT_FAILURE;

    if (!(venc_lib->fnStartReceivingEx = (int(*)(int channel, int *count))
        hal_symbol_load("cvi_venc", venc_lib->handle, "CVI_VENC_StartRecvFrame")))
        return EXIT_FAILURE;

    if (!(venc_lib->fnStopReceiving = (int(*)(int channel))
        hal_symbol_load("cvi_venc", venc_lib->handle, "CVI_VENC_StopRecvFrame")))
        return EXIT_FAILURE;

    return EXIT_SUCCESS;
}

static void cvi_venc_unload(cvi_venc_impl *venc_lib) {
    if (venc_lib->handle) dlclose(venc_lib->handle);
    venc_lib->handle = NULL;
    memset(venc_lib, 0, sizeof(*venc_lib));
}