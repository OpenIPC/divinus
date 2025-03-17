#pragma once

#include "cvi_common.h"
#include "cvi_aud.h"
#include "cvi_config.h"
#include "cvi_isp.h"
#include "cvi_rgn.h"
#include "cvi_snr.h"
#include "cvi_sys.h"
#include "cvi_vb.h"
#include "cvi_venc.h"
#include "cvi_vi.h"
#include "cvi_vpss.h"

#include <sys/select.h>
#include <unistd.h>

extern char audioOn, keepRunning;

extern hal_chnstate cvi_state[CVI_VENC_CHN_NUM];
extern int (*cvi_aud_cb)(hal_audframe*);
extern int (*cvi_vid_cb)(char, hal_vidstream*);

void cvi_hal_deinit(void);
int cvi_hal_init(void);

void cvi_audio_deinit(void);
int cvi_audio_init(int samplerate);
void *cvi_audio_thread(void);

int cvi_channel_bind(char index);
int cvi_channel_create(char index, short width, short height, char mirror, char flip);
int cvi_channel_grayscale(char enable);
int cvi_channel_unbind(char index);

void *cvi_image_thread(void);

int cvi_pipeline_create(void);
void cvi_pipeline_destroy(void);

int cvi_region_create(char handle, hal_rect rect, short opacity);
void cvi_region_destroy(char handle);
int cvi_region_setbitmap(int handle, hal_bitmap *bitmap);

int cvi_sensor_config(void);
void cvi_sensor_deconfig(void);
void cvi_sensor_deinit(void);
int cvi_sensor_init(char *name, char *obj);

int cvi_video_create(char index, hal_vidconfig *config);
int cvi_video_destroy(char index);
int cvi_video_destroy_all(void);
void cvi_video_request_idr(char index);
int cvi_video_snapshot_grab(char index, hal_jpegdata *jpeg);
void *cvi_video_thread(void);

int cvi_system_calculate_block(short width, short height, cvi_common_pixfmt pixFmt,
    unsigned int alignWidth);
void cvi_system_deinit(void);
int cvi_system_init(char *snrConfig);