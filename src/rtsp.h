#pragma once

#include <errno.h>
#include <fcntl.h>
#include <netdb.h>
#include <netinet/in.h>
#include <pthread.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>

#include "app_config.h"
#include "hal/support.h"
#include "lib/uev/uev.h"

#define RTSP_MAXIMUM_CONNECTIONS 16
#define RTSP_MAXIMUM_STREAMS 2
#define RTSP_MAXIMUM_TIMEOUT 100
#define RTSP_READ_BUFFER_SIZE 4096
#define RTSP_WRITE_BUFFER_SIZE 4096

int rtsp_init(int priority);
void rtsp_finish(void);