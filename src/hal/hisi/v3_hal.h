#pragma once

#include "v3_common.h"
#include "v3_aud.h"
#include "v3_config.h"
#include "v3_isp.h"
#include "v3_rgn.h"
#include "v3_snr.h"
#include "v3_sys.h"
#include "v3_vb.h"
#include "v3_venc.h"
#include "v3_vi.h"
#include "v3_vpss.h"

#include "../support.h"

#include <fcntl.h>
#include <sys/ioctl.h>
#include <sys/select.h>
#include <unistd.h>

extern char audioOn, keepRunning;

extern hal_chnstate v3_state[V3_VENC_CHN_NUM];
extern int (*v3_aud_cb)(hal_audframe*);
extern int (*v3_vid_cb)(char, hal_vidstream*);

void v3_hal_deinit(void);
int v3_hal_init(void);

void v3_audio_deinit(void);
int v3_audio_init(int samplerate);
void *v3_audio_thread(void);

int v3_channel_bind(char index);
int v3_channel_create(char index, short width, short height, char mirror, char flip, char framerate);
int v3_channel_grayscale(char enable);
int v3_channel_unbind(char index);

void *v3_image_thread(void);

int v3_pipeline_create(void);
void v3_pipeline_destroy(void);

int v3_region_create(char handle, hal_rect rect, short opacity);
void v3_region_destroy(char handle);
int v3_region_setbitmap(int handle, hal_bitmap *bitmap);

void v3_sensor_deconfig(void);
int v3_sensor_config(void);
void v3_sensor_deinit(void);
int v3_sensor_init(char *name, char *obj);

int v3_video_create(char index, hal_vidconfig *config);
int v3_video_destroy(char index);
int v3_video_destroy_all(void);
void v3_video_request_idr(char index);
int v3_video_snapshot_grab(char index, hal_jpegdata *jpeg);
void *v3_video_thread(void);

int v3_system_calculate_block(short width, short height, v3_common_pixfmt pixFmt,
    unsigned int alignWidth);
void v3_system_deinit(void);
int v3_system_init(char *snrConfig);
float v3_system_readtemp(void);