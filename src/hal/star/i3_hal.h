#pragma once

#include "i3_common.h"
#include "i3_aud.h"
#include "i3_isp.h"
#include "i3_osd.h"
#include "i3_sys.h"
#include "i3_venc.h"
#include "i3_vi.h"

#include "../support.h"

#include <sys/select.h>
#include <unistd.h>

extern char audioOn, keepRunning;

extern hal_chnstate i3_state[I3_VENC_CHN_NUM];
extern int (*i3_aud_cb)(hal_audframe*);
extern int (*i3_vid_cb)(char, hal_vidstream*);

void i3_hal_deinit(void);
int i3_hal_init(void);

/*void i3_audio_deinit(void);
int i3_audio_init(int samplerate);
void *i3_audio_thread(void);

int i3_channel_bind(char index, char framerate);
int i3_channel_create(char index, short width, short height, char mirror, char flip, char jpeg);
int i3_channel_grayscale(char enable);
int i3_channel_unbind(char index);*/

int i3_config_load(char *path);

/*int i3_pipeline_create(char sensor, short width, short height, char framerate);
void i3_pipeline_destroy(void);

int i3_region_create(char handle, hal_rect rect, short opacity);
void i3_region_destroy(char handle);
int i3_region_setbitmap(int handle, hal_bitmap *bitmap);

int i3_video_create(char index, hal_vidconfig *config);
int i3_video_destroy(char index);
int i3_video_destroy_all(void);
void i3_video_request_idr(char index);
int i3_video_snapshot_grab(char index, char quality, hal_jpegdata *jpeg);
void *i3_video_thread(void);*/

void i3_system_deinit(void);
int i3_system_init(void);