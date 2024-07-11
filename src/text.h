#pragma once

#include "hal/support.h"
#include "lib/schrift.h"

#include <string.h>

hal_bitmap text_create_rendered(const char *font, double size, const char *text, int color);