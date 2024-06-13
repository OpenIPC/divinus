#pragma once

#include "common.h"
#include "rtsp/rtsp_server.h"

#include <sys/time.h>

extern rtsp_handle rtspHandle;

int start_sdk(void);
int stop_sdk(void);

int take_next_free_channel(bool mainLoop);
void request_idr(void);
void set_grayscale(bool active);

int create_vpss_chn(char index, short width, short height, char framerate, char jpeg);
int bind_vpss_venc(char index, char framerate, char jpeg);
int unbind_vpss_venc(char index, char jpeg);
int disable_venc_chn(char index, char jpeg);