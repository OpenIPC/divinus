#pragma once

#include "../types.h"

#include <pthread.h>
#include <stdlib.h>
#include <string.h>

#define AK_OSD_MAX 10

typedef struct {
    hal_rect rect;
    hal_dim dim;
    short opacity;
    unsigned char *bits;
} ak_region_t;

typedef unsigned int ak_word __attribute__((may_alias));

static ak_region_t ak_region[AK_OSD_MAX];

/* The region thread swaps or frees bitmaps while the video thread paints
   them into the frame, so the table needs one lock for both sides. */
static pthread_mutex_t ak_region_mtx = PTHREAD_MUTEX_INITIALIZER;

static inline __attribute__((always_inline)) int ak_region_luma(unsigned short p)
{
    int r5 = (p >> 10) & 0x1F, g5 = (p >> 5) & 0x1F, b5 = p & 0x1F;
    int r8 = (r5 << 3) | (r5 >> 2);
    int g8 = (g5 << 3) | (g5 >> 2);
    int b8 = (b5 << 3) | (b5 >> 2);

    return (77 * r8 + 150 * g8 + 29 * b8) >> 8;
}

static inline __attribute__((always_inline)) void ak_region_chroma(unsigned short p, int *u, int *v)
{
    int r5 = (p >> 10) & 0x1F, g5 = (p >> 5) & 0x1F, b5 = p & 0x1F;
    int r8 = (r5 << 3) | (r5 >> 2);
    int g8 = (g5 << 3) | (g5 >> 2);
    int b8 = (b5 << 3) | (b5 >> 2);

    *u = ((-43 * r8 - 85 * g8 + 128 * b8) >> 8) + 128;
    *v = ((128 * r8 - 107 * g8 - 21 * b8) >> 8) + 128;
}

static inline __attribute__((always_inline)) void ak_region_blend(unsigned char *dst, int val, int op)
{
    *dst = *dst + (((val - *dst) * op) >> 8);
}

static inline __attribute__((always_inline)) void ak_region_draw(unsigned char *yrow,
    unsigned char *urow, unsigned char *vrow, int fx, unsigned short p,
    const char solid, int op, int *last_p, int *last_uv, int *Y, int *U, int *V)
{
    if (!(p & 0x8000)) return;
    if (p != *last_p) {
        *Y = ak_region_luma(p);
        if (solid) ak_region_chroma(p, U, V);
        *last_p = p;
    }

    if (!solid) {
        ak_region_blend(&yrow[fx], *Y, op);
        return;
    }

    yrow[fx] = *Y;
    if ((fx >> 1) != *last_uv) {
        *last_uv = fx >> 1;
        urow[*last_uv] = *U;
        vrow[*last_uv] = *V;
    }
}

static inline __attribute__((always_inline)) void ak_region_paint(
    unsigned char *frame, int width, int height, ak_region_t *r, const char solid)
{
    const int luma_size = width * height;
    const int chroma_size = (width >> 1) * (height >> 1);
    const int cwidth = width >> 1;
    const int bw = r->dim.width, bh = r->dim.height;
    const int stride = (bw + 1) & ~1;
    const int op = r->opacity;
    const unsigned short *src = (const unsigned short *)r->bits;
    const int x0 = r->rect.x, y0 = r->rect.y;

    int xe = width - x0, ye = height - y0;
    if (xe > bw) xe = bw;
    if (ye > bh) ye = bh;
    const int fxend = x0 + xe;
    int last_cy = -1;

    for (int y = 0; y < ye; y++) {
        const unsigned short *s = src + (size_t)y * stride;
        const ak_word *w = (const ak_word *)s;
        const int cy = (y0 + y) >> 1;
        unsigned char *yrow = frame + (size_t)(y0 + y) * width;
        unsigned char *urow = frame + luma_size + (size_t)cy * cwidth;
        unsigned char *vrow = urow + chroma_size;
        int last_p = -1, last_uv = -1, Y = 0, U = 0, V = 0;
        int x = 0;

        for (; x + 1 < xe; x += 2) {
            ak_word pair = w[x >> 1];
            if (!(pair & 0x80008000)) continue;
            ak_region_draw(yrow, urow, vrow, x0 + x, pair & 0xFFFF, solid, op,
                &last_p, &last_uv, &Y, &U, &V);
            ak_region_draw(yrow, urow, vrow, x0 + x + 1, pair >> 16, solid, op,
                &last_p, &last_uv, &Y, &U, &V);
        }
        if (x < xe)
            ak_region_draw(yrow, urow, vrow, x0 + x, s[x], solid, op,
                &last_p, &last_uv, &Y, &U, &V);

        if (solid || cy == last_cy) continue;
        last_cy = cy;

        const int rowA = (cy << 1) - y0;
        const int rowB = rowA + 1;
        const unsigned short *sa = (rowA >= 0 && rowA < ye) ?
            src + (size_t)rowA * stride : NULL;
        const unsigned short *sb = (rowB >= 0 && rowB < ye) ?
            src + (size_t)rowB * stride : NULL;
        int last_c = -1;

        for (int fx = x0 & ~1; fx < fxend; fx += 2) {
            int xa = fx - x0;
            unsigned short p = 0;

            if (sa) {
                if (xa >= 0) p = sa[xa];
                if (!(p & 0x8000) && xa + 1 < xe) p = sa[xa + 1];
            }
            if (!(p & 0x8000) && sb) {
                if (xa >= 0) p = sb[xa];
                if (!(p & 0x8000) && xa + 1 < xe) p = sb[xa + 1];
            }
            if (!(p & 0x8000)) continue;
            if (p != last_c) {
                ak_region_chroma(p, &U, &V);
                last_c = p;
            }
            ak_region_blend(&urow[fx >> 1], U, op);
            ak_region_blend(&vrow[fx >> 1], V, op);
        }
    }
}

static void ak_region_apply(unsigned char *frame, int width, int height)
{
    pthread_mutex_lock(&ak_region_mtx);
    for (int id = 0; id < AK_OSD_MAX; id++) {
        ak_region_t *r = &ak_region[id];
        if (!r->bits || r->opacity <= 0) continue;
        if (r->opacity == 255) ak_region_paint(frame, width, height, r, 1);
        else ak_region_paint(frame, width, height, r, 0);
    }
    pthread_mutex_unlock(&ak_region_mtx);
}
