#include "text.h"

const double inv255 = 1.0 / 255.0;

SFT sft;
SFT_Image canvas;
SFT_LMetrics lmtx;
hal_bitmap bitmap;

int utf8_to_utf32(const unsigned char *utf8, 
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

void text_copy_rendered(SFT_Image *dest, const SFT_Image *source, int x0, int y0, int color)
{
    unsigned short maskr = (color & 0x7C00) >> 10;
    unsigned short maskg = (color & 0x3E0) >> 5;
    unsigned short maskb = color & 0x1F;

    unsigned short *d = dest->pixels;
    unsigned char *s = source->pixels;
    d += x0 + y0 * dest->width;

    for (int y = 0; y < source->height; y++) {
        for (int x = 0; x < source->width; x++) {
            double t = s[x] * inv255;
            unsigned short r = (1.0 - t) * ((d[x] & 0x7C00) >> 10) + t * maskr;
            unsigned short g = (1.0 - t) * ((d[x] & 0x3E0) >> 5) + t * maskg;
            unsigned short b = (1.0 - t) * (d[x] & 0x1F) + t * maskb;
            d[x] = ((t > 0.0) << 15) | (r << 10) | (g << 5) | b;
        }
        d += dest->width;
        s += source->width;
    }
}

int text_load_font(SFT *sft, const char *path, double size, SFT_LMetrics *lmtx)
{
    SFT_Font *font = sft_loadfile(path);
    if (font == NULL)
        HAL_ERROR("text", "sft_loadfile failed");
    sft->font = font;
    sft->xScale = size;
    sft->yScale = size;
    sft->xOffset = 0.0;
    sft->yOffset = 0.0;
    sft->flags = SFT_DOWNWARD_Y;
    if (sft_lmetrics(sft, lmtx) < 0)
        HAL_ERROR("text", "sft_lmetrics failed");
    return EXIT_SUCCESS;
}

int text_load_glyph(const SFT *sft, SFT_UChar codepoint, SFT_Glyph *glyph, SFT_GMetrics *metrics)
{
    if (sft_lookup(sft, codepoint, glyph) < 0)
        HAL_ERROR("text", "sft_lookup failed");
    if (sft_gmetrics(sft, *glyph, metrics) < 0)
        HAL_ERROR("text", "sft_gmetrics failed");
    return EXIT_SUCCESS;
}

void text_new_rendered(SFT_Image *image, int width, int height, int color)
{
    size_t size = (size_t)(width * height * 2);
    void *pixels = malloc(size);
    image->pixels = pixels;
    image->width = width;
    image->height = height;
    if (color == 0)
        memset(pixels, 0, size);
    else for (int i = 0; i < size / 2; i++)
        ((unsigned short*)pixels)[i] = color;
}

void text_dim_rendered(double *margin, double *height, double *width, const char* text)
{
    double lwidth = 0;
    *margin = 0;
    *height = lmtx.ascender - lmtx.descender + lmtx.lineGap;
    *width = 0;

    unsigned cps[strlen(text) + 1];
    int n = utf8_to_utf32(text, cps, strlen(text) + 1);

    for (int k = 0; k < n; k++) {
        if (cps[k] == '\\' && cps[k + 1] == 'n') {
            k++;
            *width = MAX(*width, lwidth);
            *height += lmtx.ascender - lmtx.descender + lmtx.lineGap;
            lwidth = 0;
            continue;
        }
        SFT_UChar cp = (SFT_UChar)cps[k];
        SFT_Glyph gid;
        SFT_GMetrics mtx;
        text_load_glyph(&sft, cp, &gid, &mtx);
        if (lwidth == 0 && mtx.leftSideBearing < 0 && *margin < -mtx.leftSideBearing)
            *margin -= mtx.leftSideBearing;
        lwidth += MAX(mtx.advanceWidth, mtx.minWidth);
    }
    *height += -lmtx.descender + lmtx.lineGap + 2 * *margin;
    *width = MAX(*width, lwidth) + 2 * *margin;
}

hal_bitmap text_create_rendered(const char *font, double size, const char *text, int color)
{
    text_load_font(&sft, font, size, &lmtx);

    double margin, height, width;
    text_dim_rendered(&margin, &height, &width, text);
    text_new_rendered(&canvas, (CEILING(width) + 3) & ~3, CEILING(height), 0);

    unsigned cps[strlen(text) + 1];
    int n = utf8_to_utf32(text, cps, strlen(text) + 1);

    double x = margin;
    double y = margin + lmtx.ascender + lmtx.lineGap;
    SFT_Glyph ogid = 0;
    for (int k = 0; k < n; k++)
    {
        if (cps[k] == '\\' && cps[k + 1] == 'n')
        {
            k++;
            x = margin;
            y += lmtx.ascender - lmtx.descender + lmtx.lineGap;
            ogid = 0;
            continue;
        }
        SFT_Image image;
        SFT_UChar cp = (SFT_UChar)cps[k];
        SFT_Glyph gid;
        SFT_GMetrics mtx;
        SFT_Kerning kerning;
        text_load_glyph(&sft, cp, &gid, &mtx);
        text_new_rendered(&image, mtx.minWidth, mtx.minHeight, 0);
        sft_render(&sft, gid, image);
        sft_kerning(&sft, ogid, gid, &kerning);
        x += kerning.xShift;
        text_copy_rendered(&canvas, &image, x + mtx.leftSideBearing,
            y + mtx.yOffset, color);
        x += mtx.advanceWidth;
        free(image.pixels);
        ogid = gid;
    }

    bitmap.dim.width = canvas.width;
    bitmap.dim.height = canvas.height;
    bitmap.data = canvas.pixels;

    sft_freefont(sft.font);
    return bitmap;
}