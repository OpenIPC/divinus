// SPDX-License-Identifier: GPL-2.0
/*
 * Ingenic T23 V4L2/MC + Helix DMABUF + ALSA HAL header for Divinus
 *
 * Copyright (C) 2026 Difan Zhang
 */
#pragma once

#include "../config.h"
#include "../globals.h"
#include "../macros.h"
#include "../tools.h"

#include <unistd.h>

#define T23_VENC_CHN_NUM 4

extern hal_chnstate t23_state[T23_VENC_CHN_NUM];
extern int (*t23_aud_cb)(hal_audframe *);
extern int (*t23_vid_cb)(char, hal_vidstream *);

void t23_hal_deinit(void);
int t23_hal_init(void);

void t23_audio_deinit(void);
int t23_audio_init(int samplerate);
void *t23_audio_thread(void);

int t23_channel_bind(char index);
int t23_channel_create(char index, short width, short height, char framerate, char jpeg);
int t23_channel_grayscale(char enable);
int t23_channel_unbind(char index);

int t23_config_load(char *path);

int t23_pipeline_create(char mirror, char flip, char framerate, char antiflicker);
void t23_pipeline_destroy(void);

int t23_video_create(char index, hal_vidconfig *config);
int t23_video_destroy(char index);
int t23_video_destroy_all(void);
void t23_video_request_idr(char index);
int t23_video_snapshot_grab(short width, short height, char quality, hal_jpegdata *jpeg);
void *t23_video_thread(void);

void t23_system_deinit(void);
int t23_system_init(void);

int t23_system_identify(void);
