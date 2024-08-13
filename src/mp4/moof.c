#include <string.h>

#include "moof.h"

uint32_t pos_sequence_number = 0;
uint32_t pos_base_data_offset = 0;
uint32_t pos_audio_media_decode_time = 0;
uint32_t pos_video_media_decode_time = 0;

struct DataOffsetPos {
    bool data_offset_present;
    uint32_t offset;
};

enum BufError write_mfhd(struct BitBuf *ptr, const uint32_t sequence_number);
enum BufError write_traf(
    struct BitBuf *ptr, const uint32_t sequence_number,
    const uint64_t base_data_offset, const uint64_t base_media_decode_time,
    const uint32_t default_sample_duration,
    const struct SampleInfo *samples_info, const uint32_t samples_info_len,
    struct DataOffsetPos *data_offset, char is_audio);
enum BufError write_tfhd(
    struct BitBuf *ptr, const uint32_t sequence_number,
    const uint64_t base_data_offset, const uint32_t default_sample_size,
    const uint32_t default_sample_duration, char is_audio);
enum BufError
write_tfdt(struct BitBuf *ptr, const uint64_t base_media_decode_time, char is_audio);
enum BufError write_trun(
    struct BitBuf *ptr, const struct SampleInfo *samples_info,
    const uint32_t samples_info_count, struct DataOffsetPos *data_offset, char is_audio);

enum BufError
write_mdat(struct BitBuf *ptr,
    const char *data_vid, const uint32_t len_vid,
    const char *data_aud, const uint32_t len_aud) {
    enum BufError err;
    uint32_t start_atom = ptr->offset;
    err = put_u32_be(ptr, 0);
    chk_err;
    err = put_str4(ptr, "mdat");
    chk_err;
    
    err = put_u32_be(ptr, len_vid);
    chk_err;
    err = put(ptr, data_vid, len_vid);
    chk_err;
    err = put(ptr, data_aud, len_aud);
    chk_err;
    err = put_u32_be_to_offset(ptr, start_atom, ptr->offset - start_atom);
    chk_err;
    return BUF_OK;
}

enum BufError write_moof(
    struct BitBuf *ptr, const uint32_t sequence_number,
    const uint64_t base_data_offset, const uint64_t base_media_decode_time,
    const uint32_t default_sample_duration, const struct SampleInfo *samples_vid,
    const uint32_t samples_vid_len, const struct SampleInfo *samples_aud,
    const uint32_t samples_aud_len) {
    enum BufError err;
    uint32_t start_atom = ptr->offset;
    err = put_u32_be(ptr, 0);
    chk_err;
    err = put_str4(ptr, "moof");
    chk_err;

    err = write_mfhd(ptr, sequence_number);
    chk_err;
    struct DataOffsetPos vid_offset = {0};
    struct DataOffsetPos aud_offset = {0};

    if (samples_vid_len && samples_vid[0].size) {
        err = write_traf(
            ptr, sequence_number, base_data_offset, base_media_decode_time,
            default_sample_duration, samples_vid, samples_vid_len,
            &vid_offset, 0);
        chk_err;
    }

    if (samples_aud_len && samples_aud[0].size) {
        err = write_traf(
            ptr, sequence_number, base_data_offset, base_media_decode_time,
            default_sample_duration, samples_aud, samples_aud_len,
            &aud_offset, 1);
        chk_err;
        uint32_t vid_mdat = ptr->offset + 4 /*mdat size*/ + 4 /*mdat id*/;

        if (aud_offset.data_offset_present) {
            uint32_t vid_len = 0;
            for (int i = 0; i < samples_vid_len; i++)
                vid_len += samples_vid[i].size;
            err = put_u32_be_to_offset(
                ptr, aud_offset.offset, vid_mdat + vid_len);
                chk_err;
        }
    }
    
    if (vid_offset.data_offset_present) {
        err = put_u32_be_to_offset(
            ptr, vid_offset.offset,
            ptr->offset + 4 /*mdat size*/ + 4 /*mdat id*/);
        chk_err;
    }

    err = put_u32_be_to_offset(ptr, start_atom, ptr->offset - start_atom);
    chk_err;
    return BUF_OK;
}

enum BufError write_mfhd(struct BitBuf *ptr, const uint32_t sequence_number) {
    enum BufError err;
    uint32_t start_atom = ptr->offset;
    err = put_u32_be(ptr, 0);
    chk_err;
    err = put_str4(ptr, "mfhd");
    chk_err;

    err = put_u8(ptr, 0);
    chk_err; // 1 version
    err = put_u8(ptr, 0);
    chk_err;

    err = put_u8(ptr, 0);
    chk_err;
    
    err = put_u8(ptr, 0);
    chk_err; // 3 flags
    pos_sequence_number = ptr->offset;
    err = put_u32_be(ptr, sequence_number);
    chk_err; // 4 sequence_number
    err = put_u32_be_to_offset(ptr, start_atom, ptr->offset - start_atom);
    chk_err;
    return BUF_OK;
}

enum BufError write_traf(
    struct BitBuf *ptr, const uint32_t sequence_number,
    const uint64_t base_data_offset, const uint64_t base_media_decode_time,
    const uint32_t default_sample_duration,
    const struct SampleInfo *samples_info, const uint32_t samples_info_len,
    struct DataOffsetPos *data_offset, char is_audio) {
    enum BufError err;
    uint32_t start_atom = ptr->offset;
    err = put_u32_be(ptr, 0);
    chk_err;
    err = put_str4(ptr, "traf");
    chk_err;

    err = write_tfhd(
        ptr, sequence_number, base_data_offset, samples_info[0].size, 
        default_sample_duration, is_audio);
    chk_err;
    err = write_tfdt(ptr, base_media_decode_time, is_audio);
    chk_err;
    err = write_trun(ptr, samples_info, samples_info_len, data_offset, is_audio);
    chk_err;
    err = put_u32_be_to_offset(ptr, start_atom, ptr->offset - start_atom);
    chk_err;
    return BUF_OK;
}

enum BufError write_tfhd(
    struct BitBuf *ptr, const uint32_t sequence_number,
    const uint64_t base_data_offset, const uint32_t default_sample_size, 
    const uint32_t default_sample_duration, char is_audio) {
    enum BufError err;
    uint32_t start_atom = ptr->offset;
    err = put_u32_be(ptr, 0);
    chk_err;
    err = put_str4(ptr, "tfhd");
    chk_err;

    err = put_u8(ptr, 0);
    chk_err; // 1 byte version
    uint64_t flags = 0x0;
    const bool base_data_offset_present = false;
    const bool sample_description_index_present = false;
    const bool default_sample_duration_present = false;
    const bool default_sample_size_present = true;
    const bool default_sample_flags_present = true;
    const bool duration_is_empty = false;
    const bool default_base_is_moof = true;

    if (base_data_offset_present) {
        flags = flags | 0x000001;
    } // base-data-offset-present
    if (sample_description_index_present) {
        flags = flags | 0x000002;
    } // sample-description-index-present
    if (default_sample_duration_present) {
        flags = flags | 0x000008;
    } // default-sample-duration-present
    if (default_sample_size_present) {
        flags = flags | 0x000010;
    } // default-sample-size-present
    if (default_sample_flags_present) {
        flags = flags | 0x000020;
    } // default-sample-flags-present
    if (duration_is_empty) {
        flags = flags | 0x010000;
    } // duration-is-empty
    if (default_base_is_moof) {
        flags = flags | 0x020000;
    } // default-base-is-moof
    err = put_u8(ptr, flags >> 16);
    chk_err;
    err = put_u8(ptr, flags >> 8);
    chk_err;
    err = put_u8(ptr, flags >> 0);
    chk_err; // 3 flags

    err = put_u32_be(ptr, is_audio ? 2 : 1);
    chk_err; // 4 track_ID
    if (base_data_offset_present) {
        pos_base_data_offset = ptr->offset;
        err = put_u64_be(ptr, base_data_offset);
        chk_err;
    }
    // if sample_description_index_present { buf.put_u32_be(0); } // 4
    // default_sample_description_index
    if (default_sample_duration_present) {
        err = put_u32_be(ptr, default_sample_duration);
        chk_err;
    }
    if (default_sample_size_present) {
        err = put_u32_be(ptr, default_sample_size);
        chk_err;
    }
    if (default_sample_flags_present) {
        err = put_u32_be(ptr, is_audio ? 33554432 : 16842752);
        chk_err;
    }

    err = put_u32_be_to_offset(ptr, start_atom, ptr->offset - start_atom);
    chk_err;
    return BUF_OK;
}

enum BufError
write_tfdt(struct BitBuf *ptr, const uint64_t base_media_decode_time, char is_audio) {
    enum BufError err;
    uint32_t start_atom = ptr->offset;
    err = put_u32_be(ptr, 0);
    chk_err;
    err = put_str4(ptr, "tfdt");
    chk_err;

    err = put_u8(ptr, 1);
    chk_err; // 1 version

    err = put_u8(ptr, 0);
    chk_err;
    err = put_u8(ptr, 0);
    chk_err;
    err = put_u8(ptr, 0);
    chk_err; // 3 flags
    if (is_audio)
        pos_audio_media_decode_time = ptr->offset;
    else
        pos_video_media_decode_time = ptr->offset;
    err = put_u64_be(ptr, base_media_decode_time);
    chk_err; // 8 baseMediaDecodeTime
    err = put_u32_be_to_offset(ptr, start_atom, ptr->offset - start_atom);
    chk_err;
    return BUF_OK;
}

enum BufError write_trun(
    struct BitBuf *ptr, const struct SampleInfo *samples_info,
    const uint32_t samples_info_count, struct DataOffsetPos *data_offset, char is_audio) {
    enum BufError err;
    uint32_t start_atom = ptr->offset;
    err = put_u32_be(ptr, 0);
    chk_err;
    err = put_str4(ptr, "trun");
    chk_err;

    err = put_u8(ptr, 0);
    chk_err; // 1 version
    bool data_offset_present = true;
    bool first_sample_flags_present = true;
    bool sample_duration_present = true;
    bool sample_size_present = true;
    {
        uint64_t flags = 0x0;
        if (data_offset_present) {
            flags = flags | 0x000001;
        } // 0x000001 data-offset-present.
        if (first_sample_flags_present) {
            flags = flags | 0x000004;
        } // 0x000004 first-sample-flags-present
        if (sample_duration_present) {
            flags = flags | 0x000100;
        } // 0x000100 sample-duration-present
        if (sample_size_present) {
            flags = flags | 0x000200;
        } // 0x000200 sample-size-present
        err = put_u8(ptr, flags >> 16);
        chk_err;
        
        err = put_u8(ptr, flags >> 8);
        chk_err;

        err = put_u8(ptr, flags >> 0);
        chk_err // 3 flags
    }
    err = put_u32_be(ptr, samples_info_count);
    chk_err; // 4 sample_count

    data_offset->data_offset_present = data_offset_present;
    data_offset->offset =
        ptr->offset; // save pointer to this place. we will change size after
                     // moof atom will created
    if (data_offset_present) {
        err = put_i32_be(ptr, 0);
        chk_err; // 4 fake data_offset
    }
    
    if (first_sample_flags_present) {
        err = put_u32_be(ptr, is_audio ? 0 : 33554432);
        chk_err; // 4 first_sample_flags
    }
    for (uint32_t i = 0; i < samples_info_count; ++i) {
        const struct SampleInfo sample_info = samples_info[i];
        if (sample_duration_present) {
            err = put_u32_be(ptr, sample_info.duration);
            chk_err; // 4 sample_duration
        } 
        if (sample_size_present) {
            err = put_u32_be(ptr, sample_info.size);
            chk_err; // 4 sample_size
        }
    }

    err = put_u32_be_to_offset(ptr, start_atom, ptr->offset - start_atom);
    chk_err;
    return BUF_OK;
}
