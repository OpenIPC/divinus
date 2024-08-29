#pragma once

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../symbols.h"
#include "../types.h"

typedef enum {
    I3_MODE_TUNNEL,
    I3_MODE_USER,
    I3_MODE_SHARE,
    I3_MODE_NOT_TUNNEL,
    I3_MODE_INVALID
} i3_common_mode;

typedef enum {
    I3_PIXFMT_UNUSED,
    I3_PIXFMT_YUV422_YUYV,
    I3_PIXFMT_MSTAR_RAW16,
    I3_PIXFMT_MSTAR_YC16,
    I3_PIXFMT_MSTAR_STS16,
    I3_PIXFMT_YUV420SP,
    I3_PIXFMT_YUV420P,
    I3_PIXFMT_BGR565,
    I3_PIXFMT_ARGB4444,
    I3_PIXFMT_ARGB1555,
    I3_PIXFMT_RGB888,
    I3_PIXFMT_ABGR8888,
    I3_PIXFMT_YUV400,
    I3_PIXFMT_END
} i3_common_pixfmt;

typedef struct {
    unsigned int width;
    unsigned int height;
} i3_common_dim;

typedef struct {
    int x;
    int y;
} i3_common_pnt;

typedef struct {
    int x;
    int y;
    unsigned int width;
    unsigned int height;
} i3_common_rect;