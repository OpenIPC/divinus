#pragma once

#include "common.h"
#include "hal/types.h"
#include "lib/schrift.h"

#include <string.h>

#define TEXT_ERROR(x, ...) \
    do { \
        fprintf(stderr, "[text] \033[31m"); \
        fprintf(stderr, (x), ##__VA_ARGS__); \
        fprintf(stderr, "\033[0m"); \
        return EXIT_FAILURE; \
    } while (0)

hal_bitmap text_create_rendered(const char *font, double size, const char *text, int color);