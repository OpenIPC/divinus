#include <stdint.h>

#include "check.h"
#include "motion_core.h"

#define W 320
#define H 180
#define STRIDE 384

static uint8_t frame[STRIDE * H];

static void fill(uint8_t value) {
    memset(frame, value, sizeof(frame));
}

static void square(int x0, int y0, int size, uint8_t value) {
    for (int y = y0; y < y0 + size && y < H; y++)
        for (int x = x0; x < x0 + size && x < W; x++)
            frame[y * STRIDE + x] = value;
}

static void noise(int amplitude, unsigned int *seed) {
    for (int i = 0; i < sizeof(frame); i++) {
        *seed = *seed * 1103515245 + 12345;
        frame[i] = 100 + (int)((*seed >> 16) % (2 * amplitude + 1)) - amplitude;
    }
}

int main(void) {
    motion_core core;
    int events;

    CHECK(motion_core_init(&core, W, H) == EXIT_SUCCESS);
    motion_core_tune(&core, 5, 5);

    // A still scene never reports motion
    events = 0;
    for (int i = 0; i < 50; i++) {
        fill(80);
        events += motion_core_feed(&core, frame, STRIDE) != MOTION_NONE;
    }
    CHECK(events == 0);

    // A moving 64x64 square starts motion on the second analysed frame
    int started = -1;
    for (int i = 0; i < 10; i++) {
        fill(80);
        square(16 + i * 16, 48, 64, 220);
        if (motion_core_feed(&core, frame, STRIDE) == MOTION_START && started < 0) started = i;
    }
    CHECK(started == 1);
    CHECK(core.state);

    // Motion ends after hold frames without movement
    int ended = -1;
    for (int i = 0; i < 30; i++) {
        fill(80);
        if (motion_core_feed(&core, frame, STRIDE) == MOTION_END) ended = i;
    }
    CHECK(ended >= 4 && ended <= 15);
    CHECK(!core.state);

    // A global change (lights on) is ignored and the background is rebuilt
    events = 0;
    for (int i = 0; i < 30; i++) {
        fill(200);
        events += motion_core_feed(&core, frame, STRIDE) != MOTION_NONE;
    }
    CHECK(events == 0);

    // Sensor noise below the threshold never reports motion
    unsigned int seed = 1;
    events = 0;
    for (int i = 0; i < 50; i++) {
        noise(10, &seed);
        events += motion_core_feed(&core, frame, STRIDE) != MOTION_NONE;
    }
    CHECK(events == 0);

    // A pause hides movement until the background is rebuilt
    motion_core_pause(&core, 10);
    events = 0;
    for (int i = 0; i < 10; i++) {
        fill(80);
        square(16 + i * 16, 48, 64, 220);
        events += motion_core_feed(&core, frame, STRIDE) == MOTION_START;
    }
    CHECK(events == 0);

    // Reset returns the previous state
    for (int i = 0; i < 5; i++) {
        fill(80);
        square(16 + i * 16, 48, 64, 220);
        motion_core_feed(&core, frame, STRIDE);
    }
    CHECK(motion_core_reset(&core) == true);
    CHECK(motion_core_reset(&core) == false);

    motion_core_free(&core);
    CHECK_DONE();
}
