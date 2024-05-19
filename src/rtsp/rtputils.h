#pragma once

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>

#define MAX_RTP_PKT_LENGTH 1400

#define H264 96
#define G711 97

typedef enum {
    _h264 = 0x100,
    _h264nalu,
    _mjpeg,
    _g711 = 0x200,
} rtpPayload;

enum H264_FRAME_TYPE {
    FRAME_TYPE_I,
    FRAME_TYPE_P,
    FRAME_TYPE_B
};

unsigned int rtp_create(unsigned int ip, int port, rtpPayload payload);
void rtp_delete(unsigned int u32Rtp);
unsigned int rtp_send(unsigned int rtp, char *data, int size, unsigned int tstamp);