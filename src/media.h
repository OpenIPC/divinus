#pragma once

#include "common.h"
#include "rtsp/rtsp_server.h"

#include <sys/time.h>

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