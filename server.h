#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "common.h"

extern bool keepRunning;

int start_server();
int stop_server();

void send_jpeg(unsigned char chn_index, char *buf, ssize_t size);
void send_mjpeg(unsigned char chn_index, char *buf, ssize_t size);
void send_h264_to_client(unsigned char chn_index, const void *p);
void send_mp4_to_client(unsigned char chn_index, const void *p);