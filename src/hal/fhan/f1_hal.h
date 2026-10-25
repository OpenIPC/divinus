#pragma once

#include "f1_common.h"
#include "f1_aud.h"
#include "f1_isp.h"
#include "f1_osd.h"
#include "f1_snr.h"
#include "f1_sys.h"
#include "f1_venc.h"
#include "f1_vpss.h"
#include "fh_lamp.h"

#include "../config.h"
#include "../globals.h"
#include "../../gpio.h"

#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <pthread.h>
#include <unistd.h>

extern hal_chnstate f1_state[F1_VENC_CHN_NUM];
extern int (*f1_aud_cb)(hal_audframe*);
extern int (*f1_vid_cb)(char, hal_vidstream*);

void f1_hal_deinit(void);
int f1_hal_init(void);

void f1_audio_deinit(void);
int f1_audio_init(int samplerate);
void *f1_audio_thread(void);

int f1_channel_bind(char index);
int f1_channel_create(char index, short width, short height, char framerate, char jpeg);
int f1_channel_grayscale(char enable);
int f1_channel_unbind(char index);

int f1_config_load(char *path);

void *f1_image_thread(void);

int f1_night_available(void);
int f1_night_status(void);

int f1_pipeline_create(short width, short height, char mirror, char flip, char framerate, char antiflicker);
void f1_pipeline_destroy(void);

int f1_region_create(int *handle, hal_rect rect, short opacity);
void f1_region_destroy(int *handle);
int f1_region_setbitmap(int *handle, hal_bitmap *bitmap);

int f1_video_create(char index, hal_vidconfig *config);
int f1_video_destroy(char index);
int f1_video_destroy_all(void);
void f1_video_request_idr(char index);
int f1_video_snapshot_grab(signed char index, hal_jpegdata *jpeg);
void *f1_video_thread(void);

void f1_system_deinit(void);
int f1_system_init(void);
