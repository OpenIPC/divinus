#pragma once

#include "../common.h"

struct ringbuf {
    unsigned char *buffer;
    int frame_type;
    int size;
};

int ring_add(int count);
int ring_get(struct ringbuf *getinfo);
void ring_put(unsigned char *buffer, int size, int encode_type);
void ring_free();
void ring_malloc(int size);
void ring_reset();

int put_h264_data_to_buffer(hal_vidstream *stream);