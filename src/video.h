#pragma once

#include "common.h"

int start_sdk();
int stop_sdk();

int take_next_free_channel(bool mainLoop);
void set_grayscale(bool active);

int create_vpss_chn(char index, short width, short height, char framerate, char jpeg);
int bind_vpss_venc(char index, char framerate, char jpeg);
int unbind_vpss_venc(char index, char jpeg);
int disable_venc_chn(char index, char jpeg);