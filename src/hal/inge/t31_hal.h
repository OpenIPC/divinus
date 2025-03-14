#pragma once

#include "t31_common.h"
#include "t31_aud.h"
#include "t31_fs.h"
#include "t31_isp.h"
#include "t31_osd.h"
#include "t31_sys.h"
#include "t31_venc.h"

#include "../config.h"
#include "../support.h"

#include <sys/select.h>
#include <unistd.h>

extern char audioOn, keepRunning;

extern hal_chnstate t31_state[T31_VENC_CHN_NUM];
extern int (*t31_aud_cb)(hal_audframe*);
extern int (*t31_vid_cb)(char, hal_vidstream*);

void t31_hal_deinit(void);
int t31_hal_init(void);

void t31_audio_deinit(void);
int t31_audio_init(int samplerate);
void *t31_audio_thread(void);

int t31_channel_bind(char index);
int t31_channel_create(char index, short width, short height, char framerate, char jpeg);
int t31_channel_grayscale(char enable);
int t31_channel_unbind(char index);

int t31_config_load(char *path);

int t31_pipeline_create(char mirror, char flip, char antiflicker, char framerate);
void t31_pipeline_destroy(void);

int t31_region_create(int *handle, hal_rect rect, short opacity);
void t31_region_destroy(int *handle);
int t31_region_setbitmap(int *handle, hal_bitmap *bitmap);

int t31_video_create(char index, hal_vidconfig *config);
int t31_video_destroy(char index);
int t31_video_destroy_all(void);
void t31_video_request_idr(char index);
int t31_video_snapshot_grab(char index, hal_jpegdata *jpeg);
void *t31_video_thread(void);

void t31_system_deinit(void);
int t31_system_init(void);