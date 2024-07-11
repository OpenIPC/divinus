#pragma once

#include <fcntl.h>
#include <pthread.h>
#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include <sys/select.h>

#include "error.h"
#include "hal/types.h"
#include "media.h"
#include "night.h"

void jpeg_deinit();
int jpeg_init();
int jpeg_get(short width, short height, char quality, 
    char grayscale, hal_jpegdata *jpeg);
