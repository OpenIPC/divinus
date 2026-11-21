#pragma once

#include "types.h"

/* Standard Linux backend: a V4L2 capture node feeding V4L2 memory-to-memory
 * encoders, V4L2 output nodes for overlays and ALSA for audio capture.
 * It is built only with -DDIVINUS_V4L2; otherwise every call is a stub and
 * v4l2_system_identify() never claims the platform. */

extern int (*v4l2_aud_cb)(hal_audframe*);
extern int (*v4l2_vid_cb)(char, hal_vidstream*);

int v4l2_system_identify(void);
int v4l2_hal_init(void);
void v4l2_hal_deinit(void);

int v4l2_audio_init(int samplerate);
void v4l2_audio_deinit(void);
void *v4l2_audio_thread(void);

int v4l2_channel_bind(char index);
int v4l2_channel_create(char index, short width, short height, char jpeg);
void v4l2_channel_grayscale(char enable);
int v4l2_channel_unbind(char index);

int v4l2_pipeline_create(short width, short height, char mirror, char flip,
    char framerate, int antiflicker);
void v4l2_pipeline_destroy(void);

int v4l2_region_create(char handle, hal_rect rect, short opacity);
void v4l2_region_destroy(char handle);
int v4l2_region_setbitmap(char handle, hal_bitmap *bitmap);

int v4l2_video_create(char index, hal_vidconfig *config);
int v4l2_video_destroy(char index);
int v4l2_video_destroy_all(void);
void v4l2_video_request_idr(char index);
int v4l2_video_snapshot_grab(char index, char quality, hal_jpegdata *jpeg);
void *v4l2_video_thread(void);
