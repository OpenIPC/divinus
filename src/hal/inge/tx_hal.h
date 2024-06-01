#pragma once

#include "tx_common.h"
#include "tx_aud.h"
#include "tx_fs.h"
#include "tx_isp.h"
#include "tx_osd.h"
#include "tx_sys.h"
#include "tx_venc.h"

#include "../config.h"

extern char keepRunning;

extern hal_chnstate tx_state[TX_VENC_CHN_NUM];
extern int (*tx_venc_cb)(char, hal_vidstream*);

void tx_hal_deinit(void);
int tx_hal_init(void);

void tx_audio_deinit(void);
int tx_audio_init(void);

int tx_channel_bind(char index);
int tx_channel_create(char index, short width, short height, char framerate);
void tx_channel_destroy(char index);
int tx_channel_unbind(char index);

int tx_region_create(int *handle, hal_rect rect);
void tx_region_destroy(int *handle);
int tx_region_setbitmap(int *handle, hal_bitmap *bitmap);

int tx_video_create(char index, hal_vidconfig *config);
int tx_video_destroy(char index);
int tx_video_destroy_all(void);
void *tx_video_thread(void);

void tx_system_deinit(void);
int tx_system_init(void);
