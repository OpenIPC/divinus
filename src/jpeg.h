#pragma once

#include <stdint.h>

#include "common.h"
#include "hal/types.h"

void jpeg_deinit();
int jpeg_init();
int jpeg_get(short width, short height, char quality, 
    char grayscale, hal_jpegdata *jpeg);
