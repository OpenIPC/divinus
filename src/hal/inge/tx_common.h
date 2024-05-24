#pragma once

#include <dlfcn.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/select.h>

#include "../types.h"

#define TX_ERROR(x, ...) \
    do { \
        fprintf(stderr, "[tx_hal] \033[31m"); \
        fprintf(stderr, (x), ##__VA_ARGS__); \
        fprintf(stderr, "\033[0m"); \
        return EXIT_FAILURE; \
    } while (0)

typedef enum {
    TX_PIXFMT_YUV420P,
    TX_PIXFMT_YUV422_YUYV,
    TX_PIXFMT_YUV422_UYVY,
    TX_PIXFMT_YUV422P,
    TX_PIXFMT_YUV444P,
    TX_PIXFMT_YUV410P,
    TX_PIXFMT_YUV411P,
    TX_PIXFMT_GRAY8,
    TX_PIXFMT_MONOWHITE,
    TX_PIXFMT_MONOBLACK,
    TX_PIXFMT_NV12,
    TX_PIXFMT_NV24,
    TX_PIXFMT_RGB888,
    TX_PIXFMT_BGR888,
    TX_PIXFMT_ARGB8888,
    TX_PIXFMT_RGBA8888,
    TX_PIXFMT_ABGR8888,
    TX_PIXFMT_BGRA8888,
    TX_PIXFMT_RGB565BE,
    TX_PIXFMT_RGB565LE,
    // Following twos have their MSB set to 1
    TX_PIXFMT_RGB555BE, 
    TX_PIXFMT_RGB555LE,
    TX_PIXFMT_BGR565BE,
    TX_PIXFMT_BGR565LE,
    // Following twos have their MSB set to 1
    TX_PIXFMT_BGR555BE, 
    TX_PIXFMT_BGR555LE,
    TX_PIXFMT_0RGB8888,
    TX_PIXFMT_RGB08888,
    TX_PIXFMT_0BGR8888,
    TX_PIXFMT_BGR08888,
    TX_PIXFMT_BAYER_BGGR8,
    TX_PIXFMT_BAYER_RGGB8,
    TX_PIXFMT_BAYER_GBRG8,
    TX_PIXFMT_BAYER_GRBG8,
    TX_PIXFMT_RAW,
    TX_PIXFMT_HSV888,
    TX_PIXFMT_END
} tx_common_pixfmt;

typedef struct {
    int width;
    int height;
} tx_common_dim;

typedef struct {
    int x;
    int y;
} tx_common_pnt;

typedef struct {
    tx_common_pnt p0;
    tx_common_pnt p1;
} tx_common_rect;