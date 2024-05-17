#pragma once

#include "i6c_common.h"
#include "i6c_isp.h"
#include "i6c_rgn.h"
#include "i6c_scl.h"
#include "i6c_snr.h"
#include "i6c_sys.h"
#include "i6c_venc.h"
#include "i6c_vif.h"

extern char keepRunning;

extern int (*i6c_venc_cb)(char, hal_vidstream*);

void i6c_hal_deinit(void);
int i6c_hal_init(void);

int i6c_channel_bind(char index, char framerate, char jpeg);
int i6c_channel_create(char index, short width, short height, char jpeg);
int i6c_channel_grayscale(int index, char enable);
int i6c_channel_unbind(char index, char jpeg);

int i6c_config_load(char *path);

int i6c_encoder_create(char index, hal_vidconfig *config);
int i6c_encoder_destroy(char index, char jpeg);
int i6c_encoder_destroy_all(void);
int i6c_encoder_snapshot_grab(char index, short width, short height,
    char quality, char grayscale, hal_vidstream *stream);
void *i6c_encoder_thread(void);

int i6c_pipeline_create(char sensor, short width, short height, char framerate, char hdr);
void i6c_pipeline_destroy(void);

void i6c_system_deinit(void);
int i6c_system_init(void);