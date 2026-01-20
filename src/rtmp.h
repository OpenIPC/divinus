#pragma once

#include <arpa/inet.h>
#include <errno.h>
#include <netdb.h>
#include <netinet/in.h>
#include <pthread.h>
#include <stdbool.h>
#include <stdint.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <unistd.h>

#include "app_config.h"
#include "fmt/flv.h"
#include "fmt/nal.h"
#include "hal/macros.h"
#include "hal/tools.h"
#include "hal/types.h"

#define RTMP_PORT 1935
#define RTMP_SIG_SIZE 1536
#define RTMP_HEAD_SIZE 11
#define RTMP_CHUNK_SIZE 128
#define RTMP_DEFAULT_CHUNK_SIZE 128

#define RTMP_MSG_CHUNK_SIZE     1
#define RTMP_MSG_USER_CONTROL   4
#define RTMP_MSG_WINDOW_ACK_SIZE 5
#define RTMP_MSG_SET_PEER_BW    6
#define RTMP_MSG_AUDIO          8
#define RTMP_MSG_VIDEO          9
#define RTMP_MSG_AMF_META       18
#define RTMP_MSG_AMF_CMD        20

#define RTMP_CS_CMD             2
#define RTMP_CS_AUDIO           4
#define RTMP_CS_VIDEO           6

extern char keepRunning;

int rtmp_init(const char *url);
void rtmp_close(void);
int rtmp_ingest_video(hal_vidpack *packet, int is_h265);
int rtmp_ingest_audio(void *data, int len);