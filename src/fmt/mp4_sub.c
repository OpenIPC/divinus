#include "mp4_sub.h"

uint32_t default_sample_size_sub = 40000;
uint32_t last_fragment_duration_sub, timescale_sub;

unsigned int aud_samplerate_sub = 0;
unsigned short aud_bitrate_sub = 0;
char aud_channels_sub = 0, aud_codec_sub = 0, vid_framerate_sub = 15;
short vid_width_sub = 640, vid_height_sub = 480;

char buf_pps_sub[128];
uint16_t buf_pps_len_sub = 0;
char buf_sps_sub[128];
uint16_t buf_sps_len_sub = 0;
char buf_vps_sub[128];
uint16_t buf_vps_len_sub = 0;
struct BitBuf buf_aud_sub;
struct BitBuf buf_header_sub;
struct BitBuf buf_mdat_sub;
struct BitBuf buf_moof_sub;

static enum BufError create_header_sub(char is_h265) {
    if (buf_header_sub.offset > 0)
        return BUF_OK;
    if (buf_sps_len_sub == 0)
        return BUF_OK;
    if (buf_pps_len_sub == 0)
        return BUF_OK;
    if (is_h265 && buf_vps_len_sub == 0)
        return BUF_OK;

    struct MoovInfo moov_info;
    memset(&moov_info, 0, sizeof(struct MoovInfo));
    moov_info.audio_codec = aud_codec_sub;
    moov_info.audio_bitrate = aud_bitrate_sub;
    moov_info.audio_channels = aud_channels_sub;
    moov_info.audio_samplerate = aud_samplerate_sub;
    moov_info.is_h265 = is_h265 & 1;
    moov_info.profile_idc = 100;
    moov_info.level_idc = 41;
    moov_info.width = vid_width_sub;
    moov_info.height = vid_height_sub;
    moov_info.horizontal_resolution = 0x00480000;
    moov_info.vertical_resolution = 0x00480000;
    moov_info.creation_time = 0;
    timescale_sub = default_sample_size_sub * vid_framerate_sub;
    moov_info.timescale = timescale_sub;
    moov_info.sps = buf_sps_sub;
    moov_info.sps_length = buf_sps_len_sub;
    moov_info.pps = buf_pps_sub;
    moov_info.pps_length = buf_pps_len_sub;
    moov_info.vps = buf_vps_sub;
    moov_info.vps_length = buf_vps_len_sub;

    buf_aud_sub.offset = 0;
    buf_header_sub.offset = 0;
    enum BufError err = write_header(&buf_header_sub, &moov_info);
    chk_err return BUF_OK;
}

void mp4_sub_set_config(short width, short height, char framerate, char acodec,
    unsigned short bitrate, char channels, unsigned int srate) {
    vid_width_sub = width;
    vid_height_sub = height;
    vid_framerate_sub = framerate;
    aud_codec_sub = acodec;
    aud_bitrate_sub = bitrate;
    aud_channels_sub = channels;
    aud_samplerate_sub = srate;
}

void mp4_sub_set_sps(const char *nal_data, const uint32_t nal_len, char is_h265) {
    memcpy(buf_sps_sub, nal_data, MIN(nal_len, sizeof(buf_sps_sub)));
    buf_sps_len_sub = nal_len;
    create_header_sub(is_h265);
}

void mp4_sub_set_pps(const char *nal_data, const uint32_t nal_len, char is_h265) {
    memcpy(buf_pps_sub, nal_data, MIN(nal_len, sizeof(buf_pps_sub)));
    buf_pps_len_sub = nal_len;
    create_header_sub(is_h265);
}

void mp4_sub_set_vps(const char *nal_data, const uint32_t nal_len) {
    memcpy(buf_vps_sub, nal_data, MIN(nal_len, sizeof(buf_vps_sub)));
    buf_vps_len_sub = nal_len;
    create_header_sub(1);
}

enum BufError mp4_sub_set_slice(const char *nal_data, const uint32_t nal_len,
    char is_iframe) {
    uint64_t aud_ticks = 0;
    enum BufError err;

    struct SampleInfo samples_info[2];
    memset(samples_info, 0, sizeof(samples_info));
    samples_info[0].size = nal_len + 4;
    samples_info[0].duration = default_sample_size_sub;
    samples_info[0].flags = is_iframe ? 0 : 65536;
    samples_info[1].size = buf_aud_sub.offset;
    if (aud_bitrate_sub > 0 && buf_aud_sub.offset > 0) {
        aud_ticks = ((uint64_t)(buf_aud_sub.offset << 3) * timescale_sub) /
            (aud_bitrate_sub * 1000);
        samples_info[1].duration = (uint32_t)aud_ticks;
        last_fragment_duration_sub = MAX(samples_info[1].duration, samples_info[0].duration);
    } else {
        samples_info[1].duration = 0;
        last_fragment_duration_sub = samples_info[0].duration;
    }

    buf_moof_sub.offset = 0;
    err = write_moof(
        &buf_moof_sub, 0, 0, 0, default_sample_size_sub, samples_info,
        1, samples_info + 1, 1);
    chk_err;

    buf_mdat_sub.offset = 0;
    err = write_mdat(&buf_mdat_sub, nal_data, nal_len,
        buf_aud_sub.buf, buf_aud_sub.offset);
    chk_err;

    buf_aud_sub.offset = 0;

    return BUF_OK;
}

enum BufError mp4_sub_ingest_audio(const char *data, const uint32_t len) {
    enum BufError err;
    err = put(&buf_aud_sub, data, len);
    chk_err;

    return BUF_OK;
}

enum BufError mp4_sub_set_state(struct Mp4SubState *state) {
    enum BufError err;
    if (pos_sequence_number > 0)
        err = put_u32_be_to_offset(
            &buf_moof_sub, pos_sequence_number, state->sequence_number);
    chk_err if (pos_base_data_offset > 0) err = put_u64_be_to_offset(
        &buf_moof_sub, pos_base_data_offset, state->base_data_offset);
    chk_err if (pos_audio_media_decode_time > 0) err = put_u64_be_to_offset(
        &buf_moof_sub, pos_audio_media_decode_time,
        state->base_media_decode_time);
    chk_err if (pos_video_media_decode_time > 0) err = put_u64_be_to_offset(
        &buf_moof_sub, pos_video_media_decode_time,
        state->base_media_decode_time);
    chk_err state->sequence_number++;
    state->base_data_offset += buf_moof_sub.offset + buf_mdat_sub.offset;
    state->base_media_decode_time += last_fragment_duration_sub;
    return BUF_OK;
}

enum BufError mp4_sub_get_header(struct BitBuf *ptr) {
    ptr->buf = buf_header_sub.buf;
    ptr->size = buf_header_sub.size;
    ptr->offset = buf_header_sub.offset;
    return BUF_OK;
}

enum BufError mp4_sub_get_mdat(struct BitBuf *ptr) {
    ptr->buf = buf_mdat_sub.buf;
    ptr->size = buf_mdat_sub.size;
    ptr->offset = buf_mdat_sub.offset;
    return BUF_OK;
}

enum BufError mp4_sub_get_moof(struct BitBuf *ptr) {
    ptr->buf = buf_moof_sub.buf;
    ptr->size = buf_moof_sub.size;
    ptr->offset = buf_moof_sub.offset;
    return BUF_OK;
}
