#pragma once

#include "bitbuf.h"

extern uint32_t pos_sequence_number;
extern uint32_t pos_base_data_offset;
extern uint32_t pos_audio_media_decode_time;
extern uint32_t pos_video_media_decode_time;

struct SampleInfo {
    uint32_t duration;
    uint32_t size;
    uint32_t flags;
};

enum BufError
write_mdat(struct BitBuf *ptr,
    const char *data_vid, const uint32_t len_vid,
    const char *data_aud, const uint32_t len_aud);
enum BufError write_moof(
    struct BitBuf *ptr, const uint32_t sequence_number,
    const uint64_t base_data_offset, const uint64_t base_media_decode_time,
    const uint32_t default_sample_duration, const struct SampleInfo *samples_vid,
    const uint32_t samples_vid_len, const struct SampleInfo *samples_aud,
    const uint32_t samples_aud_len);
