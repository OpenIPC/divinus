#pragma once

#include <pthread.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include <unistd.h>

#include "app_config.h"
#include "error.h"
#include "hal/types.h"
#include "http_post.h"
#include "lib/shine/layer3.h"
#include "jpeg.h"
#include "rtsp/rtsp_server.h"
#include "server.h"

extern rtsp_handle rtspHandle;

int start_sdk(void);
int stop_sdk(void);

void request_idr(void);
void set_grayscale(bool active);
int take_next_free_channel(bool mainLoop);

int create_channel(char index, short width, short height, char framerate, char jpeg);
int bind_channel(char index, char framerate, char jpeg);
int unbind_channel(char index, char jpeg);
int disable_video(char index, char jpeg);

void disable_audio(void);
int enable_audio(void);
int disable_mjpeg(void);
int enable_mjpeg(void);
int disable_mp4(void);
int enable_mp4(void);