#pragma once

#include <stdbool.h>
#include <stdint.h>

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

void set_mp4_config(short width, short height, char framerate, char acodec, int srate);

enum BufError ingest_mp4_audio(const char *data, const uint32_t len);
enum BufError set_slice(const char *nal_data, const uint32_t nal_len,
    char is_iframe);
void set_sps(const char *nal_data, const uint32_t nal_len, char is_h265);
void set_pps(const char *nal_data, const uint32_t nal_len, char is_h265);
void set_vps(const char *nal_data, const uint32_t nal_len);

enum BufError get_header(struct BitBuf *ptr);

enum BufError set_mp4_state(struct Mp4State *state);
enum BufError get_moof(struct BitBuf *ptr);
enum BufError get_mdat(struct BitBuf *ptr);