#pragma once

#include "tx_common.h"

// To be validated
#define TX_VENC_CHN_NUM 4

typedef enum {
    TX_VENC_CODEC_H264,
    TX_VENC_CODEC_H265,
    TX_VENC_CODEC_MJPG = 4
} tx_venc_codec;

typedef enum {
    TX_VENC_GOPMODE_NORMAL  = 0x02,
    TX_VENC_GOPMODE_PYRAMID = 0x04,
    TX_VENC_GOPMODE_SMARTP  = 0xfe,
    TX_VENC_GOPMODE_END
} tx_venc_gopmode;

typedef enum {
    TX_VENC_NALU_H264_PSLICE = 1,
    TX_VENC_NALU_H264_DPASLICE,
    TX_VENC_NALU_H264_DPBSLICE,
    TX_VENC_NALU_H264_DPCSLICE,
    TX_VENC_NALU_H264_IDRSLICE,
    TX_VENC_NALU_H264_SEI,
    TX_VENC_NALU_H264_SPS,
    TX_VENC_NALU_H264_PPS,
    TX_VENC_NALU_H264_AUD,
    TX_VENC_NALU_H264_FILLER = 12
} tx_venc_nalu_h264;

typedef enum {
    TX_VENC_NALU_H265_TRAILLSLICE,
    TX_VENC_NALU_H265_TRAILLSLICE_REF,
    TX_VENC_NALU_H265_TSASLICE,
    TX_VENC_NALU_H265_TSASLICE_REF,
    TX_VENC_NALU_H265_STSASLICE,
    TX_VENC_NALU_H265_STSASLICE_REF,
    TX_VENC_NALU_H265_RADLSLICE,
    TX_VENC_NALU_H265_RADLSLICE_REF,
    TX_VENC_NALU_H265_RASLSLICE,
    TX_VENC_NALU_H265_RASLSLICE_REF,
    TX_VENC_NALU_H265_BLASLICE_LP = 16,
    TX_VENC_NALU_H265_BLASLICE_RADL,
    TX_VENC_NALU_H265_BLASLICE,
    TX_VENC_NALU_H265_IDRSLICE_RADL,
    TX_VENC_NALU_H265_IDRSLICE,
    TX_VENC_NALU_H265_CRASLICE,
    TX_VENC_NALU_H265_VPS = 32,
    TX_VENC_NALU_H265_SPS,
    TX_VENC_NALU_H265_PPS,
    TX_VENC_NALU_H265_AUD,
    TX_VENC_NALU_H265_EOS,
    TX_VENC_NALU_H265_EOB,
    TX_VENC_NALU_H265_FILLER,
    TX_VENC_NALU_H265_PREFIX_SEI,
    TX_VENC_NALU_H265_SUFFIX_SEI
} tx_venc_nalu_h265;

typedef enum {
    TX_VENC_OPT_NONE,
    TX_VENC_OPT_QP_TAB_RELATIVE = 0x00001,
    TX_VENC_OPT_FIX_PREDICTOR   = 0x00002,
    TX_VENC_OPT_CUSTOM_LDA      = 0x00004,
    TX_VENC_OPT_ENABLE_AUTO_QP  = 0x00008,
    TX_VENC_OPT_ADAPT_AUTO_QP   = 0x00010,
    TX_VENC_OPT_COMPRESS        = 0x00020,
    TX_VENC_OPT_FORCE_REC       = 0x00040,
    TX_VENC_OPT_FORCE_MV_OUT    = 0x00080,
    TX_VENC_OPT_HIGH_FREQ       = 0x02000,
    TX_VENC_OPT_SRD             = 0x08000,
    TX_VENC_OPT_FORCE_MV_CLIP   = 0x20000,
    TX_VENC_OPT_RDO_COST_MODE   = 0x40000,
} tx_venc_opt;

typedef enum {
    TX_VENC_PICFMT_400_8BPP = 0x88,
    TX_VENC_PICFMT_420_8BPP = 0x188,
    TX_VENC_PICFMT_422_8BPP = 0x288,
} tx_venc_picfmt;

typedef enum {
    TX_VENC_PROF_H264_BASE = (TX_VENC_CODEC_H264 << 24) | 66,
    TX_VENC_PROF_H264_MAIN = (TX_VENC_CODEC_H264 << 24) | 77,
    TX_VENC_PROF_H264_HIGH = (TX_VENC_CODEC_H264 << 24) | 100,
    TX_VENC_PROF_H265_MAIN = (TX_VENC_CODEC_H265 << 24) | 1,
    TX_VENC_PROF_MJPG = (TX_VENC_CODEC_MJPG << 24),
} tx_venc_prof;

typedef enum {
    TX_VENC_RATEMODE_QP,
    TX_VENC_RATEMODE_CBR,
    TX_VENC_RATEMODE_VBR,
    TX_VENC_RATEMODE_CVBR,
    TX_VENC_RATEMODE_AVBR
} tx_venc_ratemode;

typedef enum {
    TX_VENC_RCOPT_NONE,
    TX_VENC_RCOPT_SCN_CHG_RES    = 0x01,
    TX_VENC_RCOPT_DELAYED        = 0x02,
    TX_VENC_RCOPT_STATIC_SCENE   = 0x04,
    TX_VENC_RCOPT_ENABLE_SKIP    = 0x08,
    TX_VENC_RCOPT_SC_PREVENTION  = 0x10,
    TX_VENC_RCOPT_END
} tx_venc_rcopt;

typedef enum {
    TX_VENC_SLICE_B,
    TX_VENC_SLICE_P,
    TX_VENC_SLICE_I,
    TX_VENC_SLICE_GLODEN,
    TX_VENC_SLICE_SP,
    TX_VENC_SLICE_SI,
    TX_VENC_SLICE_CONCEAL = 6,
    TX_VENC_SLICE_SKIP,
    TX_VENC_SLICE_REPEAT,
    TX_VENC_SLICE_END
} tx_venc_slice;

typedef enum {
  TX_VENC_TOOL_WPP              = 0x001,
  TX_VENC_TOOL_TILE             = 0x002,
  TX_VENC_TOOL_LF               = 0x004,
  TX_VENC_TOOL_LF_X_SLICE       = 0x008,
  TX_VENC_TOOL_LF_X_TILE        = 0x010,
  TX_VENC_TOOL_SCL_LST          = 0x020,
  TX_VENC_TOOL_CONST_INTRA_PRED = 0x040,
  TX_VENC_TOOL_TRANSFO_SKIP     = 0x080,
  TX_VENC_TOOL_PCM              = 0x800,
} tx_venc_tool;

typedef union {
    tx_venc_nalu_h264 h264Nalu;
    tx_venc_nalu_h265 h265Nalu;
} tx_venc_nalu;

typedef struct {
    unsigned int offset;
    unsigned int length;
    long long timestamp;
    char endFrame;
    tx_venc_nalu naluType;
    tx_venc_slice sliceType;
} tx_venc_pack;

typedef struct {
    int size;
    unsigned int intra;
    unsigned int pSkip;
    unsigned int pCu8x8;
    unsigned int pCu16x16;
    unsigned int pCu32x32;
    unsigned int pCu64x64;
    short sliceQual;
    short minQual;
    short maxQual;
} tx_venc_strminfo;

typedef struct {
    int size;
    short quality;
} tx_venc_jpeginfo;

typedef struct {
    unsigned int phy;
    unsigned int addr;
    unsigned int length;
    tx_venc_pack *packet;
    unsigned int count;
    unsigned int sequence;
    char isVi;
    union {
        tx_venc_strminfo strmInfo;
        tx_venc_jpeginfo jpegInfo;
    };
} tx_venc_strm;

typedef struct {
    char enable;
    unsigned int x, y, width, height;
} tx_venc_crop;

typedef struct {
    tx_venc_prof profile;
    unsigned char level;
    unsigned char tier;
    unsigned short width;
    unsigned short height;
    tx_venc_picfmt picFmt;
    tx_venc_opt options;
    tx_venc_tool tools;
    tx_venc_crop crop;
} tx_venc_attrib;

typedef struct {
    unsigned int tgtBitrate;
    short initQual;
    short minQual;
    short maxQual;
    short ipDelta;
    short pbDelta;
    tx_venc_rcopt options;
    unsigned int maxPicSize;
} tx_venc_rate_cbr;

typedef struct {
    unsigned int tgtBitrate;
    unsigned int maxBitrate;
    short initQual;
    short minQual;
    short maxQual;
    short ipDelta;
    short pbDelta;
    tx_venc_rcopt options;
    unsigned int maxPicSize;
} tx_venc_rate_vbr;

typedef struct {
    unsigned int tgtBitrate;
    unsigned int maxBitrate;
    short initQual;
    short minQual;
    short maxQual;
    short ipDelta;
    short pbDelta;
    tx_venc_rcopt options;
    unsigned int maxPicSize;
    unsigned short maxPsnr;
} tx_venc_rate_xvbr;

typedef struct {
    tx_venc_ratemode mode;
    union {
        short qpModeQual;
        tx_venc_rate_cbr cbr;
        tx_venc_rate_vbr vbr;
        tx_venc_rate_xvbr cvbr;
        tx_venc_rate_xvbr avbr;
    };
    unsigned int fpsDen;
    unsigned int fpsNum;
} tx_venc_rate;

typedef struct {
    tx_venc_gopmode mode;
    unsigned short length;
    unsigned short notifyUserLTInter;
    unsigned int maxSameSenceCnt;
    char enableLT;
    unsigned int freqLT;
    char LTRC;
} tx_venc_gop;

typedef struct {
    tx_venc_attrib attrib;
    tx_venc_rate rate;
    tx_venc_gop gop;
} tx_venc_chn;

typedef struct {
    char isRegistered;
    unsigned int leftPics;
    unsigned int leftBytes;
    unsigned int leftFrames;
    unsigned int curPacks;
    unsigned int workDone;
} tx_venc_stat;

typedef struct {
    void *handle;
    
    int (*fnCreateGroup)(int group);
    int (*fnDestroyGroup)(int group);

    int (*fnCreateChannel)(int channel, tx_venc_chn *config);
    int (*fnDestroyChannel)(int channel);
    int (*fnRegisterChannel)(int group, int channel);
    int (*fnUnregisterChannel)(int channel);

    int (*fnGetDescriptor)(int channel);

    int (*fnFreeStream)(int channel, tx_venc_strm *stream);
    int (*fnGetStream)(int channel, tx_venc_strm *stream, char blockingOn);

    int (*fnQuery)(int channel, tx_venc_stat* stats);

    int (*fnStartReceiving)(int channel);
    int (*fnStopReceiving)(int channel);
} tx_venc_impl;

static int tx_venc_load(tx_venc_impl *venc_lib) {
    if (!(venc_lib->handle = dlopen("libimp.so", RTLD_LAZY | RTLD_GLOBAL))) {
        fprintf(stderr, "[tx_venc] Failed to load library!\nError: %s\n", dlerror());
        return EXIT_FAILURE;
    }

    if (!(venc_lib->fnCreateGroup = (int(*)(int group))
        dlsym(venc_lib->handle, "IMP_Encoder_CreateGroup"))) {
        fprintf(stderr, "[tx_venc] Failed to acquire symbol IMP_Encoder_CreateGroup!\n");
        return EXIT_FAILURE;
    }

    if (!(venc_lib->fnDestroyGroup = (int(*)(int group))
        dlsym(venc_lib->handle, "IMP_Encoder_DestroyGroup"))) {
        fprintf(stderr, "[tx_venc] Failed to acquire symbol IMP_Encoder_DestroyGroup!\n");
        return EXIT_FAILURE;
    }

    if (!(venc_lib->fnCreateChannel = (int(*)(int channel, tx_venc_chn *config))
        dlsym(venc_lib->handle, "IMP_Encoder_CreateChn"))) {
        fprintf(stderr, "[tx_venc] Failed to acquire symbol IMP_Encoder_CreateChn!\n");
        return EXIT_FAILURE;
    }

    if (!(venc_lib->fnDestroyChannel = (int(*)(int channel))
        dlsym(venc_lib->handle, "IMP_Encoder_DestroyChn"))) {
        fprintf(stderr, "[tx_venc] Failed to acquire symbol IMP_Encoder_DestroyChn!\n");
        return EXIT_FAILURE;
    }

    if (!(venc_lib->fnRegisterChannel = (int(*)(int group, int channel))
        dlsym(venc_lib->handle, "IMP_Encoder_RegisterChn"))) {
        fprintf(stderr, "[tx_venc] Failed to acquire symbol IMP_Encoder_RegisterChn!\n");
        return EXIT_FAILURE;
    }

    if (!(venc_lib->fnUnregisterChannel = (int(*)(int channel))
        dlsym(venc_lib->handle, "IMP_Encoder_UnRegisterChn"))) {
        fprintf(stderr, "[tx_venc] Failed to acquire symbol IMP_Encoder_UnRegisterChn!\n");
        return EXIT_FAILURE;
    }

    if (!(venc_lib->fnGetDescriptor = (int(*)(int channel))
        dlsym(venc_lib->handle, "IMP_Encoder_GetFd"))) {
        fprintf(stderr, "[tx_venc] Failed to acquire symbol IMP_Encoder_GetFd!\n");
        return EXIT_FAILURE;
    }

    if (!(venc_lib->fnFreeStream = (int(*)(int channel, tx_venc_strm *stream))
        dlsym(venc_lib->handle, "IMP_Encoder_ReleaseStream"))) {
        fprintf(stderr, "[tx_venc] Failed to acquire symbol IMP_Encoder_ReleaseStream!\n");
        return EXIT_FAILURE;
    }

    if (!(venc_lib->fnGetStream = (int(*)(int channel, tx_venc_strm *stream, char blockingOn))
        dlsym(venc_lib->handle, "IMP_Encoder_GetStream"))) {
        fprintf(stderr, "[tx_venc] Failed to acquire symbol IMP_Encoder_GetStream!\n");
        return EXIT_FAILURE;
    }

    if (!(venc_lib->fnQuery = (int(*)(int channel, tx_venc_stat *stats))
        dlsym(venc_lib->handle, "IMP_Encoder_Query"))) {
        fprintf(stderr, "[tx_venc] Failed to acquire symbol IMP_Encoder_Query!\n");
        return EXIT_FAILURE;
    }

    if (!(venc_lib->fnStartReceiving = (int(*)(int channel))
        dlsym(venc_lib->handle, "IMP_Encoder_StartRecvPic"))) {
        fprintf(stderr, "[tx_venc] Failed to acquire symbol IMP_Encoder_StartRecvPic!\n");
        return EXIT_FAILURE;
    }

    if (!(venc_lib->fnStopReceiving = (int(*)(int channel))
        dlsym(venc_lib->handle, "IMP_Encoder_StopRecvPic"))) {
        fprintf(stderr, "[tx_venc] Failed to acquire symbol IMP_Encoder_StopRecvPic!\n");
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}

static void tx_venc_unload(tx_venc_impl *venc_lib) {
    if (venc_lib->handle) dlclose(venc_lib->handle);
    venc_lib->handle = NULL;
    memset(venc_lib, 0, sizeof(*venc_lib));
}