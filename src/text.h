#pragma once

#include "common.h"
#include "hal/types.h"
#include "lib/schrift.h"

#include <string.h>

#define TEXT_ERROR(x, ...) \
    do { \
        fprintf(stderr, "%s \033[31m%s\033[0m\n", "[text] (x)", ##__VA_ARGS__); \
        return EXIT_FAILURE; \
    } while (0)

static SFT sft;
static SFT_Image canvas;
static SFT_LMetrics lmtx;
static hal_bitmap bitmap;

hal_bitmap text_create_rendered(const char *font, double size, const char *text);
hal_dim text_measure_rendered(const char *font, double size, const char *text);

static int utf8_to_utf32(const unsigned char *utf8, 
    unsigned int *utf32, int max)
{
    unsigned int c;
    int i = 0;
    --max;
    while (*utf8)
    {
        if (i >= max)
            return 0;
        if (!(*utf8 & 0x80U))
        {
            utf32[i++] = *utf8++;
        }
        else if ((*utf8 & 0xe0U) == 0xc0U)
        {
            c = (*utf8++ & 0x1fU) << 6;
            if ((*utf8 & 0xc0U) != 0x80U)
                return 0;
            utf32[i++] = c + (*utf8++ & 0x3fU);
        }
        else if ((*utf8 & 0xf0U) == 0xe0U)
        {
            c = (*utf8++ & 0x0fU) << 12;
            if ((*utf8 & 0xc0U) != 0x80U)
                return 0;
            c += (*utf8++ & 0x3fU) << 6;
            if ((*utf8 & 0xc0U) != 0x80U)
                return 0;
            utf32[i++] = c + (*utf8++ & 0x3fU);
        }
        else if ((*utf8 & 0xf8U) == 0xf0U)
        {
            c = (*utf8++ & 0x07U) << 18;
            if ((*utf8 & 0xc0U) != 0x80U)
                return 0;
            c += (*utf8++ & 0x3fU) << 12;
            if ((*utf8 & 0xc0U) != 0x80U)
                return 0;
            c += (*utf8++ & 0x3fU) << 6;
            if ((*utf8 & 0xc0U) != 0x80U)
                return 0;
            c += (*utf8++ & 0x3fU);
            if ((c & 0xFFFFF800U) == 0xD800U)
                return 0;
            utf32[i++] = c;
        }
        else
            return 0;
    }
    utf32[i] = 0;
    return i;
}