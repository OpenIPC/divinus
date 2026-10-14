#pragma once

#include <stddef.h>

#include "hal/types.h"

#define RTSP_MAXIMUM_CONNECTIONS 16
#define RTSP_MAXIMUM_STREAMS 2

typedef struct __rtsp_obj_t *rtsp_handle;

void rtp_disable_audio(rtsp_handle h);
int rtp_send_h26x(rtsp_handle h, hal_vidstream *stream, char isH265);
int rtp_send_mp3(rtsp_handle h, unsigned char *buf, size_t len);
int rtp_send_pcma(rtsp_handle h, unsigned char *buf, size_t len);
int rtp_send_pcmu(rtsp_handle h, unsigned char *buf, size_t len);

void rtsp_finish(rtsp_handle h);
rtsp_handle rtsp_create(unsigned char max_con, unsigned int port, int priority);
void rtsp_configure_auth(rtsp_handle h, const char *user, const char *pass);

#ifdef RTSP_IMPLEMENTATION

#include <errno.h>
#include <fcntl.h>
#include <inttypes.h>
#include <netdb.h>
#include <netinet/in.h>
#include <pthread.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <time.h>
#include <unistd.h>

#include "app_config.h"
#include "fmt/nal.h"
#include "hal/macros.h"
#include "hal/tools.h"

#undef STARTS_WITH /* SmolRTSP defines its own copy */
#include "3rd/smolrtsp-all.h"

#include "3rd/uev-all.h"

#endif
