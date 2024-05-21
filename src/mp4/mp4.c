#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "mp4.h"

uint32_t default_sample_size = 40000;

enum BufError create_header(char isH265);

short vid_width = 1920, vid_height = 1080;
char vid_framerate = 30;

char buf_pps[128];
uint16_t buf_pps_len = 0;
char buf_sps[128];
uint16_t buf_sps_len = 0;
char buf_vps[128];
uint16_t buf_vps_len = 0;
struct BitBuf buf_header;
struct BitBuf buf_mdat;
struct BitBuf buf_moof;

void set_mp4_config(short width, short height, char framerate)
{
    vid_width = width;
    vid_height = height;
    vid_framerate = framerate;
}

enum BufError create_header(char isH265) {
    if (buf_header.offset > 0)
        return BUF_OK;
    if (buf_sps_len == 0)
        return BUF_OK;
    if (buf_pps_len == 0)
        return BUF_OK;
    if (isH265 && buf_vps_len == 0)
        return BUF_OK;

    struct MoovInfo moov_info;
    memset(&moov_info, 0, sizeof(struct MoovInfo));
    moov_info.isH265 = isH265 & 1;
    moov_info.profile_idc = 100;
    moov_info.level_idc = 41;
    moov_info.width = vid_width;
    moov_info.height = vid_height;
    moov_info.horizontal_resolution = 0x00480000; // 72 dpi
    moov_info.vertical_resolution = 0x00480000;   // 72 dpi
    moov_info.creation_time = 0;
    moov_info.timescale =
        default_sample_size * vid_framerate;
    moov_info.sps = buf_sps;
    moov_info.sps_length = buf_sps_len;
    moov_info.pps = buf_pps;
    moov_info.pps_length = buf_pps_len;
    moov_info.vps = buf_vps;
    moov_info.vps_length = buf_vps_len;

    buf_header.offset = 0;
    enum BufError err = write_header(&buf_header, &moov_info);
    chk_err return BUF_OK;
}

void set_sps(const char *nal_data, const uint32_t nal_len, char isH265) {
    memcpy(buf_sps, nal_data, MIN(nal_len, sizeof(buf_sps)));
    buf_sps_len = nal_len;
    create_header(isH265);
}

void set_pps(const char *nal_data, const uint32_t nal_len, char isH265) {
    memcpy(buf_pps, nal_data, MIN(nal_len, sizeof(buf_pps)));
    buf_pps_len = nal_len;
    create_header(isH265);
}

void set_vps(const char *nal_data, const uint32_t nal_len) {
    memcpy(buf_vps, nal_data, MIN(nal_len, sizeof(buf_vps)));
    buf_vps_len = nal_len;
    create_header(1);
}

enum BufError get_header(struct BitBuf *ptr) {
    ptr->buf = buf_header.buf;
    ptr->size = buf_header.size;
    ptr->offset = buf_header.offset;
    return BUF_OK;
}

enum BufError set_slice(const char *nal_data, const uint32_t nal_len,
    char isIframe) {
    enum BufError err;

    const uint32_t samples_info_len = 1;
    struct SampleInfo samples_info[1];
    memset(&samples_info[0], 0, sizeof(struct SampleInfo));
    samples_info[0].size = nal_len + 4; // add size of sample
    samples_info[0].composition_offset = default_sample_size;
    samples_info[0].decode_time = default_sample_size;
    samples_info[0].duration = default_sample_size;
    samples_info[0].flags = isIframe ? 0 : 65536;

    buf_moof.offset = 0;
    err = write_moof(
        &buf_moof, 0, 0, 0, default_sample_size, samples_info,
        samples_info_len);
    chk_err

    buf_mdat.offset = 0;
    err = write_mdat(&buf_mdat, nal_data, nal_len);
    chk_err

    return BUF_OK;
}

enum BufError set_mp4_state(struct Mp4State *state) {
    enum BufError err;
    if (pos_sequence_number > 0)
        err = put_u32_be_to_offset(
            &buf_moof, pos_sequence_number, state->sequence_number);
    chk_err if (pos_base_data_offset > 0) err = put_u64_be_to_offset(
        &buf_moof, pos_base_data_offset, state->base_data_offset);
    chk_err if (pos_base_media_decode_time > 0) err = put_u64_be_to_offset(
        &buf_moof, pos_base_media_decode_time,
        state->base_media_decode_time);
    chk_err state->sequence_number++;
    state->base_data_offset += buf_moof.offset + buf_mdat.offset;
    state->base_media_decode_time += state->default_sample_duration;
    return BUF_OK;
}
enum BufError get_moof(struct BitBuf *ptr) {
    ptr->buf = buf_moof.buf;
    ptr->size = buf_moof.size;
    ptr->offset = buf_moof.offset;
    return BUF_OK;
}
enum BufError get_mdat(struct BitBuf *ptr) {
    ptr->buf = buf_mdat.buf;
    ptr->size = buf_mdat.size;
    ptr->offset = buf_mdat.offset;
    return BUF_OK;
}