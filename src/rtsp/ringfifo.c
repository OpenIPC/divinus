#include "ringfifo.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "rtputils.h"
#include "rtspservice.h"

#define SLOTS 32

int writePos = 0;
int readPos = 0;
int slot = 0;

struct ringbuf ringFifo[SLOTS];
extern int rtsp_update_sps_or_pps(unsigned char *data, int frame_type, int len);

void ring_malloc(int size) {
    for (int i = 0; i < SLOTS; i++) {
        ringFifo[i].buffer = malloc(size);
        ringFifo[i].size = 0;
        ringFifo[i].frame_type = 0;
    }
    writePos = 0;
    readPos = 0;
    slot = 0;
}

void ring_reset() {
    writePos = 0;
    readPos = 0;
    slot = 0;
}

void ring_free() {
    int i;
    printf("begin free mem\n");
    for (i = 0; i < SLOTS; i++) {
        free(ringFifo[i].buffer);
        ringFifo[i].size = 0;
    }
}

int ring_add(int count) {
    return (count + 1) == SLOTS ? 0 : count + 1;
}

int ring_get(struct ringbuf *getinfo) {
    if (slot > 0) {
        int pos = readPos;
        readPos = ring_add(readPos);
        slot--;
        getinfo->buffer = (ringFifo[pos].buffer);
        getinfo->frame_type = ringFifo[pos].frame_type;
        getinfo->size = ringFifo[pos].size;
        return ringFifo[pos].size;
    } else return 0;
}

void ring_put(unsigned char *buffer, int size, int encode_type) {

    if (slot < SLOTS) {
        memcpy(ringFifo[writePos].buffer, buffer, size);
        ringFifo[writePos].size = size;
        ringFifo[writePos].frame_type = encode_type;
        writePos = ring_add(writePos);
        slot++;
    }
}

/*
Put the H264 stream data into ringFifo[writePos].buffer so that the schedule_do
thread can take out the data from ringfifo[readPos].buffer and send it out.
In the same DESCRIBE step, SPS and PPS encoding will be sent to the client. 
*/
int put_h264_data_to_buffer(hal_vidstream *stream)
{
    int len = 0, off = 0, len2 = 2;
    unsigned char *pstr;
    int iframe = 0;
    for (int i = 0; i < stream->count; i++)
        len += stream->pack[i].length - stream->pack[i].offset;

    int testlen = 0;
    if (slot < SLOTS) {
        for (int i = 0; i < stream->count; i++) {
            memcpy(ringFifo[writePos].buffer + off,
                stream->pack[i].data + stream->pack[i].offset,
                stream->pack[i].length - stream->pack[i].offset);
            pstr = stream->pack[i].data + 
                stream->pack[i].offset; // Address of valid data
            off += stream->pack[i].length - 
                stream->pack[i].offset; // Next address of valid data

            if (pstr[4] == 0x67) {
                rtsp_update_sps(ringFifo[writePos].buffer + off, 9);
                iframe = 1;
            }
            else if (pstr[4] == 0x68)
                rtsp_update_pps(ringFifo[writePos].buffer + off, 4);
        }
        ringFifo[writePos].size = len;
        if (iframe)
            ringFifo[writePos].frame_type = FRAME_TYPE_I;
        else
            ringFifo[writePos].frame_type = FRAME_TYPE_P;
        writePos = ring_add(writePos);
        slot++;
    }

    return EXIT_SUCCESS;
}