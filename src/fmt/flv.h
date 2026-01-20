#pragma once

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "bitbuf.h"
#include "amf.h"
#include "../hal/macros.h"

struct FlvState {
    bool header_sent;
    uint32_t timestamp_ms;
    uint32_t audio_timestamp_ms;
    uint32_t frame_duration_ms;
};

void flv_set_config(short width, short height, char framerate, char acodec,
    unsigned short bitrate, char channels, unsigned int srate);

void flv_set_sps(const char *nal_data, const uint32_t nal_len);
void flv_set_pps(const char *nal_data, const uint32_t nal_len);
void flv_set_vps(const char *nal_data, const uint32_t nal_len);

enum BufError flv_set_slice(const char *nal_data, const uint32_t nal_len,
    char is_iframe);
enum BufError flv_ingest_audio(const char *data, const uint32_t len);

enum BufError flv_set_state(struct FlvState *state);
enum BufError flv_inc_timestamp(struct FlvState *state);
enum BufError flv_inc_audio_timestamp(struct FlvState *state, uint32_t data_len);

enum BufError flv_get_header(struct BitBuf *ptr);
enum BufError flv_get_tags(struct BitBuf *ptr);
enum BufError flv_get_metadata(struct BitBuf *ptr);
enum BufError flv_get_audio_tags(struct BitBuf *ptr);