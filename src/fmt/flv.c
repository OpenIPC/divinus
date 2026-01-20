#include "flv.h"

static unsigned int aud_samplerate = 0, aud_framesize = 0;
static unsigned short aud_bitrate = 0;
static char aud_channels = 0, aud_codec = 0, vid_framerate = 30;
static short vid_width = 1920, vid_height = 1080;

static char buf_pps[128];
static uint16_t buf_pps_len = 0;
static char buf_sps[128];
static uint16_t buf_sps_len = 0;
static char buf_vps[128];
static uint16_t buf_vps_len = 0;

static struct BitBuf buf_aud;
static struct BitBuf buf_header;
static struct BitBuf buf_tags;

#define BUFCAP (256 * 1024)

static void trim_buffer(struct BitBuf *buf) {
    if (!buf || !buf->buf) return;
    
    // If buffer is much larger than what we're using, shrink it
    if (buf->size > BUFCAP && buf->offset < BUFCAP / 2) {
        char *new_buf = realloc(buf->buf, BUFCAP);
        if (new_buf) {
            buf->buf = new_buf;
            buf->size = BUFCAP;
        }
    }
}

static void strip_annexb(const char **data, uint32_t *len) {
    if (!data || !*data || !len || *len < 4) return;

    const uint8_t *p = (const uint8_t *)(*data);
    uint32_t l = *len;
    if (l >= 4 && p[0] == 0 && p[1] == 0) {
        if (p[2] == 1) {
            p += 3; l -= 3;
        } else if (p[2] == 0 && p[3] == 1) {
            p += 4; l -= 4;
        }
    }
    *data = (const char *)p; *len = l;
}

// Offsets for patching timestamps in the last built tags
static uint32_t pos_video_timestamp = 0;     // points to 3-byte TS in video tag header
static uint32_t pos_video_timestamp_ext = 0; // points to 1-byte extended TS
static uint32_t pos_audio_timestamp = 0;
static uint32_t pos_audio_timestamp_ext = 0;

static inline uint8_t flv_audio_format_from_codec(char codec) {
    // Map internal audio codec IDs to FLV SoundFormat
    // AAC -> 10, MP3 -> 2, otherwise 0 (Linear PCM)
    switch ((unsigned char)codec) {
        case 0x40: // ACODEC_ID_AAC
        case 0x67: // ACODEC_ID_AAC_LC
            return 10;
        case 0x69: // ACODEC_ID_MP3
            return 2;
        default:
            return 0; // PCM, as fallback
    }
}

static inline uint8_t flv_video_codec_id(void) {
    // Unofficial HEVC-in-FLV uses CodecID 12; AVC uses 7
    return (buf_vps_len > 0) ? 12 : 7;
}

static inline uint8_t flv_audio_rate_code(unsigned int srate) {
    // 0=5.5k,1=11k,2=22k,3=44k; for AAC, value is always 3 per spec
    if (srate >= 44100) return 3;
    if (srate >= 22050) return 2;
    if (srate >= 11025) return 1;
    return 0;
}

static enum BufError flv_write_header() {
    enum BufError err;
    // Signature 'FLV'
    err = put(&buf_header, "FLV", 3);
    chk_err;
    // Version
    err = put_u8(&buf_header, 1);
    chk_err;
    // TypeFlags: Audio(0x04) + Video(0x01)
    uint8_t flags = 0x01; // video present
    if (aud_samplerate) flags |= 0x04; // audio present
    err = put_u8(&buf_header, flags);
    chk_err;
    // DataOffset = 9
    err = put_u32_be(&buf_header, 9);
    chk_err;
    // PreviousTagSize0
    err = put_u32_be(&buf_header, 0);
    chk_err;
    return BUF_OK;
}

static enum BufError flv_write_script_metadata() {
    enum BufError err;
    // Tag header
    uint32_t tag_start = buf_header.offset;
    err = put_u8(&buf_header, 18); // Script data
    chk_err;
    uint32_t sz_off = buf_header.offset; // 3 bytes size placeholder
    err = put_u24_be(&buf_header, 0);
    chk_err;
    // Timestamp
    err = put_u24_be(&buf_header, 0);
    chk_err;
    err = put_u8(&buf_header, 0); // TimestampExtended
    chk_err;
    err = put_u24_be(&buf_header, 0); // StreamID
    chk_err;

    // Data: AMF 'onMetaData' + ECMA array with properties
    err = AMFWriteString(&buf_header, "onMetaData", strlen("onMetaData"));
    chk_err;
    err = AMFWriteECMAArray(&buf_header);
    chk_err;
    // Width/Height/Framerate
    err = AMFWriteNamedDouble(&buf_header, "width", strlen("width"), vid_width);
    chk_err;
    err = AMFWriteNamedDouble(&buf_header, "height", strlen("height"), vid_height);
    chk_err;
    err = AMFWriteNamedDouble(&buf_header, "framerate", strlen("framerate"), vid_framerate);
    chk_err;
    // Codecs
    err = AMFWriteNamedDouble(&buf_header, "videocodecid", strlen("videocodecid"), flv_video_codec_id());
    chk_err;
    err = AMFWriteNamedDouble(&buf_header, "videodatarate", strlen("videodatarate"), 4000); // kbps
    chk_err;
    if (aud_samplerate) {
        err = AMFWriteNamedDouble(&buf_header, "audiocodecid", strlen("audiocodecid"),
                                  flv_audio_format_from_codec(aud_codec));
        chk_err;
        err = AMFWriteNamedDouble(&buf_header, "audiosamplerate", strlen("audiosamplerate"), aud_samplerate);
        chk_err;
        err = AMFWriteNamedDouble(&buf_header, "audiosamplesize", strlen("audiosamplesize"), 16);
        chk_err;
        err = AMFWriteNamedBoolean(&buf_header, "stereo", strlen("stereo"), aud_channels > 1 ? 1 : 0);
        chk_err;
        err = AMFWriteNamedDouble(&buf_header, "audiodatarate", strlen("audiodatarate"), aud_bitrate);
        chk_err;
    }
    // Remove duration/filesize for live streams
    err = AMFWriteNamedString(&buf_header, "encoder", strlen("encoder"), " Divinus", strlen("Divinus"));
    chk_err;
    err = AMFWriteObjectEnd(&buf_header);
    chk_err;

    // Patch tag size and write PreviousTagSize
    uint32_t data_size = buf_header.offset - (sz_off + 10); // exclude header fields (11 bytes total), sz_off points after TagType
    // But sz_off is after TagType (1), so header size fields till StreamID are 10 bytes.
    err = put_u24_be_to_offset(&buf_header, sz_off, data_size);
    chk_err;
    uint32_t prev_size = (buf_header.offset - tag_start);
    err = put_u32_be(&buf_header, prev_size);
    chk_err;
    return BUF_OK;
}

static enum BufError flv_write_avc_sequence_header() {
    if (buf_sps_len == 0 || buf_pps_len == 0)
        return BUF_OK; // nothing to write yet
    enum BufError err;
    uint32_t tag_start = buf_header.offset;
    err = put_u8(&buf_header, 9); // Video tag
    chk_err;
    uint32_t sz_off = buf_header.offset;
    err = put_u24_be(&buf_header, 0);
    chk_err;
    // Timestamp = 0
    err = put_u24_be(&buf_header, 0);
    chk_err;
    err = put_u8(&buf_header, 0);
    chk_err;
    err = put_u24_be(&buf_header, 0);
    chk_err;

    // Video payload: FrameType=1 (keyframe), CodecID=7 (AVC)
    err = put_u8(&buf_header, (1 << 4) | 7);
    chk_err;
    // AVCPacketType (0 = sequence header)
    err = put_u8(&buf_header, 0);
    chk_err;
    // CompositionTime
    err = put_u24_be(&buf_header, 0);
    chk_err;

    // AVCDecoderConfigurationRecord (avcC)
    err = put_u8(&buf_header, 1); // configurationVersion
    chk_err;
    err = put_u8(&buf_header, 100); // profile_idc (High)
    chk_err;
    err = put_u8(&buf_header, 0x00); // profile_compat
    chk_err;
    err = put_u8(&buf_header, 40); // level_idc (4.0)
    chk_err;
    err = put_u8(&buf_header, 0xFF); // lengthSizeMinusOne = 3 (4-byte NALU length)
    chk_err;
    // SPS
    err = put_u8(&buf_header, 1); // numOfSequenceParameterSets
    chk_err;
    err = put_u16_be(&buf_header, buf_sps_len);
    chk_err;
    err = put(&buf_header, buf_sps, buf_sps_len);
    chk_err;
    // PPS
    err = put_u8(&buf_header, 1); // numOfPictureParameterSets
    chk_err;
    err = put_u16_be(&buf_header, buf_pps_len);
    chk_err;
    err = put(&buf_header, buf_pps, buf_pps_len);
    chk_err;

    uint32_t data_size = buf_header.offset - (sz_off + 10);
    err = put_u24_be_to_offset(&buf_header, sz_off, data_size);
    chk_err;
    uint32_t prev_size = (buf_header.offset - tag_start);
    err = put_u32_be(&buf_header, prev_size);
    chk_err;
    return BUF_OK;
}

static enum BufError flv_write_hevc_sequence_header() {
    // Unofficial HEVC-in-FLV (CodecID=12) using HEVCDecoderConfigurationRecord (hvcC)
    if (buf_vps_len == 0 || buf_sps_len == 0 || buf_pps_len == 0)
        return BUF_OK;

    enum BufError err;
    uint32_t tag_start = buf_header.offset;
    err = put_u8(&buf_header, 9); // Video tag
    chk_err;
    uint32_t sz_off = buf_header.offset;
    err = put_u24_be(&buf_header, 0);
    chk_err;
    err = put_u24_be(&buf_header, 0); // Timestamp
    chk_err;
    err = put_u8(&buf_header, 0);
    chk_err;
    err = put_u24_be(&buf_header, 0); // StreamID
    chk_err;

    // FrameType=1 (keyframe), CodecID=12 (HEVC)
    err = put_u8(&buf_header, (1 << 4) | flv_video_codec_id());
    chk_err;
    // HEVCPacketType: 0 = sequence header
    err = put_u8(&buf_header, 0);
    chk_err;
    // CompositionTime
    err = put_u24_be(&buf_header, 0);
    chk_err;

    // Minimal hvcC (HEVCDecoderConfigurationRecord)
    err = put_u8(&buf_header, 1); // configurationVersion
    chk_err;
    err = put_u8(&buf_header, 0x01); // general_profile_space/tier/profile_idc (Main)
    chk_err;
    err = put_u32_be(&buf_header, 0); // general_profile_compatibility_flags
    chk_err;
    // general_constraint_indicator_flags (6 bytes)
    err = put_u24_be(&buf_header, 0);
    chk_err;
    err = put_u24_be(&buf_header, 0);
    chk_err;
    err = put_u8(&buf_header, 0x78); // general_level_idc (Level 4.0)
    chk_err;
    err = put_u16_be(&buf_header, 0xF000); // reserved(12 bits '1111') + min_spatial_segmentation_idc=0
    chk_err;
    err = put_u8(&buf_header, 0xFC); // reserved(6 bits '111111') + parallelismType=0
    chk_err;
    err = put_u8(&buf_header, 0xFD); // reserved + chromaFormat=1 (4:2:0)
    chk_err;
    err = put_u8(&buf_header, 0xF8); // reserved + bitDepthLumaMinus8=0 (8-bit)
    chk_err;
    err = put_u8(&buf_header, 0xF8); // reserved + bitDepthChromaMinus8=0 (8-bit)
    chk_err;
    err = put_u16_be(&buf_header, 0); // avgFrameRate unknown
    chk_err;
    err = put_u8(&buf_header, 0x07); // constantFrameRate=0, numTemporalLayers=0, temporalIdNested=1, lengthSizeMinusOne=3
    chk_err;

    // numOfArrays = 3 (VPS/SPS/PPS)
    err = put_u8(&buf_header, 3);
    chk_err;
    // VPS (NAL type 32)
    err = put_u8(&buf_header, 0x80 | 32);
    chk_err;
    err = put_u16_be(&buf_header, 1);
    chk_err;
    err = put_u16_be(&buf_header, buf_vps_len);
    chk_err;
    err = put(&buf_header, buf_vps, buf_vps_len);
    chk_err;
    // SPS (NAL type 33)
    err = put_u8(&buf_header, 0x80 | 33);
    chk_err;
    err = put_u16_be(&buf_header, 1);
    chk_err;
    err = put_u16_be(&buf_header, buf_sps_len);
    chk_err;
    err = put(&buf_header, buf_sps, buf_sps_len);
    chk_err;
    // PPS (NAL type 34)
    err = put_u8(&buf_header, 0x80 | 34);
    chk_err;
    err = put_u16_be(&buf_header, 1);
    chk_err;
    err = put_u16_be(&buf_header, buf_pps_len);
    chk_err;
    err = put(&buf_header, buf_pps, buf_pps_len);
    chk_err;

    uint32_t data_size = buf_header.offset - (sz_off + 10);
    err = put_u24_be_to_offset(&buf_header, sz_off, data_size);
    chk_err;
    uint32_t prev_size = (buf_header.offset - tag_start);
    err = put_u32_be(&buf_header, prev_size);
    chk_err;
    return BUF_OK;
}

static enum BufError flv_write_video_nalu(char is_iframe, const char *nal_data, const uint32_t nal_len) {
    enum BufError err;
    uint8_t codec_id = flv_video_codec_id();
    uint32_t tag_start = buf_tags.offset;
    err = put_u8(&buf_tags, 9); // Video
    chk_err;
    uint32_t sz_off = buf_tags.offset;
    err = put_u24_be(&buf_tags, 0);
    chk_err;
    // Timestamp (patched later)
    pos_video_timestamp = buf_tags.offset;
    err = put_u24_be(&buf_tags, 0);
    chk_err;
    pos_video_timestamp_ext = buf_tags.offset;
    err = put_u8(&buf_tags, 0);
    chk_err;
    err = put_u24_be(&buf_tags, 0); // StreamID
    chk_err;

    // Payload
    err = put_u8(&buf_tags, ((is_iframe ? 1 : 2) << 4) | codec_id);
    chk_err;
    err = put_u8(&buf_tags, 1); // *VCPacketType = NALU (AVC/HEVC)
    chk_err;
    err = put_u24_be(&buf_tags, 0); // CompositionTime
    chk_err;
    // Video data: 4-byte NALU length prefix + NALU (NO start code)
    err = put_u32_be(&buf_tags, nal_len);
    chk_err;
    err = put(&buf_tags, nal_data, nal_len);
    chk_err;

    uint32_t data_size = buf_tags.offset - (sz_off + 10);
    err = put_u24_be_to_offset(&buf_tags, sz_off, data_size);
    chk_err;
    uint32_t prev_size = (buf_tags.offset - tag_start);
    err = put_u32_be(&buf_tags, prev_size);
    chk_err;
    return BUF_OK;
}

static enum BufError flv_write_audio_mp3_tag() {
    if (!aud_samplerate || aud_codec != (char)0x69 || buf_aud.offset == 0)
        return BUF_OK; // Only handle MP3 chunks minimally
    enum BufError err;
    uint32_t tag_start = buf_tags.offset;
    err = put_u8(&buf_tags, 8); // Audio
    chk_err;
    uint32_t sz_off = buf_tags.offset;
    err = put_u24_be(&buf_tags, 0);
    chk_err;
    // Timestamp (patched later)
    pos_audio_timestamp = buf_tags.offset;
    err = put_u24_be(&buf_tags, 0);
    chk_err;
    pos_audio_timestamp_ext = buf_tags.offset;
    err = put_u8(&buf_tags, 0);
    chk_err;
    err = put_u24_be(&buf_tags, 0);
    chk_err;

    // SoundFormat(2) | SoundRate(2) | SoundSize(1) | SoundType(1)
    uint8_t fmt = (flv_audio_format_from_codec(aud_codec) << 4) |
                  (flv_audio_rate_code(aud_samplerate) << 2) |
                  (1 << 1) |
                  (aud_channels > 1 ? 1 : 0);
    err = put_u8(&buf_tags, fmt);
    chk_err;
    // Payload: raw MP3 frames concatenated
    err = put(&buf_tags, buf_aud.buf, buf_aud.offset);
    chk_err;

    uint32_t data_size = buf_tags.offset - (sz_off + 10);
    err = put_u24_be_to_offset(&buf_tags, sz_off, data_size);
    chk_err;
    uint32_t prev_size = (buf_tags.offset - tag_start);
    err = put_u32_be(&buf_tags, prev_size);
    chk_err;

    // Reset audio buffer after consuming
    buf_aud.offset = 0;
    trim_buffer(&buf_aud);
    return BUF_OK;
}

static enum BufError create_header() {
    if (buf_header.offset > 0) {
        return BUF_OK;
    }
    if (buf_sps_len == 0 || buf_pps_len == 0) {
        HAL_INFO("flv", "Waiting for SPS/PPS (SPS=%u, PPS=%u)\n", buf_sps_len, buf_pps_len);
        return BUF_OK;
    }
    if (flv_video_codec_id() == 12 && buf_vps_len == 0) {
        HAL_WARNING("flv", "Warning: HEVC is used, but without VPS\n");
        return BUF_OK;
    }

    HAL_INFO("flv", "Creating header (SPS=%u, PPS=%u, VPS=%u)\n", buf_sps_len, buf_pps_len, buf_vps_len);
    enum BufError err;
    buf_header.offset = 0;
    // Skip FLV file header and metadata - we send metadata via @setDataFrame
    // Just write the video sequence header
    if (flv_video_codec_id() == 12)
        err = flv_write_hevc_sequence_header();
    else
        err = flv_write_avc_sequence_header();
    chk_err;
    HAL_INFO("flv", "Following sequence header (offset=%zu)\n", buf_header.offset);
    return BUF_OK;
}

void flv_set_config(short width, short height, char framerate, char acodec,
    unsigned short bitrate, char channels, unsigned int srate) {
    vid_width = width;
    vid_height = height;
    vid_framerate = framerate;
    aud_codec = acodec;
    aud_bitrate = bitrate;
    aud_channels = channels;
    aud_samplerate = srate;
    if (aud_samplerate > 0) {
        aud_framesize =
            (aud_samplerate >= 32000 ? 144 : 72) *
            (aud_bitrate * 1000) /
            aud_samplerate;
    } else aud_framesize = 384;
}

void flv_set_sps(const char *nal_data, const uint32_t nal_len) {
    const char *p = nal_data; uint32_t l = nal_len; strip_annexb(&p, &l);
    buf_sps_len = MIN(l, sizeof(buf_sps));
    memcpy(buf_sps, p, buf_sps_len);
    create_header();
}

void flv_set_pps(const char *nal_data, const uint32_t nal_len) {
    const char *p = nal_data; uint32_t l = nal_len; strip_annexb(&p, &l);
    buf_pps_len = MIN(l, sizeof(buf_pps));
    memcpy(buf_pps, p, buf_pps_len);
    create_header();
}

void flv_set_vps(const char *nal_data, const uint32_t nal_len) {
    const char *p = nal_data; uint32_t l = nal_len; strip_annexb(&p, &l);
    buf_vps_len = MIN(l, sizeof(buf_vps));
    memcpy(buf_vps, p, buf_vps_len);
    create_header();
}

enum BufError flv_set_slice(const char *nal_data, const uint32_t nal_len,
    char is_iframe) {
    enum BufError err;
    buf_tags.offset = 0;
    pos_video_timestamp = 0;
    pos_video_timestamp_ext = 0;
    pos_audio_timestamp = 0;
    pos_audio_timestamp_ext = 0;

    // One video tag for the current slice
    err = flv_write_video_nalu(is_iframe, nal_data, nal_len);
    chk_err;
    
    return BUF_OK;
}

enum BufError flv_get_audio_tags(struct BitBuf *ptr) {
    if (!ptr) return BUF_INCORRECT;
    enum BufError err;
    
    // Use the shared tag buffer for audio tag construction
    buf_tags.offset = 0;
    pos_video_timestamp = 0;
    pos_video_timestamp_ext = 0;
    pos_audio_timestamp = 0;
    pos_audio_timestamp_ext = 0;
    
    // Write buffered audio as one or more tags
    err = flv_write_audio_mp3_tag();
    if (err != BUF_OK) return err;
    
    ptr->buf = buf_tags.buf;
    ptr->size = buf_tags.size;
    ptr->offset = buf_tags.offset;
    trim_buffer(&buf_tags);
    return BUF_OK;
}

enum BufError flv_ingest_audio(const char *data, const uint32_t len) {
    enum BufError err;
    err = put(&buf_aud, data, len);
    chk_err;
    return BUF_OK;
}

enum BufError flv_set_state(struct FlvState *state) {
    enum BufError err = BUF_OK;

    if (pos_video_timestamp) {
        err = put_u24_be_to_offset(&buf_tags, pos_video_timestamp, state->timestamp_ms & 0xFFFFFF);
        chk_err;
    }
    if (pos_video_timestamp_ext) {
        err = put_u8_to_offset(&buf_tags, pos_video_timestamp_ext, (state->timestamp_ms >> 24) & 0xFF);
        chk_err;
    }
    if (pos_audio_timestamp) {
        err = put_u24_be_to_offset(&buf_tags, pos_audio_timestamp, state->audio_timestamp_ms & 0xFFFFFF);
        chk_err;
    }
    if (pos_audio_timestamp_ext) {
        err = put_u8_to_offset(&buf_tags, pos_audio_timestamp_ext, (state->audio_timestamp_ms >> 24) & 0xFF);
        chk_err;
    }

    return BUF_OK;
}

enum BufError flv_inc_timestamp(struct FlvState *state) {
    if (state->frame_duration_ms == 0)
        state->frame_duration_ms = (vid_framerate > 0) ? (1000 / vid_framerate) : 33;

    state->timestamp_ms += state->frame_duration_ms;
    return BUF_OK;
}

enum BufError flv_inc_audio_timestamp(struct FlvState *state, uint32_t data_len) {
    // Duration (ms) = (Bits) / (Bitrate in kbps)
    // (bytes * 8) / bitrate
    if (aud_bitrate > 0)
        state->audio_timestamp_ms += (data_len * 8) / aud_bitrate;

    return BUF_OK;
}

enum BufError flv_get_header(struct BitBuf *ptr) {
    ptr->buf = buf_header.buf;
    ptr->size = buf_header.size;
    ptr->offset = buf_header.offset;
    return BUF_OK;
}

enum BufError flv_get_tags(struct BitBuf *ptr) {
    ptr->buf = buf_tags.buf;
    ptr->size = buf_tags.size;
    ptr->offset = buf_tags.offset;
    trim_buffer(&buf_tags);
    return BUF_OK;
}

// Generates AMF payload for "onMetaData" WITHOUT FLV Tag headers.
// Intended for use in RTMP_MSG_AMF_META (18) packet body.
enum BufError flv_get_metadata(struct BitBuf *ptr) {
    enum BufError err;
    
    err = AMFWriteString(ptr, "onMetaData", strlen("onMetaData"));
    chk_err;
    err = AMFWriteECMAArray(ptr);
    chk_err;
    // Width/Height/Framerate
    err = AMFWriteNamedDouble(ptr, "width", strlen("width"), vid_width);
    chk_err;
    err = AMFWriteNamedDouble(ptr, "height", strlen("height"), vid_height);
    chk_err;
    err = AMFWriteNamedDouble(ptr, "framerate", strlen("framerate"), vid_framerate);
    chk_err;
    // Codecs
    err = AMFWriteNamedDouble(ptr, "videocodecid", strlen("videocodecid"), flv_video_codec_id());
    chk_err;
    err = AMFWriteNamedDouble(ptr, "videodatarate", strlen("videodatarate"), 4000); // kbps
    chk_err;
    if (aud_samplerate) {
        err = AMFWriteNamedDouble(ptr, "audiocodecid", strlen("audiocodecid"),
                                  flv_audio_format_from_codec(aud_codec));
        chk_err;
        err = AMFWriteNamedDouble(ptr, "audiosamplerate", strlen("audiosamplerate"), aud_samplerate);
        chk_err;
        err = AMFWriteNamedDouble(ptr, "audiosamplesize", strlen("audiosamplesize"), 16);
        chk_err;
        err = AMFWriteNamedBoolean(ptr, "stereo", strlen("stereo"), aud_channels > 1 ? 1 : 0);
        chk_err;
        err = AMFWriteNamedDouble(ptr, "audiodatarate", strlen("audiodatarate"), aud_bitrate);
        chk_err;
    }
    err = AMFWriteNamedString(ptr, "encoder", strlen("encoder"), "Divinus", 7);
    chk_err;
    err = AMFWriteObjectEnd(ptr);
    chk_err;
    
    return BUF_OK;
}