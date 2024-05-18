#pragma once

#include "i6_common.h"
#include "i6_isp.h"
#include "i6_rgn.h"
#include "i6_snr.h"
#include "i6_sys.h"
#include "i6_venc.h"
#include "i6_vif.h"
#include "i6_vpe.h"

extern char keepRunning;

extern int (*i6_venc_cb)(char, hal_vidstream*);

void i6_hal_deinit(void);
int i6_hal_init(void);

int i6_channel_bind(char index, char framerate, char jpeg);
int i6_channel_create(char index, short width, short height, char mirror, char flip, char jpeg);
void i6_channel_disable(char index);
int i6_channel_enabled(char index);
int i6_channel_grayscale(char enable);
int i6_channel_in_mainloop(char index);
int i6_channel_next(char mainLoop);
int i6_channel_unbind(char index);

int i6_config_load(char *path);

int i6_encoder_create(char index, hal_vidconfig *config);
int i6_encoder_destroy(char index);
int i6_encoder_destroy_all(void);
int i6_encoder_snapshot_grab(char index, short width, short height, 
    char quality, char grayscale, hal_vidstream *stream);
void *i6_encoder_thread(void);

int i6_pipeline_create(char sensor, short width, short height, char framerate);
void i6_pipeline_destroy(void);

void i6_system_deinit(void);
int i6_system_init(void);