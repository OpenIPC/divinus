#pragma once

#include <stdbool.h>
#include <stdint.h>

#define MOTION_MAX_WIDTH 640

enum { MOTION_NONE, MOTION_START, MOTION_END };

typedef struct {
    int width, height;
    int threshold, minBlocks, holdFrames;
    uint16_t *background;
    bool primed, state;
    int streak, quiet, settle;
} motion_core;

int motion_core_init(motion_core *core, int width, int height);
void motion_core_free(motion_core *core);
void motion_core_tune(motion_core *core, int sensitivity, int holdFrames);
void motion_core_pause(motion_core *core, int frames);
bool motion_core_reset(motion_core *core);
int motion_core_feed(motion_core *core, const uint8_t *luma, int stride);
