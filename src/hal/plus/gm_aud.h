#pragma once

#define GM_AUD_CHN_NUM 1

typedef enum {
    GM_AENC_TYPE_PCM = 1,
    GM_AENC_TYPE_AAC,
    GM_AENC_TYPE_ADPCM,
    GM_AENC_TYPE_G711A,
    GM_AENC_TYPE_G711U
} gm_aenc_type;

typedef struct {
    int internal[8];
    gm_aenc_type codec;
    int bitrate;
    int packNumPerFrm;
    int reserved[4];
} gm_aenc_cnf;

typedef struct {
    int internal[8];
    int channel;
    int rate;
    int frmNum;
    // Must be 1 or 2 (mono or stereo)
    int chnNum;
    int reserved[5];
} gm_ain_cnf;