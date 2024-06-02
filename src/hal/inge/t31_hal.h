#pragma once

#include "t31_common.h"
#include "t31_aud.h"
#include "t31_fs.h"
#include "t31_isp.h"
#include "t31_osd.h"
#include "t31_sys.h"
#include "t31_venc.h"

#include "../config.h"

extern char keepRunning;

extern hal_chnstate t31_state[T31_VENC_CHN_NUM];
extern int (*t31_venc_cb)(char, hal_vidstream*);

void t31_hal_deinit(void);
int t31_hal_init(void);

void t31_audio_deinit(void);
int t31_audio_init(void);

int t31_channel_bind(char index);
int t31_channel_create(char index, short width, short height, char framerate);
void t31_channel_destroy(char index);
int t31_channel_grayscale(char enable);
int t31_channel_unbind(char index);

int t31_region_create(int *handle, hal_rect rect);
void t31_region_destroy(int *handle);
int t31_region_setbitmap(int *handle, hal_bitmap *bitmap);

int t31_video_create(char index, hal_vidconfig *config);
int t31_video_destroy(char index);
int t31_video_destroy_all(void);
void *t31_video_thread(void);

void t31_system_deinit(void);
int t31_system_init(void);
