#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

enum NalUnitType {                    //   Table 7-1 NAL unit type codes
    NalUnitType_Unspecified = 0,      // Unspecified
    NalUnitType_CodedSliceNonIdr = 1, // Coded slice of a non-IDR picture
    NalUnitType_CodedSliceDataPartitionA = 2, // Coded slice data partition A
    NalUnitType_CodedSliceDataPartitionB = 3, // Coded slice data partition B
    NalUnitType_CodedSliceDataPartitionC = 4, // Coded slice data partition C
    NalUnitType_CodedSliceIdr = 5,            // Coded slice of an IDR picture
    NalUnitType_SEI = 6, // Supplemental enhancement information (SEI)
    NalUnitType_SPS = 7, // Sequence parameter set
    NalUnitType_PPS = 8, // Picture parameter set
    NalUnitType_AUD = 9, // Access unit delimiter
    NalUnitType_EndOfSequence = 10, // End of sequence
    NalUnitType_EndOfStream = 11,   // End of stream
    NalUnitType_Filler = 12,        // Filler data
    NalUnitType_SpsExt = 13,        // Sequence parameter set extension
                                    // 14..18           // Reserved
    NalUnitType_CodedSliceAux =
        19, // Coded slice of an auxiliary coded picture without partitioning
    // 20..23           // Reserved
    // 24..31           // Unspecified
    NalUnitType_VPS_HEVC = 32,
    NalUnitType_SPS_HEVC = 33,
    NalUnitType_PPS_HEVC = 34,
    NalUnitType_AUD_HEVC = 35,
    NalUnitType_EndOfSequence_HEVC = 36,
    NalUnitType_EndOfStream_HEVC = 37,
    NalUnitType_Filler_HEVC = 38,
    NalUnitType_SEI_HEVC = 39,
    NalUnitType_SEI_HEVC_2 = 40,
};

static inline unsigned int nal_find_startcode(const unsigned char *buf,
    unsigned int off, unsigned int len)
{
    for (; off + 2 < len; off++) {
        if (buf[off] == 0 && buf[off + 1] == 0) {
            if (buf[off + 2] == 1)
                return off;
            if (off + 3 < len && buf[off + 2] == 0 && buf[off + 3] == 1)
                return off;
        }
    }
    return len;
}

/* Splits an Annex B buffer into NAL units, one per call.
 *
 * Stateful iterator: on each successful call, sets *nalptr to the start of
 * the next NALU's payload (past its start code) and *p_len to its length.
 * Returns 0 until the buffer is exhausted, -1 then.
 *
 * Handles both 3-byte (0x000001, used by SigmaStar/MStar hardware) and
 * 4-byte (0x00000001) start codes.
 *
 * Trailing zero bytes between NALUs (alignment padding) are kept as part
 * of the preceding NALU: they are legal EBSP trailing bytes, and keeping
 * them is what lets the iterator advance to the next start code.  Trimming
 * them strands the next call inside the padding, which yields a zero-length
 * NALU and loops forever.
 */
static inline int nal_split(unsigned char *buf, unsigned char **nalptr,
                            size_t *p_len, size_t max_len)
{
    size_t pos = (size_t)(*nalptr - buf) + *p_len;

    for (;;) {
        uint32_t sc = nal_find_startcode((const uint8_t *)buf, (uint32_t)pos,
            (uint32_t)max_len);
        if (sc >= (uint32_t)max_len) {
            if (pos >= max_len)
                return -1;
            /* Last segment runs to end of buffer */
            *nalptr = buf + pos;
            *p_len  = max_len - pos;
            return 0;
        }

        if (pos < sc) {
            /* NALU payload from pos up to the start code */
            *nalptr = buf + pos;
            *p_len  = sc - pos;
            return 0;
        }

        /* pos sits on a (possibly misaligned) start code: skip it.  When
         * 3-byte codes are preceded by padding, nal_find_startcode() can
         * report a 4-byte match one byte early; skipping sc_len from that
         * match still lands on the payload, so boundaries stay correct. */
        pos = sc + ((buf[sc + 2] == 1) ? 3u : 4u);
    }
}

struct NAL {
    char isH265;
    char *data;
    uint64_t data_size;
    uint32_t picture_order_count;

    // NAL header
    bool forbidden_zero_bit;
    uint8_t ref_idc;
    uint8_t unit_type_value;
    enum NalUnitType unit_type;
};
