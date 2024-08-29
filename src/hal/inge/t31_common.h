#pragma once

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../symbols.h"
#include "../types.h"

typedef enum {
    T31_PIXFMT_YUV420P,
    T31_PIXFMT_YUV422_YUYV,
    T31_PIXFMT_YUV422_UYVY,
    T31_PIXFMT_YUV422P,
    T31_PIXFMT_YUV444P,
    T31_PIXFMT_YUV410P,
    T31_PIXFMT_YUV411P,
    T31_PIXFMT_GRAY8,
    T31_PIXFMT_MONOWHITE,
    T31_PIXFMT_MONOBLACK,
    T31_PIXFMT_NV12,
    T31_PIXFMT_NV24,
    T31_PIXFMT_RGB888,
    T31_PIXFMT_BGR888,
    T31_PIXFMT_ARGB8888,
    T31_PIXFMT_RGBA8888,
    T31_PIXFMT_ABGR8888,
    T31_PIXFMT_BGRA8888,
    T31_PIXFMT_RGB565BE,
    T31_PIXFMT_RGB565LE,
    // Following twos have their MSB set to 1
    T31_PIXFMT_RGB555BE, 
    T31_PIXFMT_RGB555LE,
    T31_PIXFMT_BGR565BE,
    T31_PIXFMT_BGR565LE,
    // Following twos have their MSB set to 1
    T31_PIXFMT_BGR555BE, 
    T31_PIXFMT_BGR555LE,
    T31_PIXFMT_0RGB8888,
    T31_PIXFMT_RGB08888,
    T31_PIXFMT_0BGR8888,
    T31_PIXFMT_BGR08888,
    T31_PIXFMT_BAYER_BGGR8,
    T31_PIXFMT_BAYER_RGGB8,
    T31_PIXFMT_BAYER_GBRG8,
    T31_PIXFMT_BAYER_GRBG8,
    T31_PIXFMT_RAW,
    T31_PIXFMT_HSV888,
    T31_PIXFMT_END
} t31_common_pixfmt;

typedef struct {
    int width;
    int height;
} t31_common_dim;

typedef struct {
    int x;
    int y;
} t31_common_pnt;

typedef struct {
    t31_common_pnt p0;
    t31_common_pnt p1;
} t31_common_rect;