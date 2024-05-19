#pragma once

#include "rtsputils.h"

#define RTP_DEFAULT_PORT 5004

void rtsp_eventloop(int mainFd);
void rtsp_interrupt(int signal);
void rtsp_portpool_init(int port);
void *rtsp_schedule_thread(void);
int rtsp_server(rtspBuffer *rtsp);
void rtsp_update_sps(unsigned char *data, int len);
void rtsp_update_pps(unsigned char *data, int len);