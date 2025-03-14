#pragma once

#include "ak_common.h"
#include "ak_aud.h"
#include "ak_sys.h"
#include "ak_venc.h"
#include "ak_vi.h"

extern char audioOn, keepRunning;

extern hal_chnstate ak_state[AK_VENC_CHN_NUM];
extern int (*ak_aud_cb)(hal_audframe*);
extern int (*ak_vid_cb)(char, hal_vidstream*);

void ak_hal_deinit(void);
int ak_hal_init(void);

void ak_audio_deinit(void);
int ak_audio_init(int samplerate);
void *ak_audio_thread(void);

int ak_channel_bind(char index);
int ak_channel_grayscale(char enable);
int ak_channel_unbind(char index);

int ak_config_load(char *path);

int ak_pipeline_create(char mirror, char flip);
void ak_pipeline_destroy(void);

int ak_video_create(char index, hal_vidconfig *config);
int ak_video_destroy(char index);
int ak_video_destroy_all(void);
void ak_video_request_idr(char index);
int ak_video_snapshot_grab(char index, hal_jpegdata *jpeg);
void *ak_video_thread(void);

void ak_system_deinit(void);
int ak_system_init(char *snrConfig);