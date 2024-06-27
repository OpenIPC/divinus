#include <string.h>

#include "moov.h"

#include "nal.h"

enum BufError write_ftyp(struct BitBuf *ptr, const struct MoovInfo *moov_info);
enum BufError write_moov(struct BitBuf *ptr, const struct MoovInfo *moov_info);

enum BufError write_mvhd(struct BitBuf *ptr, const struct MoovInfo *moov_info);
enum BufError write_trak(struct BitBuf *ptr, const struct MoovInfo *moov_info);
enum BufError write_tkhd(struct BitBuf *ptr, const struct MoovInfo *moov_info);
enum BufError write_mdia(struct BitBuf *ptr, const struct MoovInfo *moov_info);
enum BufError write_mdhd(struct BitBuf *ptr, const struct MoovInfo *moov_info);
enum BufError write_minf(struct BitBuf *ptr, const struct MoovInfo *moov_info);
enum BufError write_dinf(struct BitBuf *ptr);
enum BufError write_dref(struct BitBuf *ptr);
enum BufError write_url(struct BitBuf *ptr);
enum BufError write_vmhd(struct BitBuf *ptr);
enum BufError write_smhd(struct BitBuf *ptr);
enum BufError write_stbl(struct BitBuf *ptr, const struct MoovInfo *moov_info);
enum BufError write_stsd(struct BitBuf *ptr, const struct MoovInfo *moov_info);
enum BufError write_avc1_hev1(struct BitBuf *ptr, const struct MoovInfo *moov_info);
enum BufError write_avcC(struct BitBuf *ptr, const struct MoovInfo *moov_info);
enum BufError write_hvcC(struct BitBuf *ptr, const struct MoovInfo *moov_info);
enum BufError write_stts(struct BitBuf *ptr);
enum BufError write_stsc(struct BitBuf *ptr);
enum BufError write_stsz(struct BitBuf *ptr);
enum BufError write_stco(struct BitBuf *ptr);
enum BufError write_mvex(struct BitBuf *ptr);
enum BufError write_trex(struct BitBuf *ptr);
enum BufError write_udta(struct BitBuf *ptr);
enum BufError write_meta(struct BitBuf *ptr);
enum BufError write_hdlr(
    struct BitBuf *ptr, const char name[4], const char manufacturer[4],
    const char *value, const uint32_t value_len);
enum BufError
write_ilst(struct BitBuf *ptr, const uint8_t *array, const uint32_t len);

enum BufError write_header(struct BitBuf *ptr, struct MoovInfo *moov_info) {
    enum BufError err;
    err = write_ftyp(ptr, moov_info);
    chk_err;
    err = write_moov(ptr, moov_info);
    chk_err;
    return BUF_OK;
}

enum BufError write_ftyp(struct BitBuf *ptr, const struct MoovInfo *moov_info) {
    enum BufError err;
    // atom header  <fake size><id>
    uint32_t start_atom = ptr->offset;
    err = put_u32_be(ptr, 0);
    chk_err;

    err = put_str4(ptr, "ftyp");
    chk_err;

    err = put_str4(ptr, "isom");
    chk_err; // major_brand
    err = put_u32_be(ptr, 0x00000200);
    chk_err; // minor_version
    err = put_str4(ptr, "isom");
    chk_err;
    err = put_str4(ptr, "iso2");
    chk_err;

    if (moov_info->isH265)
        err = put_str4(ptr, "hvc1");
    else
        err = put_str4(ptr, "avc1");
    chk_err;
    
    err = put_str4(ptr, "iso6");
    chk_err;
    err = put_str4(ptr, "mp41");
    chk_err;

    // write atom size
    uint32_t atom_size = ptr->offset - start_atom;
    err = put_u32_be_to_offset(ptr, start_atom, atom_size);
    chk_err;
    return BUF_OK;
}

enum BufError write_moov(struct BitBuf *ptr, const struct MoovInfo *moov_info) {
    enum BufError err;
    uint32_t start_atom = ptr->offset;
    err = put_u32_be(ptr, 0);
    chk_err;

    err = put_str4(ptr, "moov");
    chk_err;
    err = write_mvhd(ptr, moov_info);
    chk_err;
    err = write_trak(ptr, moov_info);
    chk_err;
    err = write_mvex(ptr);
    chk_err;
    err = write_udta(ptr);
    chk_err;
    err = put_u32_be_to_offset(ptr, start_atom, ptr->offset - start_atom);
    chk_err;
    return BUF_OK;
}

enum BufError write_mvhd(struct BitBuf *ptr, const struct MoovInfo *moov_info) {
    enum BufError err;
    uint32_t start_atom = ptr->offset;
    err = put_u32_be(ptr, 0);
    chk_err;

    err = put_str4(ptr, "mvhd");
    chk_err;

    err = put_u8(ptr, 0);
    chk_err; // 1 version

    err = put_u8(ptr, 0);
    chk_err;
    err = put_u8(ptr, 0);
    chk_err;
    err = put_u8(ptr, 0);
    chk_err; // 3 flags

    // A 32-bit integer that specifies the calendar date and time (in seconds
    // since midnight, January 1, 1904) when the movie atom was created in
    // coordinated universal time (UTC)
    err = put_u32_be(ptr, moov_info->creation_time);
    chk_err; // 4 creation_time
    err = put_u32_be(ptr, 0);
    chk_err; // 4 modification_time

    // A time value that indicates the time scale for this movie—that is, the
    // number of time units that pass per second in its time coordinate system
    err = put_u32_be(ptr, moov_info->timescale);
    chk_err; // 4 timescale

    // A time value that indicates the duration of the movie in time scale
    // units, derived from the movie’s tracks, corresponding to the duration of
    // the longest track in the movie
    err = put_u32_be(ptr, 0);
    chk_err; // 4 duration

    // A 32-bit fixed-point number that specifies the rate at which to play this
    // movie (a value of 1.0 indicates normal rate); set here to '0x00010000'
    err = put_u32_be(ptr, 65536);
    chk_err; // 4 preferred rate

    // A 16-bit fixed-point number that specifies how loud to play this movie’s
    // sound (a value of 1.0 indicates full volume); set here to '0x0100'
    err = put_u16_le(ptr, 1);
    chk_err; // 2 preferred volume

    err = put_skip(ptr, 10);
    chk_err; // 10 reserved
    {        // 36 matrix
        err = put_u32_be(ptr, 0x10000);
        chk_err;
        err = put_u32_be(ptr, 0);
        chk_err;
        err = put_u32_be(ptr, 0);
        chk_err;
        err = put_u32_be(ptr, 0);
        chk_err;
        err = put_u32_be(ptr, 0x10000);
        chk_err;
        err = put_u32_be(ptr, 0);
        chk_err;
        err = put_u32_be(ptr, 0);
        chk_err;
        err = put_u32_be(ptr, 0);
        chk_err;
        err = put_u32_be(ptr, 0x40000000);
        chk_err;
    }
    err = put_u32_be(ptr, 0);
    chk_err; // 4 Preview time
    err = put_u32_be(ptr, 0);
    chk_err; // 4 Preview duration
    err = put_u32_be(ptr, 0);
    chk_err; // 4 Poster time
    err = put_u32_be(ptr, 0);
    chk_err; // 4 Selection time
    err = put_u32_be(ptr, 0);
    chk_err; // 4 Selection duration
    err = put_u32_be(ptr, 0);
    chk_err; // 4 Current time
    err = put_u32_be(ptr, 2);
    chk_err; // 4 Next track ID

    err = put_u32_be_to_offset(ptr, start_atom, ptr->offset - start_atom);
    chk_err;
    return BUF_OK;
}

enum BufError write_trak(struct BitBuf *ptr, const struct MoovInfo *moov_info) {
    enum BufError err;
    uint32_t start_atom = ptr->offset;
    err = put_u32_be(ptr, 0);
    chk_err;

    err = put_str4(ptr, "trak");
    chk_err;
    err = write_tkhd(ptr, moov_info);
    chk_err;
    err = write_mdia(ptr, moov_info);
    chk_err;
    err = put_u32_be_to_offset(ptr, start_atom, ptr->offset - start_atom);
    chk_err;
    return BUF_OK;
}

enum BufError write_tkhd(struct BitBuf *ptr, const struct MoovInfo *moov_info) {
    enum BufError err;
    uint32_t start_atom = ptr->offset;
    err = put_u32_be(ptr, 0);
    chk_err;

    err = put_str4(ptr, "tkhd");
    chk_err;

    err = put_u8(ptr, 0); // 1 version
    err = put_u8(ptr, 0);
    chk_err;

    err = put_u8(ptr, 0);
    chk_err;

    err = put_u8(ptr, 3);
    chk_err;                                         // 3 flags
    err = put_u32_be(ptr, moov_info->creation_time); // 4 creation_time
    err = put_u32_be(ptr, 0);                        // 4 modification_time
    err = put_u32_be(ptr, 1);                        // 4 track id
    err = put_u32_be(ptr, 0);                        // 4 reserved
    err = put_u32_be(ptr, 0);                        // 4 duration
    err = put_skip(ptr, 8);                          // 8 reserved
    err = put_u16_be(ptr, 0);                        // 2 layer
    err = put_u16_be(ptr, 0);                        // 2 Alternate group
    err = put_u16_be(ptr, 0);                        // 2 Volume
    err = put_u16_be(ptr, 0);                        // 2 Reserved
    {                                                // 36 Matrix structure
        err = put_u32_be(ptr, 0x10000);
        chk_err;
        err = put_u32_be(ptr, 0);
        chk_err;
        err = put_u32_be(ptr, 0);
        chk_err;
        err = put_u32_be(ptr, 0);
        chk_err;
        err = put_u32_be(ptr, 0x10000);
        chk_err;
        err = put_u32_be(ptr, 0);
        chk_err;
        err = put_u32_be(ptr, 0);
        chk_err;
        err = put_u32_be(ptr, 0);
        chk_err;
        err = put_u32_be(ptr, 0x40000000);
        chk_err;
    }
    err = put_u32_be(ptr, moov_info->width << 16);
    chk_err; // 4 Track width
    err = put_u32_be(ptr, moov_info->height << 16);
    chk_err; // 4 Track height

    err = put_u32_be_to_offset(ptr, start_atom, ptr->offset - start_atom);
    chk_err;
    return BUF_OK;
}

enum BufError write_mdia(struct BitBuf *ptr, const struct MoovInfo *moov_info) {
    enum BufError err;
    uint32_t start_atom = ptr->offset;
    err = put_u32_be(ptr, 0);
    chk_err;

    err = put_str4(ptr, "mdia");
    chk_err;
    err = write_mdhd(ptr, moov_info);
    chk_err;
    char *str = "VideoHandler";
    err = write_hdlr(ptr, "vide", "\0\0\0\0", str, strlen(str));
    chk_err;
    err = write_minf(ptr, moov_info);
    chk_err;

    err = put_u32_be_to_offset(ptr, start_atom, ptr->offset - start_atom);
    chk_err;
    return BUF_OK;
}

enum BufError write_mdhd(struct BitBuf *ptr, const struct MoovInfo *moov_info) {
    enum BufError err;
    uint32_t start_atom = ptr->offset;
    err = put_u32_be(ptr, 0);
    chk_err;

    err = put_str4(ptr, "mdhd");
    chk_err;
    err = put_u8(ptr, 0); // 1 version
    err = put_u8(ptr, 0);
    chk_err;

    err = put_u8(ptr, 0);
    chk_err;

    err = put_u8(ptr, 0);
    chk_err; // 3 flags
    err = put_u32_be(ptr, 0);
    chk_err; // 4 creation_time
    err = put_u32_be(ptr, 0);
    chk_err; // 4 modification_time
    err = put_u32_be(ptr, moov_info->timescale);
    chk_err; // 4 timescale
    err = put_u32_be(ptr, 0);
    chk_err; // 4 duration
    err = put_u16_be(ptr, 0x55C4);
    chk_err; // 2 language
    err = put_u16_be(ptr, 0);
    chk_err; // 2 quality
    err = put_u32_be_to_offset(ptr, start_atom, ptr->offset - start_atom);
    chk_err;
    return BUF_OK;
}

enum BufError write_minf(struct BitBuf *ptr, const struct MoovInfo *moov_info) {
    enum BufError err;
    uint32_t start_atom = ptr->offset;
    err = put_u32_be(ptr, 0);
    chk_err;

    err = put_str4(ptr, "minf");
    chk_err;
    err = write_vmhd(ptr);
    chk_err;
    err = write_dinf(ptr);
    chk_err;
    err = write_stbl(ptr, moov_info);
    chk_err;
    err = put_u32_be_to_offset(ptr, start_atom, ptr->offset - start_atom);
    chk_err;
    return BUF_OK;
}

enum BufError write_dinf(struct BitBuf *ptr) {
    enum BufError err;
    uint32_t start_atom = ptr->offset;
    err = put_u32_be(ptr, 0);
    chk_err;

    err = put_str4(ptr, "dinf");
    chk_err;
    err = write_dref(ptr);
    chk_err;
    err = put_u32_be_to_offset(ptr, start_atom, ptr->offset - start_atom);
    chk_err;
    return BUF_OK;
}

enum BufError write_dref(struct BitBuf *ptr) {
    enum BufError err;
    uint32_t start_atom = ptr->offset;
    err = put_u32_be(ptr, 0);
    chk_err;

    err = put_str4(ptr, "dref");
    chk_err;
    err = put_u8(ptr, 0);
    chk_err; // 1 version
    err = put_u8(ptr, 0);
    chk_err;

    err = put_u8(ptr, 0);
    chk_err;

    err = put_u8(ptr, 0);
    chk_err; // 3 flags
    err = put_u32_be(ptr, 1);
    chk_err; // 4 Component flags mask
    err = write_url(ptr);
    chk_err;
    err = put_u32_be_to_offset(ptr, start_atom, ptr->offset - start_atom);
    chk_err;
    return BUF_OK;
}

enum BufError write_url(struct BitBuf *ptr) {
    enum BufError err;
    uint32_t start_atom = ptr->offset;
    err = put_u32_be(ptr, 0);
    chk_err;

    err = put_str4(ptr, "url ");
    chk_err;
    err = put_u8(ptr, 0);
    chk_err; // 1 version
    err = put_u8(ptr, 0);
    chk_err;

    err = put_u8(ptr, 0);
    chk_err;

    err = put_u8(ptr, 1);
    chk_err; // 3 flags
    err = put_u32_be_to_offset(ptr, start_atom, ptr->offset - start_atom);
    chk_err;
    return BUF_OK;
}

enum BufError write_vmhd(struct BitBuf *ptr) {
    enum BufError err;
    uint32_t start_atom = ptr->offset;
    err = put_u32_be(ptr, 0);
    chk_err;

    err = put_str4(ptr, "vmhd");
    chk_err;

    err = put_u8(ptr, 0);
    chk_err; // 1 version

    err = put_u8(ptr, 0);
    chk_err;
    err = put_u8(ptr, 0);
    chk_err;
    err = put_u8(ptr, 1);
    chk_err; // 3 flags

    err = put_u16_be(ptr, 0);
    chk_err; // 2 Graphics mode
    err = put_u16_be(ptr, 0);
    chk_err; // 2 Opcolor
    err = put_u16_be(ptr, 0);
    chk_err; // 2 Opcolor
    err = put_u16_be(ptr, 0);
    chk_err; // 2 Opcolor
    err = put_u32_be_to_offset(ptr, start_atom, ptr->offset - start_atom);
    chk_err;
    return BUF_OK;
}

enum BufError write_smhd(struct BitBuf *ptr) {
    enum BufError err;
    uint32_t start_atom = ptr->offset;
    err = put_u32_be(ptr, 0);
    chk_err;

    err = put_str4(ptr, "smhd");
    chk_err;

    err = put_u8(ptr, 0);
    chk_err; // 1 version

    err = put_u8(ptr, 0);
    chk_err;
    err = put_u8(ptr, 0);
    chk_err;
    err = put_u8(ptr, 0);
    chk_err; // 3 flags

    err = put_u16_be(ptr, 0);
    chk_err; // 2 Balance
    err = put_u16_be(ptr, 0);
    chk_err; // 2 Apple reserved
    err = put_u32_be_to_offset(ptr, start_atom, ptr->offset - start_atom);
    chk_err;
    return BUF_OK;
}

enum BufError write_stbl(struct BitBuf *ptr, const struct MoovInfo *moov_info) {
    enum BufError err;
    uint32_t start_atom = ptr->offset;
    err = put_u32_be(ptr, 0);
    chk_err;

    err = put_str4(ptr, "stbl");
    chk_err;
    err = write_stsd(ptr, moov_info);
    chk_err;
    err = write_stts(ptr);
    chk_err;
    err = write_stsc(ptr);
    chk_err;
    err = write_stsz(ptr);
    chk_err;
    err = write_stco(ptr);
    chk_err;
    err = put_u32_be_to_offset(ptr, start_atom, ptr->offset - start_atom);
    chk_err;
    return BUF_OK;
}

enum BufError write_stsd(struct BitBuf *ptr, const struct MoovInfo *moov_info) {
    enum BufError err;
    uint32_t start_atom = ptr->offset;
    err = put_u32_be(ptr, 0);
    chk_err;

    err = put_str4(ptr, "stsd");
    chk_err;
    err = put_u8(ptr, 0);
    chk_err; // 1 version
    err = put_u8(ptr, 0);
    chk_err;

    err = put_u8(ptr, 0);
    chk_err;

    err = put_u8(ptr, 0);
    chk_err; // 3 flags
    err = put_u32_be(ptr, 1);
    chk_err; // 4  Number of entries
    err = write_avc1_hev1(ptr, moov_info);
    chk_err;
    err = put_u32_be_to_offset(ptr, start_atom, ptr->offset - start_atom);
    chk_err;
    return BUF_OK;
}

enum BufError write_avc1_hev1(struct BitBuf *ptr, const struct MoovInfo *moov_info) {
    enum BufError err;
    uint32_t start_atom = ptr->offset;
    err = put_u32_be(ptr, 0);
    chk_err;

    if (moov_info->isH265)
        err = put_str4(ptr, "hvc1");
    else
        err = put_str4(ptr, "avc1");
    chk_err;

    err = put_u8(ptr, 0);
    chk_err;

    err = put_u8(ptr, 0);
    chk_err;

    err = put_u8(ptr, 0);
    chk_err; // reserved
    err = put_u8(ptr, 0);
    chk_err;

    err = put_u8(ptr, 0);
    chk_err;

    err = put_u8(ptr, 0);
    chk_err; // reserved
    err = put_u16_be(ptr, 1);
    chk_err; // data_reference_index
    err = put_u16_be(ptr, 0);
    chk_err; // pre_defined
    err = put_u16_be(ptr, 0);
    chk_err; // reserved
    err = put_u32_be(ptr, 0);
    chk_err;
    err = put_u32_be(ptr, 0);
    chk_err;
    err = put_u32_be(ptr, 0);
    chk_err; // pre_defined
    err = put_u16_be(ptr, moov_info->width);
    chk_err; // 2 width
    err = put_u16_be(ptr, moov_info->height);
    chk_err; // 2 height
    err = put_u32_be(ptr, moov_info->horizontal_resolution);
    chk_err; // 4 horizontal_resolution
    err = put_u32_be(ptr, moov_info->vertical_resolution);
    chk_err; // 4 vertical_resolution
    err = put_u32_be(ptr, 0);
    chk_err; // reserved
    err = put_u16_be(ptr, 1);
    chk_err; // 2 frame_count
    err = put_u8(ptr, 0);
    chk_err;
    char compressorname[50] = "OpenIPC project                    ";

    err = put(ptr, compressorname, 31);
    chk_err; // compressorname
    err = put_u16_be(ptr, 24);
    chk_err; // 2 depth
    err = put_u16_be(ptr, 0xffff);
    chk_err; // 2 color_table_id

    if (moov_info->isH265)
        err = write_hvcC(ptr, moov_info);
    else
        err = write_avcC(ptr, moov_info);
    chk_err;

    err = put_u32_be_to_offset(ptr, start_atom, ptr->offset - start_atom);
    chk_err;
    return BUF_OK;
}

enum BufError write_avcC(struct BitBuf *ptr, const struct MoovInfo *moov_info) {
    enum BufError err;
    uint32_t start_atom = ptr->offset;
    err = put_u32_be(ptr, 0);
    chk_err;

    err = put_str4(ptr, "avcC");
    chk_err;

    err = put_u8(ptr, 1);
    chk_err; // 1 version
    err = put_u8(ptr, moov_info->profile_idc);
    chk_err; // 1 profile
    err = put_u8(ptr, 0);
    chk_err; // 1 compatibility
    err = put_u8(ptr, moov_info->level_idc);
    chk_err; // 1 level
    err = put_u8(ptr, 0xFF);
    chk_err; // 6 bits reserved (111111) + 2 bits nal size length - 1 (11)
    err = put_u8(ptr, 0xE1);
    chk_err; // 3 bits reserved (111) + 5 bits number of sps (00001)
    err = put_u16_be(ptr, moov_info->sps_length);
    chk_err;
    err = put(ptr, (const char *)moov_info->sps, moov_info->sps_length);
    chk_err; // SPS
    err = put_u8(ptr, 1);
    chk_err; // 1 num pps
    err = put_u16_be(ptr, moov_info->pps_length);
    chk_err;
    err = put(ptr, (const char *)moov_info->pps, moov_info->pps_length);
    chk_err; // pps

    err = put_u32_be_to_offset(ptr, start_atom, ptr->offset - start_atom);
    chk_err;
    return BUF_OK;
}

/* Discards emulation prevention three bytes */
static inline size_t nal_decode(const uint8_t *p_src, uint8_t *p_dst, size_t i_size) {
    size_t j = 0;
    for (size_t i = 0; i < i_size; i++) {
        if (i < i_size - 3 && p_src[i] == 0 && p_src[i + 1] == 0 &&
            p_src[i + 2] == 3) {
            p_dst[j++] = 0;
            p_dst[j++] = 0;
            i += 2;
            continue;
        }
        p_dst[j++] = p_src[i];
    }
    return j;
}

static void hevcParseVPS(
    uint8_t *p_buffer, size_t i_buffer, uint8_t *general, uint8_t *numTemporalLayer,
    bool *temporalIdNested) {
    const size_t i_decoded_nal_size = 512;
    uint8_t p_dec_nal[i_decoded_nal_size];
    size_t i_size =
        (i_buffer < i_decoded_nal_size) ? i_buffer : i_decoded_nal_size;
    nal_decode(p_buffer, p_dec_nal, i_size);

    /* first two bytes are the NAL header, 3rd and 4th are:
        vps_video_parameter_set_id(4)
        vps_reserved_3_2bis(2)
        vps_max_layers_minus1(6)
        vps_max_sub_layers_minus1(3)
        vps_temporal_id_nesting_flags
    */
    *numTemporalLayer = ((p_dec_nal[3] & 0x0E) >> 1) + 1;
    *temporalIdNested = (bool)(p_dec_nal[3] & 0x01);

    /* 5th & 6th are reserved 0xffff */
    /* copy the first 12 bytes of profile tier */
    memcpy(general, &p_dec_nal[6], 12);
}

// HEVC Configuration Atom
// http://git.videolan.org/?p=vlc.git;a=blob;f=modules/mux/mp4.c;h=92fe09a4d26480ca5a707324cea167bf01d538fb;hb=HEAD#l881
// https://lists.matroska.org/pipermail/matroska-devel/2013-September/004567.html
enum BufError write_hvcC(struct BitBuf *ptr, const struct MoovInfo *moov_info) {
    enum BufError err;
    uint32_t start_atom = ptr->offset;
    err = put_u32_be(ptr, 0);
    chk_err;

    err = put_str4(ptr, "hvcC");
    chk_err;

    err = put_u8(ptr, 1);
    chk_err; // 1 version

    uint8_t general_configuration[12] = {0}, i_numTemporalLayer = 0;
    bool b_temporalIdNested = false;
    hevcParseVPS(
        (uint8_t *)moov_info->vps, moov_info->vps_length, general_configuration,
        &i_numTemporalLayer, &b_temporalIdNested);
    err = put(
        ptr, (const uint8_t *)general_configuration, sizeof(general_configuration));
    chk_err; 

    err = put_u16_be(ptr, 0xF000);
    chk_err; // spatial_seg

    err = put_u8(ptr, 0xFC);
    chk_err; // parallelism

    uint8_t i_chroma_idc = 1, i_bit_depth_luma_minus8 = 0, i_bit_depth_chroma_minus8 = 0;
    err = put_u8(ptr, 0xFC | (i_chroma_idc & 0x03));
    chk_err;
    err = put_u8(ptr, 0xF8 | (i_bit_depth_luma_minus8 & 0x07));
    chk_err;
    err = put_u8(ptr, 0xF8 | (i_bit_depth_chroma_minus8 & 0x07));
    chk_err;

    err = put_u16_be(ptr, 0);
    chk_err; // framerate
    
    err = put_u8(ptr, 0x0F); // from VPS
    chk_err; // nal_size

    err = put_u8(ptr, 3);
    chk_err; // nal_num

    err = put_u8(ptr, NalUnitType_VPS_HEVC);
    chk_err; 
    err = put_u16_be(ptr, 1);
    chk_err; // 1 num vps
    err = put_u16_be(ptr, moov_info->vps_length);
    chk_err;
    err = put(ptr, (const char *)moov_info->vps, moov_info->vps_length);
    chk_err; // vps
    err = put_u8(ptr, NalUnitType_SPS_HEVC);
    chk_err; 
    err = put_u16_be(ptr, 1);
    chk_err; // 1 num sps
    err = put_u16_be(ptr, moov_info->sps_length);
    chk_err;
    err = put(ptr, (const char *)moov_info->sps, moov_info->sps_length);
    chk_err; // sps
    err = put_u8(ptr, NalUnitType_PPS_HEVC);
    chk_err; 
    err = put_u16_be(ptr, 1);
    chk_err; // 1 num pps
    err = put_u16_be(ptr, moov_info->pps_length);
    chk_err;
    err = put(ptr, (const char *)moov_info->pps, moov_info->pps_length);
    chk_err; // pps

    err = put_u32_be_to_offset(ptr, start_atom, ptr->offset - start_atom);
    chk_err;
    return BUF_OK;
}

enum BufError write_stts(struct BitBuf *ptr) {
    enum BufError err;
    uint32_t start_atom = ptr->offset;
    err = put_u32_be(ptr, 0);
    chk_err;

    err = put_str4(ptr, "stts");
    chk_err;

    err = put_u8(ptr, 0);
    chk_err; // 1 version
    err = put_u8(ptr, 0);
    chk_err;

    err = put_u8(ptr, 0);
    chk_err;

    err = put_u8(ptr, 0);
    chk_err; // 3 flags
    err = put_u32_be(ptr, 0);
    chk_err; // Number of entries
    // Time-to-sample table
    err = put_u32_be_to_offset(ptr, start_atom, ptr->offset - start_atom);
    chk_err;
    return BUF_OK;
}

enum BufError write_stsc(struct BitBuf *ptr) {
    enum BufError err;
    uint32_t start_atom = ptr->offset;
    err = put_u32_be(ptr, 0);
    chk_err;

    err = put_str4(ptr, "stsc");
    chk_err;
    err = put_u8(ptr, 0);
    chk_err; // 1 version
    err = put_u8(ptr, 0);
    chk_err;

    err = put_u8(ptr, 0);
    chk_err;

    err = put_u8(ptr, 0);
    chk_err; // 3 flags
    err = put_u32_be(ptr, 0);
    chk_err; // Number of entries
    err = put_u32_be_to_offset(ptr, start_atom, ptr->offset - start_atom);
    chk_err;
    return BUF_OK;
}

enum BufError write_stsz(struct BitBuf *ptr) {
    enum BufError err;
    uint32_t start_atom = ptr->offset;
    err = put_u32_be(ptr, 0);
    chk_err;

    err = put_str4(ptr, "stsz");
    chk_err;
    err = put_u8(ptr, 0);
    chk_err; // 1 version
    err = put_u8(ptr, 0);
    chk_err;

    err = put_u8(ptr, 0);
    chk_err;

    err = put_u8(ptr, 0);
    chk_err; // 3 flags
    err = put_u32_be(ptr, 0);
    chk_err; // Sample size
    err = put_u32_be(ptr, 0);
    chk_err; // Number of entries
    err = put_u32_be_to_offset(ptr, start_atom, ptr->offset - start_atom);
    chk_err;
    return BUF_OK;
}

enum BufError write_stco(struct BitBuf *ptr) {
    enum BufError err;
    uint32_t start_atom = ptr->offset;
    err = put_u32_be(ptr, 0);
    chk_err;

    err = put_str4(ptr, "stco");
    chk_err;
    err = put_u8(ptr, 0);
    chk_err; // 1 version
    err = put_u8(ptr, 0);
    chk_err;

    err = put_u8(ptr, 0);
    chk_err;

    err = put_u8(ptr, 0);
    chk_err; // 3 flags
    err = put_u32_be(ptr, 0);
    chk_err; // Number of entries
    err = put_u32_be_to_offset(ptr, start_atom, ptr->offset - start_atom);
    chk_err;
    return BUF_OK;
}

enum BufError write_mvex(struct BitBuf *ptr) {
    enum BufError err;
    uint32_t start_atom = ptr->offset;
    err = put_u32_be(ptr, 0);
    chk_err;

    err = put_str4(ptr, "mvex");
    chk_err;
    err = write_trex(ptr);
    chk_err;
    err = put_u32_be_to_offset(ptr, start_atom, ptr->offset - start_atom);
    chk_err;
    return BUF_OK;
}

enum BufError write_trex(struct BitBuf *ptr) {
    enum BufError err;
    uint32_t start_atom = ptr->offset;
    err = put_u32_be(ptr, 0);
    chk_err;

    err = put_str4(ptr, "trex");
    chk_err;
    err = put_u8(ptr, 0);
    chk_err; // 1 version
    err = put_u8(ptr, 0);
    chk_err;

    err = put_u8(ptr, 0);
    chk_err;

    err = put_u8(ptr, 0);
    chk_err; // 3 flags
    err = put_u32_be(ptr, 1);
    chk_err; // track_ID
    err = put_u32_be(ptr, 1);
    chk_err; // default_sample_description_index
    err = put_u32_be(ptr, 0);
    chk_err; // default_sample_duration
    err = put_u32_be(ptr, 0);
    chk_err; // default_sample_size
    err = put_u32_be(ptr, 0);
    chk_err; // default_sample_flags
    err = put_u32_be_to_offset(ptr, start_atom, ptr->offset - start_atom);
    chk_err;
    return BUF_OK;
}

enum BufError write_udta(struct BitBuf *ptr) {
    enum BufError err;
    uint32_t start_atom = ptr->offset;
    err = put_u32_be(ptr, 0);
    chk_err;

    err = put_str4(ptr, "udta");
    chk_err;
    err = write_meta(ptr);
    chk_err;
    err = put_u32_be_to_offset(ptr, start_atom, ptr->offset - start_atom);
    chk_err;
    return BUF_OK;
}

enum BufError write_meta(struct BitBuf *ptr) {
    enum BufError err;
    uint32_t start_atom = ptr->offset;
    err = put_u32_be(ptr, 0);
    chk_err;

    err = put_str4(ptr, "meta");
    chk_err;
    err = put_u8(ptr, 0);
    chk_err; // 1 version
    err = put_u8(ptr, 0);
    chk_err;

    err = put_u8(ptr, 0);
    chk_err;

    err = put_u8(ptr, 0);
    chk_err; // 3 flags
    err = write_hdlr(ptr, "mdir", "appl", "", 0);
    chk_err;
    uint8_t array[37] = {0,  0,  0,   37, 169, 116, 111, 111, 0,  0,
                         0,  29, 100, 97, 116, 97,  0,   0,   0,  1,
                         0,  0,  0,   0,  76,  97,  118, 102, 53, 55,
                         46, 56, 51,  46, 49,  48,  48};
    err = write_ilst(ptr, array, 37);
    chk_err;
    err = put_u32_be_to_offset(ptr, start_atom, ptr->offset - start_atom);
    chk_err;
    return BUF_OK;
}

enum BufError write_hdlr(
    struct BitBuf *ptr, const char name[4], const char manufacturer[4],
    const char *value, const uint32_t value_len) {
    enum BufError err;
    uint32_t start_atom = ptr->offset;
    err = put_u32_be(ptr, 0);
    chk_err;

    err = put_str4(ptr, "hdlr");
    chk_err;
    err = put_u8(ptr, 0);
    chk_err; // 1 version
    err = put_u8(ptr, 0);
    chk_err;

    err = put_u8(ptr, 0);
    chk_err;

    err = put_u8(ptr, 0);
    chk_err; // 3 flags
    err = put_u32_be(ptr, 0);
    chk_err; // 4 Predefined
    err = put_str4(ptr, name);
    chk_err; // 4 Component subtype
    err = put_str4(ptr, manufacturer);
    chk_err; // 4 Component manufacturer
    err = put_u32_be(ptr, 0);
    chk_err; // 4 Component flags
    err = put_u32_be(ptr, 0);
    chk_err; // 4 Component flags mask
    err = put_counted_str(ptr, value, value_len);
    chk_err; // <counted string> Component name
    err = put_u32_be_to_offset(ptr, start_atom, ptr->offset - start_atom);
    chk_err;
    return BUF_OK;
}

enum BufError
write_ilst(struct BitBuf *ptr, const uint8_t *array, const uint32_t len) {
    enum BufError err;
    uint32_t start_atom = ptr->offset;
    err = put_u32_be(ptr, 0);
    chk_err;

    err = put_str4(ptr, "ilst");
    chk_err;
    err = put(ptr, array, len);
    chk_err; // <counted string> Component name
    err = put_u32_be_to_offset(ptr, start_atom, ptr->offset - start_atom);
    chk_err;
    return BUF_OK;
}
