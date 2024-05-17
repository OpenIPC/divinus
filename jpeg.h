#pragma once

#include <stdint.h>

#include "common.h"

struct jpegdata {
    char *buf;
    ssize_t buf_size;
    ssize_t jpeg_size;
};

int jpeg_deinit();
int jpeg_init();
int jpeg_get(short width, short height, char quality, 
    char grayscale, struct jpegdata *jpeg);
