#pragma once

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dlfcn.h>
#include <pthread.h>
#include <unistd.h>
#include <sys/stat.h>

#include "../symbols.h"
#include "../types.h"
#include "../globals.h"
#include "../tools.h"
#include "../../fmt/nal.h"

#define I3_VENC_CHN_NUM 3

extern hal_chnstate i3_state[I3_VENC_CHN_NUM];
extern int (*i3_aud_cb)(hal_audframe*);
extern int (*i3_vid_cb)(char, hal_vidstream*);

void i3_hal_deinit(void);
void i3_property_set(const char *name, const char *value);
int i3_hal_init(void);

void i3_audio_deinit(void);
int i3_audio_init(int samplerate, int gain);
void *i3_audio_thread(void);
int i3_region_create(char handle, hal_rect rect, short opacity);
void i3_region_deinit(void);
void i3_region_destroy(char handle);
void i3_region_init(void);
int i3_region_setbitmap(int handle, hal_bitmap *bitmap);
int i3_speaker_write(const short *pcm, unsigned int samples);
int i3_speaker_volume(int volume);

int i3_channel_bind(char index, char framerate);
int i3_channel_create(char index, short width, short height, char jpeg);
int i3_channel_grayscale(char enable);
int i3_channel_unbind(char index);
int i3_exposure_info(unsigned int *sensorGain, unsigned int *ispGain,
    unsigned int *shutterUs, unsigned int *avgY, int *bv);

int i3_config_load(char *path);

int i3_pipeline_create(char sensor, short width, short height, char mirror, char flip, char framerate);
void i3_pipeline_destroy(void);

int i3_video_create(char index, hal_vidconfig *config);
int i3_video_destroy(char index);
int i3_video_destroy_all(void);
void i3_video_request_idr(char index);
int i3_video_snapshot_grab(char index, char quality, hal_jpegdata *jpeg);
void *i3_video_thread(void);

void i3_system_deinit(void);
int i3_system_init(void);

int i3_system_identify(void);
