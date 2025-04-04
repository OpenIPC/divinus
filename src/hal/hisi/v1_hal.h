#pragma once

#include "v1_common.h"
#include "v1_aud.h"
#include "v1_config.h"
#include "v1_isp.h"
#include "v1_rgn.h"
#include "v1_snr.h"
#include "v1_sys.h"
#include "v1_vb.h"
#include "v1_venc.h"
#include "v1_vi.h"
#include "v1_vpss.h"

#include <fcntl.h>
#include <sys/ioctl.h>
#include <sys/select.h>
#include <unistd.h>

extern char audioOn, keepRunning;

extern hal_chnstate v1_state[V1_VENC_CHN_NUM];
extern int (*v1_aud_cb)(hal_audframe*);
extern int (*v1_vid_cb)(char, hal_vidstream*);

void v1_hal_deinit(void);
int v1_hal_init(void);

void v1_audio_deinit(void);
int v1_audio_init(int samplerate);
void *v1_audio_thread(void);

int v1_channel_bind(char index);
int v1_channel_create(char index, short width, short height, char mirror, char flip, char framerate);
int v1_channel_grayscale(char enable);
int v1_channel_unbind(char index);

void *v1_image_thread(void);

int v1_pipeline_create(void);
void v1_pipeline_destroy(void);

int v1_region_create(char handle, hal_rect rect, short opacity);
void v1_region_destroy(char handle);
int v1_region_setbitmap(int handle, hal_bitmap *bitmap);

void v1_sensor_deconfig(void);
void v1_sensor_deinit(void);
int v1_sensor_init(char *name, char *obj);

int v1_video_create(char index, hal_vidconfig *config);
int v1_video_destroy(char index);
int v1_video_destroy_all(void);
void v1_video_request_idr(char index);
int v1_video_snapshot_grab(char index, hal_jpegdata *jpeg);
void *v1_video_thread(void);

int v1_system_calculate_block(short width, short height, v1_common_pixfmt pixFmt,
    unsigned int alignWidth);
void v1_system_deinit(void);
int v1_system_init(char *snrConfig);