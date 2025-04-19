#pragma once

#include "rk_common.h"

#define RK_VENC_CHN_NUM 16

typedef enum {
    RK_VENC_CODEC_JPEG = 15,
    RK_VENC_CODEC_H264 = 8,
    RK_VENC_CODEC_H265 = 12,
    RK_VENC_CODEC_MJPG = 9,
    RK_VENC_CODEC_END
} rk_venc_codec;

typedef enum {
    RK_VENC_GOPMODE_INIT,
    RK_VENC_GOPMODE_NORMALP,
    RK_VENC_GOPMODE_TSVC2,
    RK_VENC_GOPMODE_TSVC3,
    RK_VENC_GOPMODE_TSVC4,
    RK_VENC_GOPMODE_SMARTP,
    RK_VENC_GOPMODE_END
} rk_venc_gopmode;

typedef enum {
    RK_VENC_NALU_H264_BSLICE,
    RK_VENC_NALU_H264_PSLICE,
    RK_VENC_NALU_H264_ISLICE,
    RK_VENC_NALU_H264_IDRSLICE = 5,
    RK_VENC_NALU_H264_SEI,
    RK_VENC_NALU_H264_SPS,
    RK_VENC_NALU_H264_PPS,
    RK_VENC_NALU_H264_END
} rk_venc_nalu_h264;

typedef enum {
    RK_VENC_NALU_H265_BSLICE,
    RK_VENC_NALU_H265_PSLICE,
    RK_VENC_NALU_H265_ISLICE,
    RK_VENC_NALU_H265_IDRSLICE = 19,
    RK_VENC_NALU_H265_VPS = 32,
    RK_VENC_NALU_H265_SPS,
    RK_VENC_NALU_H265_PPS,
    RK_VENC_NALU_H265_SEI = 39,
    RK_VENC_NALU_H265_END
} rk_venc_nalu_h265;

typedef enum {
    RK_VENC_NALU_MJPG_ECS = 5,
    RK_VENC_NALU_MJPG_APP,
    RK_VENC_NALU_MJPG_VDO,
    RK_VENC_NALU_MJPG_PIC,
    RK_VENC_NALU_MJPG_DCF,
    RK_VENC_NALU_MJPG_DCFPIC,
    RK_VENC_NALU_MJPG_END
} rk_venc_nalu_mjpg;

typedef enum {
    RK_VENC_RATEMODE_H264CBR = 1,
    RK_VENC_RATEMODE_H264VBR,
    RK_VENC_RATEMODE_H264AVBR,
    RK_VENC_RATEMODE_H264QP,
    RK_VENC_RATEMODE_MJPGCBR,
    RK_VENC_RATEMODE_MJPGVBR,
    RK_VENC_RATEMODE_MJPGQP,
    RK_VENC_RATEMODE_H265CBR,
    RK_VENC_RATEMODE_H265VBR,
    RK_VENC_RATEMODE_H265AVBR,
    RK_VENC_RATEMODE_H265QP,
    RK_VENC_RATEMODE_END
} rk_venc_ratemode;

typedef struct {
    int enable;
    // Accepts values from 128 to height
    unsigned int bufLine;
    unsigned int bufSize;
} rk_venc_buf;

typedef struct {
    unsigned int level;
} rk_venc_attr_h264;

typedef struct {

} rk_venc_attr_unused;

typedef struct {
    int dcfThumbs;
    unsigned char numThumbs;
    rk_common_dim sizeThumbs[2];
    int multiReceiveOn;
} rk_venc_attr_jpg;

typedef struct {
    rk_venc_codec codec;
    rk_common_dim maxPic;
    rk_common_pixfmt pixFmt;
    rk_common_mirr mirror;
    unsigned int bufSize;
    unsigned int profile;
    int byFrame;
    rk_common_dim pic;
    // Takes the stride dimensions (vir = (pic+15) & ~15)
    rk_common_dim vir;
    unsigned int strmBufCnt;
    union {
        rk_venc_attr_h264 h264;
        rk_venc_attr_unused h265;
        rk_venc_attr_unused mjpg;
        rk_venc_attr_jpg jpg;
    };
} rk_venc_attrib;

typedef struct {
    unsigned int gop;
    unsigned int srcFpsNum, srcFpsDen;
    unsigned int dstFpsNum, dstFpsDen;
    unsigned int bitrate;
    unsigned int statTime;
} rk_venc_rate_h26xcbr;

typedef struct {
    unsigned int gop;
    unsigned int srcFpsNum, srcFpsDen;
    unsigned int dstFpsNum, dstFpsDen;
    unsigned int bitrate;
    unsigned int maxBitrate;
    unsigned int minBitrate;
    unsigned int statTime;
} rk_venc_rate_h26xvbr;

typedef struct {
    unsigned int gop;
    unsigned int srcFpsNum, srcFpsDen;
    unsigned int dstFpsNum, dstFpsDen;
    // Following fields accept values from 1 to 51
    unsigned int interQual;
    unsigned int predQual;
    unsigned int bipredQual;
} rk_venc_rate_h26xqp;

typedef struct {
    unsigned int srcFpsNum, srcFpsDen;
    unsigned int dstFpsNum, dstFpsDen;
    unsigned int bitrate;
    unsigned int statTime;
} rk_venc_rate_mjpgcbr;

typedef struct {
    unsigned int srcFpsNum, srcFpsDen;
    unsigned int dstFpsNum, dstFpsDen;
    unsigned int bitrate;
    unsigned int maxBitrate;
    unsigned int minBitrate;
    unsigned int statTime;
} rk_venc_rate_mjpgvbr;

typedef struct {
    unsigned int srcFpsNum, srcFpsDen;
    unsigned int dstFpsNum, dstFpsDen;
    // Following field accept values from 1 to 99
    unsigned int quality;
} rk_venc_rate_mjpgqp;

typedef struct {
    rk_venc_ratemode mode;
    union {
        rk_venc_rate_h26xcbr h264Cbr;
        rk_venc_rate_h26xvbr h264Vbr;
        rk_venc_rate_h26xvbr h264Avbr;
        rk_venc_rate_h26xqp h264Qp;
        rk_venc_rate_mjpgcbr mjpgCbr;
        rk_venc_rate_mjpgvbr mjpgVbr;
        rk_venc_rate_mjpgqp mjpgQp;
        rk_venc_rate_h26xcbr h265Cbr;
        rk_venc_rate_h26xvbr h265Vbr;
        rk_venc_rate_h26xvbr h265Avbr;
        rk_venc_rate_h26xqp h265Qp;
    };
} rk_venc_rate;

typedef struct {
    int ipQualDelta;
} rk_venc_gop_normalp;

typedef struct {
    rk_venc_gopmode mode;
    // Virtual IDR frame length for SmartP
    int virIdrLen;
    // Set to 0 to save buffer when NormalP is used
    unsigned int maxLtrCnt;
    // Set to 0 to save buffer when SmartP is used
    unsigned int tsvcPreload;
} rk_venc_gop;

typedef struct {
    rk_venc_attrib attrib;
    rk_venc_rate rate;
    rk_venc_gop gop;
} rk_venc_chn;

typedef struct {
    unsigned int quality;
    unsigned char qtLuma[64];
    unsigned char qtChromaBlue[64];
    unsigned char qtChromaRed[64];
    unsigned int mcuPerEcs;
} rk_venc_jpg;

typedef union {
    rk_venc_nalu_h264 h264Nalu;
    rk_venc_nalu_mjpg mjpgNalu;
    rk_venc_nalu_h265 h265Nalu;
    int proresNalu;
} rk_venc_nalu;

typedef struct {
    rk_venc_nalu packType;
    unsigned int offset;
    unsigned int length;
} rk_venc_packinfo;

typedef struct {
    void* __attribute__((aligned (4)))mbBlk;
    unsigned int __attribute__((aligned (4)))length;
    unsigned long long timestamp;
    int endFrame;
    int endStrm;
    rk_venc_nalu naluType;
    unsigned int offset;
    unsigned int packNum;
    rk_venc_packinfo packetInfo[8];
} rk_venc_pack;

typedef struct {
    int grayscaleOn;
    unsigned int prio;
    unsigned int maxStrmCnt;
    unsigned int wakeFrmCnt;
    int cropOn;
    rk_common_rect crop;
    // Source and destination, numbers must be multiples of two
    // and scaling ratio must not be more than 16
    rk_common_rect scale[2];
    int frateOn;
    int srcFpsNum, srcFpsDen;
    int dstFpsNum, dstFpsDen;
} rk_venc_para;

typedef struct {
    unsigned int leftPics;
    unsigned int leftBytes;
    unsigned int leftFrames;
    unsigned int curPacks;
    unsigned int leftRecvPics;
    unsigned int leftEncPics;
    int jpegSnapEnd;
    int strmInfo[14];
} rk_venc_stat;

typedef struct {
    unsigned int size;
    unsigned int iMb16x16;
    unsigned int iMb8x8;
    unsigned int pMb16;
    unsigned int pMb8;
    unsigned int pMb4;
    int refType;
    unsigned int updAttrCnt;
    unsigned int startQual;
    unsigned int meanQual;
    int pSkip;
} rk_venc_strminfo_h264;

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
} rk_venc_strminfo_h265;

typedef struct {
    unsigned int size;
    unsigned int updAttrCnt;
    unsigned int quality;
} rk_venc_strminfo_jpeg;

typedef struct {
    unsigned int size;
    unsigned int updAttrCnt;
} rk_venc_strminfo_pres;

typedef struct {
    int sseOn;
    unsigned int sseVal;
} rk_venc_sseinfo;

typedef struct {
    unsigned int residualBitNum;
    unsigned int headBitNum;
    unsigned int madiVal;
    unsigned int madpVal;
    double psnrVal;
    unsigned int mseLcuCnt;
    unsigned int mseSum;
    rk_venc_sseinfo sseInfo[8];
    unsigned int qualHstgrm[52];
    unsigned int moveSceneNum;
    unsigned int moveSceneBits;
} rk_venc_strmadvinfo_h26x;

typedef struct {

} rk_venc_strmadvinfo_jpeg;

typedef struct {

} rk_venc_strmadvinfo_pres;

typedef struct {
    rk_venc_pack __attribute__((aligned(4)))*packet;
    unsigned int __attribute__((aligned(4)))count;
    unsigned int sequence;
    union {
        rk_venc_strminfo_h264 h264Info;
        rk_venc_strminfo_jpeg jpegInfo;
        rk_venc_strminfo_h265 h265Info;
        rk_venc_strminfo_pres proresInfo;
    };
    union {
        rk_venc_strmadvinfo_h26x h264aInfo;
        rk_venc_strmadvinfo_jpeg mjpgaInfo;
        rk_venc_strmadvinfo_h26x h265aInfo;
        rk_venc_strmadvinfo_pres proresaInfo;
    };
} rk_venc_strm;

typedef struct {
    void *handle;

    int (*fnCreateChannel)(int channel, rk_venc_chn *config);
    int (*fnGetChannelConfig)(int channel, rk_venc_chn *config);
    int (*fnGetChannelParam)(int channel, rk_venc_para *config);
    int (*fnDestroyChannel)(int channel);
    int (*fnResetChannel)(int channel);
    int (*fnSetChannelBufferShare)(int channel, int *enable);
    int (*fnSetChannelBufferWrap)(int channel, rk_venc_buf *config);
    int (*fnSetChannelConfig)(int channel, rk_venc_chn *config);
    int (*fnSetChannelParam)(int channel, rk_venc_para *config);
    int (*fnSetColorToGray)(int channel, int *active);

    int (*fnFreeDescriptor)(int channel);
    int (*fnGetDescriptor)(int channel);

    int (*fnGetJpegParam)(int channel, rk_venc_jpg *param);
    int (*fnSetJpegParam)(int channel, rk_venc_jpg *param);

    int (*fnFreeStream)(int channel, rk_venc_strm *stream);
    int (*fnGetStream)(int channel, rk_venc_strm *stream, unsigned int timeout);

    int (*fnQuery)(int channel, rk_venc_stat* stats);

    int (*fnRequestIdr)(int channel, int instant);
    int (*fnStartReceivingEx)(int channel, int *count);
    int (*fnStopReceiving)(int channel);
} rk_venc_impl;

static int rk_venc_load(rk_venc_impl *venc_lib) {
    if (!(venc_lib->handle = dlopen("librockit.so", RTLD_LAZY | RTLD_GLOBAL)))
        HAL_ERROR("rk_venc", "Failed to load library!\nError: %s\n", dlerror());

    if (!(venc_lib->fnCreateChannel = (int(*)(int channel, rk_venc_chn *config))
        hal_symbol_load("rk_venc", venc_lib->handle, "RK_MPI_VENC_CreateChn")))
        return EXIT_FAILURE;

    if (!(venc_lib->fnDestroyChannel = (int(*)(int channel))
        hal_symbol_load("rk_venc", venc_lib->handle, "RK_MPI_VENC_DestroyChn")))
        return EXIT_FAILURE;

    if (!(venc_lib->fnGetChannelConfig = (int(*)(int channel, rk_venc_chn *config))
        hal_symbol_load("rk_venc", venc_lib->handle, "RK_MPI_VENC_GetChnAttr")))
        return EXIT_FAILURE;

    if (!(venc_lib->fnGetChannelParam = (int(*)(int channel, rk_venc_para *config))
        hal_symbol_load("rk_venc", venc_lib->handle, "RK_MPI_VENC_GetChnParam")))
        return EXIT_FAILURE;

    if (!(venc_lib->fnResetChannel = (int(*)(int channel))
        hal_symbol_load("rk_venc", venc_lib->handle, "RK_MPI_VENC_ResetChn")))
        return EXIT_FAILURE;

    if (!(venc_lib->fnSetChannelBufferShare = (int(*)(int channel, int *enable))
        hal_symbol_load("rk_venc", venc_lib->handle, "RK_MPI_VENC_SetChnRefBufShareAttr")))
        return EXIT_FAILURE;

    if (!(venc_lib->fnSetChannelBufferWrap = (int(*)(int channel, rk_venc_buf *config))
        hal_symbol_load("rk_venc", venc_lib->handle, "RK_MPI_VENC_SetChnBufWrapAttr")))
        return EXIT_FAILURE;

    if (!(venc_lib->fnSetChannelConfig = (int(*)(int channel, rk_venc_chn *config))
        hal_symbol_load("rk_venc", venc_lib->handle, "RK_MPI_VENC_SetChnAttr")))
        return EXIT_FAILURE;

    if (!(venc_lib->fnSetChannelParam = (int(*)(int channel, rk_venc_para *config))
        hal_symbol_load("rk_venc", venc_lib->handle, "RK_MPI_VENC_SetChnParam")))
        return EXIT_FAILURE;

    if (!(venc_lib->fnFreeDescriptor = (int(*)(int channel))
        hal_symbol_load("rk_venc", venc_lib->handle, "RK_MPI_VENC_CloseFd")))
        return EXIT_FAILURE;

    if (!(venc_lib->fnGetDescriptor = (int(*)(int channel))
        hal_symbol_load("rk_venc", venc_lib->handle, "RK_MPI_VENC_GetFd")))
        return EXIT_FAILURE;

    if (!(venc_lib->fnGetJpegParam = (int(*)(int channel, rk_venc_jpg *param))
        hal_symbol_load("rk_venc", venc_lib->handle, "RK_MPI_VENC_GetJpegParam")))
        return EXIT_FAILURE;

    if (!(venc_lib->fnSetJpegParam = (int(*)(int channel, rk_venc_jpg *param))
        hal_symbol_load("rk_venc", venc_lib->handle, "RK_MPI_VENC_SetJpegParam")))
        return EXIT_FAILURE;

    if (!(venc_lib->fnFreeStream = (int(*)(int channel, rk_venc_strm *stream))
        hal_symbol_load("rk_venc", venc_lib->handle, "RK_MPI_VENC_ReleaseStream")))
        return EXIT_FAILURE;

    if (!(venc_lib->fnGetStream = (int(*)(int channel, rk_venc_strm *stream, unsigned int timeout))
        hal_symbol_load("rk_venc", venc_lib->handle, "RK_MPI_VENC_GetStream")))
        return EXIT_FAILURE;

    if (!(venc_lib->fnQuery = (int(*)(int channel, rk_venc_stat *stats))
        hal_symbol_load("rk_venc", venc_lib->handle, "RK_MPI_VENC_QueryStatus")))
        return EXIT_FAILURE;

    if (!(venc_lib->fnRequestIdr = (int(*)(int channel, int instant))
        hal_symbol_load("rk_venc", venc_lib->handle, "RK_MPI_VENC_RequestIDR")))
        return EXIT_FAILURE;

    if (!(venc_lib->fnStartReceivingEx = (int(*)(int channel, int *count))
        hal_symbol_load("rk_venc", venc_lib->handle, "RK_MPI_VENC_StartRecvFrame")))
        return EXIT_FAILURE;

    if (!(venc_lib->fnStopReceiving = (int(*)(int channel))
        hal_symbol_load("rk_venc", venc_lib->handle, "RK_MPI_VENC_StopRecvFrame")))
        return EXIT_FAILURE;

    return EXIT_SUCCESS;
}

static void rk_venc_unload(rk_venc_impl *venc_lib) {
    if (venc_lib->handle) dlclose(venc_lib->handle);
    venc_lib->handle = NULL;
    memset(venc_lib, 0, sizeof(*venc_lib));
}