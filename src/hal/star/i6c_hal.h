#pragma once

#include "i6c_common.h"
#include "i6c_aud.h"
#include "i6c_ipu.h"
#include "i6c_isp.h"
#include "i6c_rgn.h"
#include "i6c_scl.h"
#include "i6c_snr.h"
#include "i6c_sys.h"
#include "i6c_venc.h"
#include "i6c_vif.h"

#include <sys/select.h>
#include <unistd.h>

extern char audioOn, keepRunning;

extern hal_chnstate i6c_state[I6C_VENC_CHN_NUM];
extern int (*i6c_aud_cb)(hal_audframe*);
extern int (*i6c_vid_cb)(char, hal_vidstream*);

void i6c_hal_deinit(void);
int i6c_hal_init(void);

void i6c_audio_deinit(void);
int i6c_audio_init(int samplerate, int gain);
void *i6c_audio_thread(void);

int i6c_channel_bind(char index, char framerate);
int i6c_channel_create(char index, short width, short height, char mirror, char flip, char jpeg);
int i6c_channel_grayscale(char enable);
int i6c_channel_unbind(char index);

int i6c_config_load(char *path);

int i6c_pipeline_create(char sensor, short width, short height, char framerate);
void i6c_pipeline_destroy(void);

int i6c_region_create(char handle, hal_rect rect, short opacity);
void i6c_region_deinit(void);
void i6c_region_destroy(char handle);
void i6c_region_init(void);
int i6c_region_setbitmap(int handle, hal_bitmap *bitmap);

int i6c_video_create(char index, hal_vidconfig *config);
int i6c_video_destroy(char index);
int i6c_video_destroy_all(void);
void i6c_video_request_idr(char index);
int i6c_video_snapshot_grab(char index, char quality, hal_jpegdata *jpeg);
void *i6c_video_thread(void);

void i6c_system_deinit(void);
int i6c_system_init(void);