#pragma once

#include "v2_common.h"
#include "v2_aud.h"
#include "v2_config.h"
#include "v2_isp.h"
#include "v2_rgn.h"
#include "v2_snr.h"
#include "v2_sys.h"
#include "v2_vb.h"
#include "v2_venc.h"
#include "v2_vi.h"
#include "v2_vpss.h"

#include <fcntl.h>
#include <sys/ioctl.h>
#include <sys/select.h>
#include <unistd.h>

extern char audioOn, keepRunning;

extern hal_chnstate v2_state[V2_VENC_CHN_NUM];
extern int (*v2_aud_cb)(hal_audframe*);
extern int (*v2_vid_cb)(char, hal_vidstream*);

void v2_hal_deinit(void);
int v2_hal_init(void);

void v2_audio_deinit(void);
int v2_audio_init(int samplerate);
void *v2_audio_thread(void);

int v2_channel_bind(char index);
int v2_channel_create(char index, short width, short height, char mirror, char flip, char framerate);
int v2_channel_grayscale(char enable);
int v2_channel_unbind(char index);

void *v2_image_thread(void);

int v2_pipeline_create(void);
void v2_pipeline_destroy(void);

int v2_region_create(char handle, hal_rect rect, short opacity);
void v2_region_destroy(char handle);
int v2_region_setbitmap(int handle, hal_bitmap *bitmap);

void v2_sensor_deconfig(void);
int v2_sensor_config(void);
void v2_sensor_deinit(void);
int v2_sensor_init(char *name, char *obj);

int v2_video_create(char index, hal_vidconfig *config);
int v2_video_destroy(char index);
int v2_video_destroy_all(void);
void v2_video_request_idr(char index);
int v2_video_snapshot_grab(char index, hal_jpegdata *jpeg);
void *v2_video_thread(void);

int v2_system_calculate_block(short width, short height, v2_common_pixfmt pixFmt,
    unsigned int alignWidth);
void v2_system_deinit(void);
int v2_system_init(char *snrConfig);
float v2_system_readtemp(void);