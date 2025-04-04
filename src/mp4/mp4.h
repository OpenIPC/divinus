#pragma once

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "bitbuf.h"
#include "moof.h"
#include "moov.h"
#include "nal.h"

#ifndef MIN
#define MIN(a, b) (((a) < (b)) ? (a) : (b))
#endif

extern uint32_t default_sample_size;

struct Mp4State {
    bool header_sent;

    uint32_t sequence_number;
    uint64_t base_data_offset;
    uint64_t base_media_decode_time;
    uint32_t default_sample_duration;

    uint32_t nals_count;
};

void mp4_set_config(short width, short height, char framerate, char acodec,
    unsigned short bitrate, char channels, unsigned int srate);

void mp4_set_sps(const char *nal_data, const uint32_t nal_len, char is_h265);
void mp4_set_pps(const char *nal_data, const uint32_t nal_len, char is_h265);
void mp4_set_vps(const char *nal_data, const uint32_t nal_len);
enum BufError mp4_set_slice(const char *nal_data, const uint32_t nal_len,
    char is_iframe);
enum BufError mp4_ingest_audio(const char *data, const uint32_t len);

enum BufError mp4_set_state(struct Mp4State *state);

enum BufError mp4_get_header(struct BitBuf *ptr);
enum BufError mp4_get_moof(struct BitBuf *ptr);
enum BufError mp4_get_mdat(struct BitBuf *ptr);