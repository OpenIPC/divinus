#pragma once

#include "f2_common.h"
#include "f2_aud.h"
#include "f2_isp.h"
#include "f2_osd.h"
#include "f2_snr.h"
#include "f2_sys.h"
#include "f2_venc.h"
#include "f2_vpss.h"
#include "fh_lamp.h"

#include "../config.h"
#include "../globals.h"
#include "../../gpio.h"

#include <dirent.h>
#include <fcntl.h>
#include <pthread.h>
#include <unistd.h>

extern hal_chnstate f2_state[F2_VENC_CHN_NUM];
extern int (*f2_aud_cb)(hal_audframe*);
extern int (*f2_vid_cb)(char, hal_vidstream*);

void f2_hal_deinit(void);
int f2_hal_init(void);

void f2_audio_deinit(void);
int f2_audio_init(int samplerate);
void *f2_audio_thread(void);

int f2_channel_bind(char index);
int f2_channel_create(char index, short width, short height, char framerate, char jpeg);
int f2_channel_grayscale(char enable);
int f2_channel_unbind(char index);

int f2_config_load(char *path);

void *f2_image_thread(void);

int f2_night_available(void);
int f2_night_status(void);
int f2_night_thresholds(unsigned short night, unsigned short day);

int f2_pipeline_create(short width, short height, char mirror, char flip, char framerate, char antiflicker);
void f2_pipeline_destroy(void);

int f2_region_create(int *handle, hal_rect rect, short opacity);
void f2_region_destroy(int *handle);
int f2_region_setbitmap(int *handle, hal_bitmap *bitmap);

int f2_video_create(char index, hal_vidconfig *config);
int f2_video_destroy(char index);
int f2_video_destroy_all(void);
void f2_video_request_idr(char index);
int f2_video_snapshot_grab(signed char index, hal_jpegdata *jpeg);
void *f2_video_thread(void);

void f2_system_deinit(void);
int f2_system_init(void);
