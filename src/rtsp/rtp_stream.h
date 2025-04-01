/*
 * Copyright (c) 2025 Kevin Le Bihan
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

#ifndef RTP_STREAM_H
#define RTP_STREAM_H

#if defined (__cplusplus)
extern "C" {
#endif
#include <unistd.h>
#include <time.h>
#include <stdbool.h>

/******************************************************************************
 *              DEFINITIONS
 ******************************************************************************/


void rtp_init(const char *rtp_ip, unsigned int rtp_port);
void rtp_deinit(void);

// Structure pour représenter une unité NAL
typedef struct {
    unsigned char *data;
    unsigned int size;
} NALUnit_t;

void rtp_send_frame_h26x(unsigned long nbNal, NALUnit_t* nals, bool isH265);


#if defined (__cplusplus)
}
#endif


#endif // RTP_STREAM_H