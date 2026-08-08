#pragma once

#define AUD_RING_CAP (1 << 16) /* 64 KB, roughly 0.7 s of 48 kHz mono */
#define AUD_FRAME_MAX 16384    /* largest frame any HAL produces */

#include <pthread.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include <unistd.h>

#include "app_config.h"
#include "error.h"
#include "hal/globals.h"
#include "hal/ringbuf.h"
#include "hal/types.h"
#include "http_post.h"
#include "jpeg.h"
#include "lib/shine/layer3.h"
#include "rtmp.h"
#include "rtsp/rtsp_server.h"
#include "server.h"
#include "stream.h"

extern rtsp_handle rtspHandle;

int sdk_start(void);
int sdk_stop(void);

int media_start(void);
void media_stop(void);

void request_idr(void);
void set_grayscale(bool active);
int take_next_free_channel(bool mainLoop);

int create_channel(char index, short width, short height, char framerate, char jpeg);
int bind_channel(char index, char framerate, char jpeg);
int unbind_channel(char index, char jpeg);
int media_video_disable(char index, char jpeg);

void media_audio_disable(void);
int media_audio_enable(void);
int media_mjpeg_disable(void);
int media_mjpeg_enable(void);
int media_mp4_disable(void);
int media_mp4_enable(void);
