#pragma once

#include "v4_common.h"
#include "v4_aud.h"
#include "v4_config.h"
#include "v4_isp.h"
#include "v4_rgn.h"
#include "v4_snr.h"
#include "v4_sys.h"
#include "v4_vb.h"
#include "v4_venc.h"
#include "v4_vi.h"
#include "v4_vpss.h"

#include <fcntl.h>
#include <pthread.h>
#include <sys/ioctl.h>
#include <unistd.h>

extern char keepRunning;

extern hal_chnstate v4_state[V4_VENC_CHN_NUM];
extern int (*v4_aud_cb)(hal_audframe*);
extern int (*v4_venc_cb)(char, hal_vidstream*);

void v4_hal_deinit(void);
int v4_hal_init(void);

void v4_audio_deinit(void);
int v4_audio_init(void);

int v4_channel_bind(char index);
int v4_channel_create(char index, char mirror, char flip, char framerate);
int v4_channel_grayscale(char enable);
int v4_channel_unbind(char index);

void *v4_image_thread(void);

int v4_pipeline_create(void);
void v4_pipeline_destroy(void);

int v4_region_create(char handle, hal_rect rect, short opacity);
void v4_region_destroy(char handle);
int v4_region_setbitmap(int handle, hal_bitmap *bitmap);

void v4_sensor_deconfig(void);
int v4_sensor_config(void);
void v4_sensor_deinit(void);
int v4_sensor_init(char *name, char *obj);

int v4_video_create(char index, hal_vidconfig *config);
int v4_video_destroy(char index);
int v4_video_destroy_all(void);
void v4_video_request_idr(char index);
int v4_video_snapshot_grab(char index, hal_jpegdata *jpeg);
void *v4_video_thread(void);

int v4_system_calculate_block(short width, short height, v4_common_pixfmt pixFmt,
    unsigned int alignWidth);
void v4_system_deinit(void);
int v4_system_init(char *snrConfig);