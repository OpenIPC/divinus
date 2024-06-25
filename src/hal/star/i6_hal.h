#pragma once

#include "i6_common.h"
#include "i6_aud.h"
#include "i6_isp.h"
#include "i6_rgn.h"
#include "i6_snr.h"
#include "i6_sys.h"
#include "i6_venc.h"
#include "i6_vif.h"
#include "i6_vpe.h"

#include "../support.h"

extern char keepRunning;

extern int (*i6_aud_cb)(hal_audframe*);
extern int (*i6_vid_cb)(char, hal_vidstream*);

void i6_hal_deinit(void);
int i6_hal_init(void);
void *i6_audio_thread(void);

void i6_audio_deinit(void);
int i6_audio_init(int samplerate);

int i6_channel_bind(char index, char framerate);
int i6_channel_create(char index, short width, short height, char mirror, char flip, char jpeg);
int i6_channel_grayscale(char enable);
int i6_channel_unbind(char index);

int i6_config_load(char *path);

int i6_pipeline_create(char sensor, short width, short height, char framerate);
void i6_pipeline_destroy(void);

int i6_region_create(char handle, hal_rect rect, short opacity);
void i6_region_deinit(void);
void i6_region_destroy(char handle);
void i6_region_init(void);
int i6_region_setbitmap(int handle, hal_bitmap *bitmap);

int i6_video_create(char index, hal_vidconfig *config);
int i6_video_destroy(char index);
int i6_video_destroy_all(void);
void i6_video_request_idr(char index);
int i6_video_snapshot_grab(char index, char quality, hal_jpegdata *jpeg);
void *i6_video_thread(void);

void i6_system_deinit(void);
int i6_system_init(void);