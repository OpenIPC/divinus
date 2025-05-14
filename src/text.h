#pragma once

#include <string.h>

#include "hal/support.h"
#include "lib/schrift.h"

hal_bitmap text_create_rendered(const char *font, double size, const char *text, 
    int color, int outline, double thick);