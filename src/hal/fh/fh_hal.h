#pragma once

#include "fh_common.h"
#include "fh_aud.h"
#include "fh_isp.h"
#include "fh_snr.h"
#include "fh_sys.h"
#include "fh_venc.h"
#include "fh_vpss.h"

#include "../config.h"
#include "../globals.h"

#include <unistd.h>

extern hal_chnstate fh_state[FH_VENC_CHN_NUM];
extern int (*fh_aud_cb)(hal_audframe*);
extern int (*fh_vid_cb)(char, hal_vidstream*);

void fh_hal_deinit(void);
int fh_hal_init(void);

void fh_audio_deinit(void);
int fh_audio_init(int samplerate);
void *fh_audio_thread(void);

int fh_channel_bind(char index);
int fh_channel_create(char index, short width, short height, char framerate, char jpeg);
int fh_channel_grayscale(char enable);
int fh_channel_unbind(char index);

int fh_config_load(char *path);

void *fh_image_thread(void);

int fh_pipeline_create(short width, short height, char mirror, char flip, char framerate, char antiflicker);
int fh_set_antiflicker(char hz);
int fh_night_available(void);
int fh_night_status(void);
int fh_night_thresholds(unsigned short night, unsigned short day);
int fh_irled(char enable);
int fh_whitelamp(char enable);
void fh_pipeline_destroy(void);

int fh_region_create(int *handle, hal_rect rect, short opacity);
void fh_region_destroy(int *handle);
int fh_region_setbitmap(int *handle, hal_bitmap *bitmap);

int fh_video_create(char index, hal_vidconfig *config);
int fh_video_destroy(char index);
int fh_video_destroy_all(void);
void fh_video_request_idr(char index);
int fh_video_snapshot_grab(signed char index, hal_jpegdata *jpeg);
void *fh_video_thread(void);

void fh_system_deinit(void);
int fh_system_init(char *snrConfig);
