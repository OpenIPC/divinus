#include "motion.h"

#define MOTION_WIDTH 320
#define MOTION_HEIGHT 180
#define MOTION_FPS 5
#define MOTION_STALL_S 10

static char motionOn = 0;
static pthread_t motionPid = 0;
static pthread_mutex_t pauseMtx = PTHREAD_MUTEX_INITIALIZER;
static int pauseFrames = 0;

void motion_pause(int ms) {
    pthread_mutex_lock(&pauseMtx);
    pauseFrames = ms * MOTION_FPS / 1000;
    pthread_mutex_unlock(&pauseMtx);
}

static void motion_report(int change) {
    if (change == MOTION_NONE) return;
    HAL_INFO("motion", "Motion %s\n", change == MOTION_START ? "started" : "ended");
    onvif_motion_notify(change == MOTION_START, time(NULL));
}

static void *motion_thread(void) {
    motion_core core;
    hal_rawframe frame;
    int missed = 0;
    bool reopened = false;

    if (motion_core_init(&core, MOTION_WIDTH, MOTION_HEIGHT)) {
        HAL_DANGER("motion", "Can't allocate the detector!\n");
        return NULL;
    }
    motion_core_tune(&core, app_config.motion_detect_sensitivity,
        app_config.motion_detect_hold_s * MOTION_FPS);

    while (keepRunning && motionOn) {
        usleep(1000000 / MOTION_FPS);

        pthread_mutex_lock(&pauseMtx);
        if (pauseFrames) {
            motion_core_pause(&core, pauseFrames);
            pauseFrames = 0;
        }
        pthread_mutex_unlock(&pauseMtx);

        if (raw_get(&frame)) {
            if (++missed < MOTION_STALL_S * MOTION_FPS) continue;
            HAL_WARNING("motion", "No frames for %d seconds!\n", MOTION_STALL_S);
            if (motion_core_reset(&core)) motion_report(MOTION_END);
            if (reopened) {
                HAL_DANGER("motion", "Frames did not come back, detection stopped!\n");
                break;
            }
            raw_destroy();
            if (raw_create(MOTION_WIDTH, MOTION_HEIGHT)) break;
            reopened = true;
            missed = 0;
            continue;
        }
        missed = 0;

        if (frame.width < MOTION_WIDTH || frame.height < MOTION_HEIGHT ||
            frame.stride < MOTION_WIDTH) {
            raw_release(&frame);
            continue;
        }

        int change = motion_core_feed(&core, frame.luma, frame.stride);
        raw_release(&frame);
        motion_report(change);
    }

    if (motion_core_reset(&core)) motion_report(MOTION_END);
    motion_core_free(&core);
    HAL_INFO("motion", "Motion detection thread is closing...\n");
    return NULL;
}

int motion_start(void) {
    if (motionOn) return EXIT_SUCCESS;

    if (raw_create(MOTION_WIDTH, MOTION_HEIGHT)) {
        HAL_WARNING("motion", "Motion detection is disabled!\n");
        return EXIT_FAILURE;
    }

    motionOn = 1;
    pthread_attr_t thread_attr;
    pthread_attr_init(&thread_attr);
    size_t new_stacksize = 16 * 1024;
    if (pthread_attr_setstacksize(&thread_attr, new_stacksize))
        HAL_DANGER("motion", "Can't set stack size %zu\n", new_stacksize);
    if (pthread_create(&motionPid, &thread_attr, (void *(*)(void *))motion_thread, NULL)) {
        HAL_DANGER("motion", "Can't create thread\n");
        motionOn = 0;
        raw_destroy();
    }
    pthread_attr_destroy(&thread_attr);

    return motionOn ? EXIT_SUCCESS : EXIT_FAILURE;
}

void motion_stop(void) {
    if (!motionOn) return;

    motionOn = 0;
    pthread_join(motionPid, NULL);
    raw_destroy();
}
