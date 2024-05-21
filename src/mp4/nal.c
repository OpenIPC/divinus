#include "nal.h"

char *nal_type_to_str(const enum NalUnitType nal_type) {
    switch (nal_type) {
    case NalUnitType_Unspecified:
        return "Unspecified";
    case NalUnitType_CodedSliceNonIdr:
        return "CodedSliceNonIdr";
    case NalUnitType_CodedSliceDataPartitionA:
        return "CodedSliceDataPartitionA";
    case NalUnitType_CodedSliceDataPartitionB:
        return "CodedSliceDataPartitionB";
    case NalUnitType_CodedSliceDataPartitionC:
        return "CodedSliceDataPartitionC";
    case NalUnitType_CodedSliceIdr:
        return "CodedSliceIdr";
    case NalUnitType_SEI:
    case NalUnitType_SEI_HEVC:
    case NalUnitType_SEI_HEVC_2:
        return "SEI";
    case NalUnitType_SPS:
    case NalUnitType_SPS_HEVC:
        return "SPS";
    case NalUnitType_PPS:
    case NalUnitType_PPS_HEVC:
        return "PPS";
    case NalUnitType_VPS_HEVC:
        return "VPS";
    case NalUnitType_AUD:
    case NalUnitType_AUD_HEVC:
        return "AUD";
    case NalUnitType_EndOfSequence:
    case NalUnitType_EndOfSequence_HEVC:
        return "EndOfSequence";
    case NalUnitType_EndOfStream:
    case NalUnitType_EndOfStream_HEVC:
        return "EndOfStream";
    case NalUnitType_Filler:
    case NalUnitType_Filler_HEVC:
        return "Filler";
    case NalUnitType_SpsExt:
        return "SpsExt";
    case NalUnitType_CodedSliceAux:
        return "CodedSliceAux (AVC) / CodedSliceIdr (HEVC)";
    default:
        return "Unknown";
    }
}

void nal_parse_header(struct NAL *nal, const char first_byte) {
    nal->forbidden_zero_bit = nal->isH265 ? (first_byte & 0b00000001) : (((first_byte & 0b10000000) >> 7) == 1);
    nal->ref_idc = nal->isH265 ? 0 : ((first_byte & 0b01100000) >> 5);
    nal->unit_type = nal->isH265 ? ((first_byte & 0b01111110) >> 1) : (first_byte & 0b00011111);
}

bool nal_chk4(const char *buf, const unsigned int offset) {
    if (buf[offset] == 0x00 && buf[offset + 1] == 0x00 &&
        buf[offset + 2] == 0x01) {
        return true;
    }
    if (buf[offset] == 0x00 && buf[offset + 1] == 0x00 &&
        buf[offset + 2] == 0x00 && buf[offset + 3] == 0x01) {
        return true;
    }
    return false;
}

bool nal_chk3(const char *buf, const unsigned int offset) {
    if (buf[offset] == 0x00 && buf[offset + 1] == 0x00 &&
        buf[offset + 2] == 0x01) {
        return true;
    }
    return false;
}