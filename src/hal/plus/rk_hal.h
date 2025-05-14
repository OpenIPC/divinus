#pragma once

#include "rk_common.h"
#include "rk_aiq.h"
#include "rk_aud.h"
#include "rk_mb.h"
#include "rk_rgn.h"
#include "rk_sys.h"
#include "rk_venc.h"
#include "rk_vi.h"
#include "rk_vpss.h"

#include "../support.h"

#include <fcntl.h>
#include <pthread.h>
#include <sys/ioctl.h>
#include <sys/select.h>
#include <unistd.h>

extern char audioOn, keepRunning;

extern hal_chnstate rk_state[RK_VENC_CHN_NUM];
extern int (*rk_aud_cb)(hal_audframe*);
extern int (*rk_vid_cb)(char, hal_vidstream*);

void rk_hal_deinit(void);
int rk_hal_init(void);

void rk_audio_deinit(void);
int rk_audio_init(int samplerate);
void *rk_audio_thread(void);

int rk_channel_bind(char index);
int rk_channel_create(char index, short width, short height, char mirror, char flip);
int rk_channel_grayscale(char enable);
int rk_channel_unbind(char index);

int rk_pipeline_create(short width, short height);
void rk_pipeline_destroy(void);

int rk_region_create(char handle, hal_rect rect, short opacity);
void rk_region_destroy(char handle);
int rk_region_setbitmap(int handle, hal_bitmap *bitmap);

int rk_sensor_find_v4l2_endpoint(void);

int rk_video_create(char index, hal_vidconfig *config);
int rk_video_destroy(char index);
int rk_video_destroy_all(void);
void rk_video_request_idr(char index);
int rk_video_snapshot_grab(char index, hal_jpegdata *jpeg);
void *rk_video_thread(void);

int rk_system_calculate_block(short width, short height, rk_common_pixfmt pixFmt,
    unsigned int alignWidth);
void rk_system_deinit(void);
int rk_system_init(char *snrConfig);