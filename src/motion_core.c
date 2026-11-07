#include "motion_core.h"

#include <stdlib.h>
#include <string.h>

#define BLOCK 16
#define BG_SHIFT 4
#define ENTER_FRAMES 2
#define SETTLE_FRAMES 10

int motion_core_init(motion_core *core, int width, int height) {
    memset(core, 0, sizeof(*core));
    if (width % BLOCK || width > MOTION_MAX_WIDTH || height < BLOCK)
        return EXIT_FAILURE;

    core->width = width;
    core->height = height / BLOCK * BLOCK;
    if (!(core->background = malloc(core->width * core->height * sizeof(*core->background))))
        return EXIT_FAILURE;

    motion_core_tune(core, 5, 25);
    return EXIT_SUCCESS;
}

void motion_core_free(motion_core *core) {
    free(core->background);
    core->background = NULL;
}

void motion_core_tune(motion_core *core, int sensitivity, int holdFrames) {
    core->threshold = 40 - 3 * sensitivity;
    core->minBlocks = 11 - sensitivity;
    core->holdFrames = holdFrames;
}

void motion_core_pause(motion_core *core, int frames) {
    core->settle = frames;
}

bool motion_core_reset(motion_core *core) {
    bool state = core->state;

    core->primed = core->state = false;
    core->streak = core->quiet = core->settle = 0;
    return state;
}

static int motion_core_step(motion_core *core, bool moving) {
    if (moving) {
        core->quiet = 0;
        if (!core->state && ++core->streak >= ENTER_FRAMES) {
            core->state = true;
            core->streak = 0;
            return MOTION_START;
        }
    } else {
        core->streak = 0;
        if (core->state && ++core->quiet >= core->holdFrames) {
            core->state = false;
            core->quiet = 0;
            return MOTION_END;
        }
    }

    return MOTION_NONE;
}

int motion_core_feed(motion_core *core, const uint8_t *luma, int stride) {
    int blocksX = core->width / BLOCK, blocksY = core->height / BLOCK;
    int changed = 0, active = 0;

    if (core->settle) {
        core->settle--;
        core->primed = false;
        return motion_core_step(core, false);
    }

    // Background in 8.8 fixed point, alpha 1/16: about 3 s of memory at 5 fps
    if (!core->primed) {
        for (int y = 0; y < core->height; y++)
            for (int x = 0; x < core->width; x++)
                core->background[y * core->width + x] = luma[y * stride + x] << 8;
        core->primed = true;
        return motion_core_step(core, false);
    }

    for (int by = 0; by < blocksY; by++) {
        int counts[MOTION_MAX_WIDTH / BLOCK] = {0};

        for (int y = by * BLOCK; y < (by + 1) * BLOCK; y++) {
            const uint8_t *row = luma + y * stride;
            uint16_t *bg = core->background + y * core->width;
            for (int x = 0; x < core->width; x++) {
                int diff = row[x] - (bg[x] >> 8);
                if (diff > core->threshold || -diff > core->threshold) {
                    counts[x / BLOCK]++;
                    changed++;
                }
                bg[x] += (((int)row[x] << 8) - bg[x]) >> BG_SHIFT;
            }
        }

        for (int bx = 0; bx < blocksX; bx++)
            if (counts[bx] > BLOCK * BLOCK / 4) active++;
    }

    // Lights switching or the IR cut filter moving: rebuild the background
    if (changed > core->width * core->height / 2) {
        core->settle = SETTLE_FRAMES;
        return motion_core_step(core, false);
    }

    return motion_core_step(core, active >= core->minBlocks);
}
