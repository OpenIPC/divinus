#pragma once

#include "v3_common.h"
#include "v3_config.h"
#include "v3_isp.h"
#include "v3_rgn.h"
#include "v3_snr.h"
#include "v3_sys.h"
#include "v3_vb.h"
#include "v3_venc.h"
#include "v3_vi.h"
#include "v3_vpss.h"

#include <fcntl.h>

typedef struct {
    void* handle;
    int (*fnRegister)(void);
    int (*fnUnregister)(void);
} v3_drv_impl;

extern char keepRunning;

extern hal_chnstate v3_state[V3_VENC_CHN_NUM];
extern int (*v3_venc_cb)(char, hal_vidstream*);

void v3_hal_deinit(void);
int v3_hal_init(void);

int v3_channel_bind(char index);
int v3_channel_create(char index, short width, short height, char framerate);
int v3_channel_grayscale(char index, int enable);
int v3_channel_unbind(char index);

int v3_encoder_create(char index, hal_vidconfig *config);
int v3_encoder_destroy(char index);
int v3_encoder_destroy_all(void);
int v3_encoder_snapshot_grab(char index, short width, short height, 
    char quality, char grayscale, hal_vidstream *stream);
void *v3_encoder_thread(void);

void *v3_image_thread(void);

int v3_pipeline_create(char mirror, char flip);
void v3_pipeline_destroy(void);

int v3_sensor_config(void);
void v3_sensor_deinit(void);
int v3_sensor_init(char *name);

int v3_system_calculate_block(short width, short height, v3_common_pixfmt pixFmt,
    unsigned int alignWidth);
void v3_system_deinit(void);
int v3_system_init(unsigned int alignWidth, unsigned int blockCnt, 
    unsigned int poolCnt, char *snrConfig);