#pragma once

#include "t31_common.h"

#define T31_VENC_CHN_NUM 6

typedef enum {
    T31_VENC_CODEC_H264,
    T31_VENC_CODEC_H265,
    T31_VENC_CODEC_MJPG = 4
} t31_venc_codec;

typedef enum {
    T31_VENC_GOPMODE_NORMAL  = 0x02,
    T31_VENC_GOPMODE_PYRAMID = 0x04,
    T31_VENC_GOPMODE_SMARTP  = 0xfe,
    T31_VENC_GOPMODE_END
} t31_venc_gopmode;

typedef enum {
    T31_VENC_NALU_H264_PSLICE = 1,
    T31_VENC_NALU_H264_DPASLICE,
    T31_VENC_NALU_H264_DPBSLICE,
    T31_VENC_NALU_H264_DPCSLICE,
    T31_VENC_NALU_H264_IDRSLICE,
    T31_VENC_NALU_H264_SEI,
    T31_VENC_NALU_H264_SPS,
    T31_VENC_NALU_H264_PPS,
    T31_VENC_NALU_H264_AUD,
    T31_VENC_NALU_H264_FILLER = 12
} t31_venc_nalu_h264;

typedef enum {
    T31_VENC_NALU_H265_TRAILLSLICE,
    T31_VENC_NALU_H265_TRAILLSLICE_REF,
    T31_VENC_NALU_H265_TSASLICE,
    T31_VENC_NALU_H265_TSASLICE_REF,
    T31_VENC_NALU_H265_STSASLICE,
    T31_VENC_NALU_H265_STSASLICE_REF,
    T31_VENC_NALU_H265_RADLSLICE,
    T31_VENC_NALU_H265_RADLSLICE_REF,
    T31_VENC_NALU_H265_RASLSLICE,
    T31_VENC_NALU_H265_RASLSLICE_REF,
    T31_VENC_NALU_H265_BLASLICE_LP = 16,
    T31_VENC_NALU_H265_BLASLICE_RADL,
    T31_VENC_NALU_H265_BLASLICE,
    T31_VENC_NALU_H265_IDRSLICE_RADL,
    T31_VENC_NALU_H265_IDRSLICE,
    T31_VENC_NALU_H265_CRASLICE,
    T31_VENC_NALU_H265_VPS = 32,
    T31_VENC_NALU_H265_SPS,
    T31_VENC_NALU_H265_PPS,
    T31_VENC_NALU_H265_AUD,
    T31_VENC_NALU_H265_EOS,
    T31_VENC_NALU_H265_EOB,
    T31_VENC_NALU_H265_FILLER,
    T31_VENC_NALU_H265_PREFIX_SEI,
    T31_VENC_NALU_H265_SUFFIX_SEI
} t31_venc_nalu_h265;

typedef enum {
    T31_VENC_OPT_NONE,
    T31_VENC_OPT_QP_TAB_RELATIVE = 0x00001,
    T31_VENC_OPT_FIX_PREDICTOR   = 0x00002,
    T31_VENC_OPT_CUSTOM_LDA      = 0x00004,
    T31_VENC_OPT_ENABLE_AUTO_QP  = 0x00008,
    T31_VENC_OPT_ADAPT_AUTO_QP   = 0x00010,
    T31_VENC_OPT_COMPRESS        = 0x00020,
    T31_VENC_OPT_FORCE_REC       = 0x00040,
    T31_VENC_OPT_FORCE_MV_OUT    = 0x00080,
    T31_VENC_OPT_HIGH_FREQ       = 0x02000,
    T31_VENC_OPT_SRD             = 0x08000,
    T31_VENC_OPT_FORCE_MV_CLIP   = 0x20000,
    T31_VENC_OPT_RDO_COST_MODE   = 0x40000,
} t31_venc_opt;

typedef enum {
    T31_VENC_PICFMT_400_8BPP = 0x88,
    T31_VENC_PICFMT_420_8BPP = 0x188,
    T31_VENC_PICFMT_422_8BPP = 0x288,
} t31_venc_picfmt;

typedef enum {
    T31_VENC_PROF_H264_BASE = (T31_VENC_CODEC_H264 << 24) | 66,
    T31_VENC_PROF_H264_MAIN = (T31_VENC_CODEC_H264 << 24) | 77,
    T31_VENC_PROF_H264_HIGH = (T31_VENC_CODEC_H264 << 24) | 100,
    T31_VENC_PROF_H265_MAIN = (T31_VENC_CODEC_H265 << 24) | 1,
    T31_VENC_PROF_MJPG = (T31_VENC_CODEC_MJPG << 24),
} t31_venc_prof;

typedef enum {
    T31_VENC_RATEMODE_QP,
    T31_VENC_RATEMODE_CBR,
    T31_VENC_RATEMODE_VBR,
    T31_VENC_RATEMODE_LOWLATENCY,
    T31_VENC_RATEMODE_CVBR = 4,
    T31_VENC_RATEMODE_AVBR = 8
} t31_venc_ratemode;

typedef enum {
    T31_VENC_RCOPT_NONE,
    T31_VENC_RCOPT_SCN_CHG_RES    = 0x01,
    T31_VENC_RCOPT_DELAYED        = 0x02,
    T31_VENC_RCOPT_STATIC_SCENE   = 0x04,
    T31_VENC_RCOPT_ENABLE_SKIP    = 0x08,
    T31_VENC_RCOPT_SC_PREVENTION  = 0x10,
    T31_VENC_RCOPT_END
} t31_venc_rcopt;

typedef enum {
    T31_VENC_SLICE_B,
    T31_VENC_SLICE_P,
    T31_VENC_SLICE_I,
    T31_VENC_SLICE_GLODEN,
    T31_VENC_SLICE_SP,
    T31_VENC_SLICE_SI,
    T31_VENC_SLICE_CONCEAL = 6,
    T31_VENC_SLICE_SKIP,
    T31_VENC_SLICE_REPEAT,
    T31_VENC_SLICE_END
} t31_venc_slice;

typedef enum {
  T31_VENC_TOOL_WPP              = 0x001,
  T31_VENC_TOOL_TILE             = 0x002,
  T31_VENC_TOOL_LF               = 0x004,
  T31_VENC_TOOL_LF_X_SLICE       = 0x008,
  T31_VENC_TOOL_LF_X_TILE        = 0x010,
  T31_VENC_TOOL_SCL_LST          = 0x020,
  T31_VENC_TOOL_CONST_INTRA_PRED = 0x040,
  T31_VENC_TOOL_TRANSFO_SKIP     = 0x080,
  T31_VENC_TOOL_PCM              = 0x800,
} t31_venc_tool;

typedef union {
    t31_venc_nalu_h264 h264Nalu;
    t31_venc_nalu_h265 h265Nalu;
} t31_venc_nalu;

typedef struct {
    unsigned int offset;
    unsigned int length;
    long long timestamp;
    char endFrame;
    t31_venc_nalu naluType;
    t31_venc_slice sliceType;
} t31_venc_pack;

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
} t31_venc_strminfo;

typedef struct {
    int size;
    short quality;
} t31_venc_jpeginfo;

typedef struct {
    unsigned int phy;
    unsigned int addr;
    unsigned int length;
    t31_venc_pack *packet;
    unsigned int count;
    unsigned int sequence;
    char isVi;
    union {
        t31_venc_strminfo strmInfo;
        t31_venc_jpeginfo jpegInfo;
    };
} t31_venc_strm;

typedef struct {
    char enable;
    unsigned int x, y, width, height;
} t31_venc_crop;

typedef struct {
    t31_venc_prof profile;
    unsigned char level;
    unsigned char tier;
    unsigned short width;
    unsigned short height;
    t31_venc_picfmt picFmt;
    t31_venc_opt options;
    t31_venc_tool tools;
    t31_venc_crop crop;
} t31_venc_attrib;

typedef struct {
    unsigned int tgtBitrate;
    short initQual;
    short minQual;
    short maxQual;
    short ipDelta;
    short pbDelta;
    t31_venc_rcopt options;
    unsigned int maxPicSize;
} t31_venc_rate_cbr;

typedef struct {
    unsigned int tgtBitrate;
    unsigned int maxBitrate;
    short initQual;
    short minQual;
    short maxQual;
    short ipDelta;
    short pbDelta;
    t31_venc_rcopt options;
    unsigned int maxPicSize;
} t31_venc_rate_vbr;

typedef struct {
    unsigned int tgtBitrate;
    unsigned int maxBitrate;
    short initQual;
    short minQual;
    short maxQual;
    short ipDelta;
    short pbDelta;
    t31_venc_rcopt options;
    unsigned int maxPicSize;
    unsigned short maxPsnr;
} t31_venc_rate_xvbr;

typedef struct {
    t31_venc_ratemode mode;
    union {
        short qpModeQual;
        t31_venc_rate_cbr cbr;
        t31_venc_rate_vbr vbr;
        t31_venc_rate_xvbr cvbr;
        t31_venc_rate_xvbr avbr;
    };
    unsigned int fpsNum;
    unsigned int fpsDen;
} t31_venc_rate;

typedef struct {
    t31_venc_gopmode mode;
    unsigned short length;
    unsigned short notifyUserLTInter;
    unsigned int maxSameSenceCnt;
    char enableLT;
    unsigned int freqLT;
    char LTRC;
} t31_venc_gop;

typedef struct {
    t31_venc_attrib attrib;
    t31_venc_rate rate;
    t31_venc_gop gop;
} t31_venc_chn;

typedef struct {
    char isRegistered;
    unsigned int leftPics;
    unsigned int leftBytes;
    unsigned int leftFrames;
    unsigned int curPacks;
    unsigned int workDone;
} t31_venc_stat;

typedef struct {
    void *handle;

    int (*fnSetDefaults)(t31_venc_chn *config, t31_venc_prof profile,
        t31_venc_ratemode ratemode, unsigned short width, unsigned short height,
        unsigned int fpsNum, unsigned int fpsDen, unsigned int gopLength,
        int maxSameSenceCnt, int initialQp, unsigned int tgtBitrate);
    
    int (*fnCreateGroup)(int group);
    int (*fnDestroyGroup)(int group);

    int (*fnCreateChannel)(int channel, t31_venc_chn *config);
    int (*fnDestroyChannel)(int channel);
    int (*fnRegisterChannel)(int group, int channel);
    int (*fnSetChannelShared)(int channel, int shrChn);
    int (*fnUnregisterChannel)(int channel);

    int (*fnGetDescriptor)(int channel);

    int (*fnFreeStream)(int channel, t31_venc_strm *stream);
    int (*fnGetStream)(int channel, t31_venc_strm *stream, char blockingOn);
    int (*fnPollStream)(int channel, int timeoutMs);

    int (*fnQuery)(int channel, t31_venc_stat* stats);

    int (*fnRequestIdr)(int channel);
    int (*fnStartReceiving)(int channel);
    int (*fnStopReceiving)(int channel);
} t31_venc_impl;

static int t31_venc_load(t31_venc_impl *venc_lib) {
    if (!(venc_lib->handle = dlopen("libimp.so", RTLD_LAZY | RTLD_GLOBAL)))
        HAL_ERROR("t31_venc", "Failed to load library!\nError: %s\n", dlerror());

    if (!(venc_lib->fnSetDefaults = (int(*)(t31_venc_chn *config, t31_venc_prof profile, 
        t31_venc_ratemode ratemode, unsigned short width, unsigned short height, 
        unsigned int fpsNum, unsigned int fpsDen, unsigned int gopLength, 
        int maxSameSenceCnt, int initialQp, unsigned int tgtBitrate))
        hal_symbol_load("t31_venc", venc_lib->handle, "IMP_Encoder_SetDefaultParam")))
        return EXIT_FAILURE;

    if (!(venc_lib->fnCreateGroup = (int(*)(int group))
        hal_symbol_load("t31_venc", venc_lib->handle, "IMP_Encoder_CreateGroup")))
        return EXIT_FAILURE;

    if (!(venc_lib->fnDestroyGroup = (int(*)(int group))
        hal_symbol_load("t31_venc", venc_lib->handle, "IMP_Encoder_DestroyGroup")))
        return EXIT_FAILURE;

    if (!(venc_lib->fnCreateChannel = (int(*)(int channel, t31_venc_chn *config))
        hal_symbol_load("t31_venc", venc_lib->handle, "IMP_Encoder_CreateChn")))
        return EXIT_FAILURE;

    if (!(venc_lib->fnDestroyChannel = (int(*)(int channel))
        hal_symbol_load("t31_venc", venc_lib->handle, "IMP_Encoder_DestroyChn")))
        return EXIT_FAILURE;

    if (!(venc_lib->fnRegisterChannel = (int(*)(int group, int channel))
        hal_symbol_load("t31_venc", venc_lib->handle, "IMP_Encoder_RegisterChn")))
        return EXIT_FAILURE;

    if (!(venc_lib->fnSetChannelShared = (int(*)(int channel, int shrChn))
        hal_symbol_load("t31_venc", venc_lib->handle, "IMP_Encoder_SetbufshareChn")))
        return EXIT_FAILURE;

    if (!(venc_lib->fnUnregisterChannel = (int(*)(int channel))
        hal_symbol_load("t31_venc", venc_lib->handle, "IMP_Encoder_UnRegisterChn")))
        return EXIT_FAILURE;

    if (!(venc_lib->fnGetDescriptor = (int(*)(int channel))
        hal_symbol_load("t31_venc", venc_lib->handle, "IMP_Encoder_GetFd")))
        return EXIT_FAILURE;

    if (!(venc_lib->fnFreeStream = (int(*)(int channel, t31_venc_strm *stream))
        hal_symbol_load("t31_venc", venc_lib->handle, "IMP_Encoder_ReleaseStream")))
        return EXIT_FAILURE;

    if (!(venc_lib->fnGetStream = (int(*)(int channel, t31_venc_strm *stream, char blockingOn))
        hal_symbol_load("t31_venc", venc_lib->handle, "IMP_Encoder_GetStream")))
        return EXIT_FAILURE;

    if (!(venc_lib->fnPollStream = (int(*)(int channel, int timeoutMs))
        hal_symbol_load("t31_venc", venc_lib->handle, "IMP_Encoder_PollingStream")))
        return EXIT_FAILURE;

    if (!(venc_lib->fnQuery = (int(*)(int channel, t31_venc_stat *stats))
        hal_symbol_load("t31_venc", venc_lib->handle, "IMP_Encoder_Query")))
        return EXIT_FAILURE;

    if (!(venc_lib->fnRequestIdr = (int(*)(int channel))
        hal_symbol_load("t31_venc", venc_lib->handle, "IMP_Encoder_RequestIDR")))
        return EXIT_FAILURE;

    if (!(venc_lib->fnStartReceiving = (int(*)(int channel))
        hal_symbol_load("t31_venc", venc_lib->handle, "IMP_Encoder_StartRecvPic")))
        return EXIT_FAILURE;

    if (!(venc_lib->fnStopReceiving = (int(*)(int channel))
        hal_symbol_load("t31_venc", venc_lib->handle, "IMP_Encoder_StopRecvPic")))
        return EXIT_FAILURE;

    return EXIT_SUCCESS;
}

static void t31_venc_unload(t31_venc_impl *venc_lib) {
    if (venc_lib->handle) dlclose(venc_lib->handle);
    venc_lib->handle = NULL;
    memset(venc_lib, 0, sizeof(*venc_lib));
}