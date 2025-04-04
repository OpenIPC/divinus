#pragma once

#include <arpa/inet.h>
#include <errno.h>
#include <netinet/in.h>
#include <pthread.h>
#include <regex.h>
#include <signal.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <time.h>
#include <unistd.h>

#include "app_config.h"
#include "hal/types.h"
#include "jpeg.h"
#include "media.h"
#include "mp4/mp4.h"
#include "mp4/nal.h"
#include "network.h"
#include "night.h"
#include "region.h"
#include "watchdog.h"

extern char graceful, keepRunning;

int start_server();
int stop_server();

void send_jpeg_to_client(char index, char *buf, ssize_t size);
void send_mjpeg_to_client(char index, char *buf, ssize_t size);
void send_h26x_to_client(char index, hal_vidstream *stream);
void send_mp3_to_client(char *buf, ssize_t size);
void send_mp4_to_client(char index, hal_vidstream *stream, char isH265);
void send_pcm_to_client(hal_audframe *frame);