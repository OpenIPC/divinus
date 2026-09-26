#pragma once

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../symbols.h"
#include "../types.h"

/*
 * Fullhan FH8852/FH8856 (V100 generation, SDK V1.2.0 "OSDRV" libraries).
 *
 * The SDK ships as binary-only shared objects without headers; the interface
 * below was recovered from the libraries and from a vendor application that
 * statically links the same SDK. Only the subset divinus needs is declared.
 */

#define FH_VPSS_CHN_NUM 4
#define FH_VENC_CHN_NUM 8

/* Error codes returned by the MPI (negative) */
#define FH_ERR_NODEV   (-0x3e9)
#define FH_ERR_PARAM   (-0x3ed)
#define FH_ERR_CHN     (-0x3f0)

typedef enum {
    FH_VENC_TYPE_JPEG  = 0x01,
    FH_VENC_TYPE_MJPEG = 0x02,
    FH_VENC_TYPE_H264  = 0x04,
    FH_VENC_TYPE_H264B = 0x08,   /* variant that also needs a BGM gop threshold */
    FH_VENC_TYPE_H265  = 0x10,
    FH_VENC_TYPE_H265B = 0x20
} fh_venc_type;

typedef enum {
    FH_VENC_RC_H264_CBR = 3,     /* reported as VBR by /proc/driver/enc but it is the working CBR path; mode 8 boot-loops */
    FH_VENC_RC_H264_VBR = 4,
    FH_VENC_RC_H264_FIXQP = 5,
    FH_VENC_RC_H264_AVBR = 6,
    FH_VENC_RC_H265_CBR = 7,
    FH_VENC_RC_H265_VBR = 8
} fh_venc_rcmode;

typedef struct {
    unsigned int width;
    unsigned int height;
} fh_common_dim;
