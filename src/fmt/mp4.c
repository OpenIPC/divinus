#include "mp4.h"

pthread_mutex_t mp4_mtx = PTHREAD_MUTEX_INITIALIZER;

uint32_t default_sample_size = 40000;
uint32_t last_video_duration, last_audio_duration, timescale;
uint64_t last_vid_tstamp;
uint32_t vid_duration;

unsigned int aud_samplerate = 0;
unsigned short aud_bitrate = 0;
char aud_channels = 0, aud_codec = 0, vid_framerate = 30;
short vid_width = 1920, vid_height = 1080;

char buf_pps[128];
uint16_t buf_pps_len = 0;
char buf_sps[128];
uint16_t buf_sps_len = 0;
char buf_vps[128];
uint16_t buf_vps_len = 0;
struct BitBuf buf_aud;
#define MP4_AUD_MAX 16
static uint32_t aud_frm_size[MP4_AUD_MAX], aud_frm_samp[MP4_AUD_MAX], aud_frm_cnt;
struct BitBuf buf_header;
struct BitBuf buf_mdat;
struct BitBuf buf_moof;

enum BufError create_header(char is_h265) {
    if (buf_header.offset > 0)
        return BUF_OK;
    if (buf_sps_len == 0)
        return BUF_OK;
    if (buf_pps_len == 0)
        return BUF_OK;
    if (is_h265 && buf_vps_len == 0)
        return BUF_OK;

    struct MoovInfo moov_info;
    memset(&moov_info, 0, sizeof(struct MoovInfo));
    moov_info.audio_codec = aud_codec;
    moov_info.audio_bitrate = aud_bitrate;
    moov_info.audio_channels = aud_channels;
    moov_info.audio_samplerate = aud_samplerate;
    moov_info.is_h265 = is_h265 & 1;
    moov_info.profile_idc = 100;
    moov_info.level_idc = 41;
    moov_info.width = vid_width;
    moov_info.height = vid_height;
    moov_info.horizontal_resolution = 0x00480000; // 72 dpi
    moov_info.vertical_resolution = 0x00480000;   // 72 dpi
    moov_info.creation_time = 0;
    timescale = default_sample_size * vid_framerate;
    moov_info.timescale = timescale;
    moov_info.sps = buf_sps;
    moov_info.sps_length = buf_sps_len;
    moov_info.pps = buf_pps;
    moov_info.pps_length = buf_pps_len;
    moov_info.vps = buf_vps;
    moov_info.vps_length = buf_vps_len;

    buf_aud.offset = 0;
    aud_frm_cnt = 0;
    buf_header.offset = 0;
    enum BufError err = write_header(&buf_header, &moov_info);
    chk_err return BUF_OK;
}

void mp4_set_config(short width, short height, char framerate, char acodec,
    unsigned short bitrate, char channels, unsigned int srate) {
    /* A new stream configuration (codec, size) invalidates the cached header
     * and parameter sets; without this a codec switch keeps sending the old one */
    buf_header.offset = 0;
    buf_sps_len = buf_pps_len = buf_vps_len = 0;
    last_vid_tstamp = 0;
    vid_duration = default_sample_size;
    vid_width = width;
    vid_height = height;
    vid_framerate = framerate;
    aud_codec = acodec;
    aud_bitrate = bitrate;
    aud_channels = channels;
    aud_samplerate = srate;
}

void mp4_set_sps(const char *nal_data, const uint32_t nal_len, char is_h265) {
    buf_sps_len = MIN(nal_len, (uint32_t)sizeof(buf_sps));
    memcpy(buf_sps, nal_data, buf_sps_len);
    create_header(is_h265);
}

void mp4_set_pps(const char *nal_data, const uint32_t nal_len, char is_h265) {
    buf_pps_len = MIN(nal_len, (uint32_t)sizeof(buf_pps));
    memcpy(buf_pps, nal_data, buf_pps_len);
    create_header(is_h265);
}

void mp4_set_vps(const char *nal_data, const uint32_t nal_len) {
    buf_vps_len = MIN(nal_len, (uint32_t)sizeof(buf_vps));
    memcpy(buf_vps, nal_data, buf_vps_len);
    create_header(1);
}

void mp4_set_timestamp(uint64_t tstamp) {
    if (!tstamp) return;

    if (last_vid_tstamp && tstamp > last_vid_tstamp && timescale) {
        uint32_t duration = ((tstamp - last_vid_tstamp) * timescale) / 1000000;
        if (duration) vid_duration = duration;
    }
    last_vid_tstamp = tstamp;
}

enum BufError mp4_set_slice(const char *nal_data, const uint32_t nal_len,
    char is_iframe) {
    enum BufError err;

    /* One MP4 sample per MPEG audio frame: decoders that validate packet
     * lengths reject several frames glued into a single sample. */
    struct SampleInfo samples_info[1 + MP4_AUD_MAX];
    uint32_t aud_cnt = 0, aud_len = 0, aud_dur = 0;
    memset(samples_info, 0, sizeof(samples_info));
    samples_info[0].size = nal_len + 4; // add size of sample
    samples_info[0].duration = vid_duration;
    samples_info[0].flags = is_iframe ? 0 : 65536;
    if (aud_frm_cnt > 0 && aud_samplerate > 0) {
        /* Frames handed over one by one with their length (Opus) */
        for (; aud_cnt < aud_frm_cnt; aud_cnt++) {
            samples_info[1 + aud_cnt].size = aud_frm_size[aud_cnt];
            samples_info[1 + aud_cnt].duration =
                (uint32_t)((uint64_t)aud_frm_samp[aud_cnt] * timescale / aud_samplerate);
            aud_dur += samples_info[1 + aud_cnt].duration;
            aud_len += aud_frm_size[aud_cnt];
        }
    } else if (aud_codec != 0x69 /* MP3 */ && aud_bitrate > 0 && buf_aud.offset > 0) {
        samples_info[1].size = aud_len = buf_aud.offset;
        samples_info[1].duration = aud_dur = (uint32_t)(((uint64_t)(buf_aud.offset << 3) *
            timescale) / (aud_bitrate * 1000));
        aud_cnt = 1;
    } else if (aud_bitrate > 0 && aud_samplerate > 0 && buf_aud.offset > 0) {
        static const uint16_t kbps[2][15] = {
            {0, 32, 40, 48, 56, 64, 80, 96, 112, 128, 160, 192, 224, 256, 320},
            {0, 8, 16, 24, 32, 40, 48, 56, 64, 80, 96, 112, 128, 144, 160}};
        static const uint16_t rates[3] = {44100, 48000, 32000};
        const unsigned char *a = (const unsigned char*)buf_aud.buf;
        while (aud_cnt < MP4_AUD_MAX && aud_len + 4 <= buf_aud.offset) {
            const unsigned char *h = a + aud_len;
            if (h[0] != 0xFF || (h[1] & 0xE6) != 0xE2) break; // Layer III sync
            unsigned ver = (h[1] >> 3) & 3, bi = h[2] >> 4, ri = (h[2] >> 2) & 3;
            if (ver == 1 || bi == 0 || bi == 15 || ri == 3) break;
            unsigned lsf = ver != 3, rate = rates[ri] >> (ver == 3 ? 0 : ver == 2 ? 1 : 2);
            unsigned spf = lsf ? 576 : 1152;
            unsigned len = spf / 8 * kbps[lsf][bi] * 1000 / rate + ((h[2] >> 1) & 1);
            if (aud_len + len > buf_aud.offset) break;
            samples_info[1 + aud_cnt].size = len;
            samples_info[1 + aud_cnt].duration = (uint32_t)((uint64_t)spf * timescale / rate);
            aud_dur += samples_info[1 + aud_cnt].duration;
            aud_len += len;
            aud_cnt++;
        }
    }
    /* Each track keeps its own clock; sharing one made every video frame
     * inherit the length of the audio chunk muxed with it. */
    last_video_duration = samples_info[0].duration;
    last_audio_duration = aud_dur;

    buf_moof.offset = 0;
    err = write_moof(
        &buf_moof, 0, 0, 0, default_sample_size, samples_info,
        1, samples_info + 1, aud_cnt ? aud_cnt : 1);
    chk_err;

    buf_mdat.offset = 0;
    err = write_mdat(&buf_mdat, nal_data, nal_len,
        buf_aud.buf, aud_len);
    chk_err;

    /* Anything that is not a whole frame is unusable: drop it and resync. */
    buf_aud.offset = 0;
    aud_frm_cnt = 0;

    return BUF_OK;
}

enum BufError mp4_ingest_audio_frame(const char *data, const uint32_t len,
    const uint32_t samples) {
    if (aud_bitrate == 0) return BUF_OK;
    /* Nobody draining (no client): keep only the newest frames */
    if (aud_frm_cnt >= MP4_AUD_MAX) { aud_frm_cnt = 0; buf_aud.offset = 0; }
    enum BufError err = put(&buf_aud, data, len);
    chk_err;
    aud_frm_size[aud_frm_cnt] = len;
    aud_frm_samp[aud_frm_cnt++] = samples;
    return BUF_OK;
}

enum BufError mp4_ingest_audio(const char *data, const uint32_t len) {
    enum BufError err;
    /* Audio is only drained when a slice is muxed; with no client attached it
     * would pile up and the next fragment would jump the timeline by minutes. */
    if (aud_bitrate == 0) return BUF_OK;
    if (buf_aud.offset + len > (uint32_t)aud_bitrate * 1000 / 8 / 4)
        buf_aud.offset = 0;
    err = put(&buf_aud, data, len);
    chk_err;

    return BUF_OK;
}

enum BufError mp4_set_state(struct Mp4State *state) {
    enum BufError err;
    if (pos_sequence_number > 0)
        err = put_u32_be_to_offset(
            &buf_moof, pos_sequence_number, state->sequence_number);
    chk_err if (pos_base_data_offset > 0) err = put_u64_be_to_offset(
        &buf_moof, pos_base_data_offset, state->base_data_offset);
    chk_err if (pos_audio_media_decode_time > 0) err = put_u64_be_to_offset(
        &buf_moof, pos_audio_media_decode_time,
        state->audio_media_decode_time);
    chk_err if (pos_video_media_decode_time > 0) err = put_u64_be_to_offset(
        &buf_moof, pos_video_media_decode_time,
        state->base_media_decode_time);
    chk_err state->sequence_number++;
    state->base_data_offset += buf_moof.offset + buf_mdat.offset;
    state->base_media_decode_time += last_video_duration;
    state->audio_media_decode_time += last_audio_duration;
    return BUF_OK;
}

enum BufError mp4_get_header(struct BitBuf *ptr) {
    ptr->buf = buf_header.buf;
    ptr->size = buf_header.size;
    ptr->offset = buf_header.offset;
    return BUF_OK;
}

enum BufError mp4_get_mdat(struct BitBuf *ptr) {
    ptr->buf = buf_mdat.buf;
    ptr->size = buf_mdat.size;
    ptr->offset = buf_mdat.offset;
    return BUF_OK;
}

enum BufError mp4_get_moof(struct BitBuf *ptr) {
    ptr->buf = buf_moof.buf;
    ptr->size = buf_moof.size;
    ptr->offset = buf_moof.offset;
    return BUF_OK;
}
