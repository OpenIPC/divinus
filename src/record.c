#include "record.h"

static FILE *recordFile;
static struct Mp4State recordState;
static int recordSize;
time_t recordStartTime = 0;
char recordOn = 0, recordPath[256];

static void record_check_segment_size(int upcoming) {
    if (app_config.record_segment_size <= 0) return;
    if (recordSize + upcoming >= app_config.record_segment_size) {
        record_stop();
        record_start();
    }
}

static void record_check_segment_duration() {
    if (app_config.record_segment_duration <= 0) return;

    time_t currentTime = time(NULL);
    if (currentTime == (time_t)-1 || recordStartTime == (time_t)-1) return;

    if (currentTime - recordStartTime >= app_config.record_segment_duration) {
        record_stop();
        record_start();
    }
}

void record_start(void) {
    if (recordOn) return;

    if (recordFile) {
        HAL_DANGER("record", "Output file needs to be closed before initializing a new one.\n");
        return;
    }

    recordSize = 0;
    recordState.header_sent = false;
    recordStartTime = time(NULL);

    if (EMPTY(app_config.record_path)) {
        HAL_DANGER("record", "Destination path is not set!\n");
        return;
    }

    strcpy(recordPath, app_config.record_path);
    if (recordPath[strlen(recordPath) - 1] != '/')
        strncat(recordPath, "/", sizeof(recordPath) - strlen(recordPath) - 1);

    if (!EMPTY(app_config.record_filename)) {
        strncpy(recordPath, app_config.record_filename, sizeof(recordPath) - 1);
        recordPath[sizeof(recordPath) - 1] = '\0';
    } else {
        char tempName[160];
        struct tm tm_buf, *tm_info = localtime_r(&recordStartTime, &tm_buf);
        sprintf(tempName, "recording_%s.mp4", timefmt);
        strftime(recordPath, sizeof(recordPath), tempName, tm_info);
    }

    if (!(recordFile = fopen(recordPath, "wb"))) {
        HAL_DANGER("record", "Failed to open the destination file!\n");
        return;
    }

    recordOn = 1;
}

void record_stop(void) {
    if (!recordOn) return;

    if (!recordFile) {
        HAL_DANGER("record", "No output file is opened and ready to finalize!\n");
        return;
    }

    fclose(recordFile);
    recordFile = NULL;

    recordOn = 0;
    recordStartTime = 0;
}

void send_mp4_to_record(hal_vidstream *stream, char isH265) {
    if (!recordOn) return;

    if (!recordFile) {
        HAL_DANGER("record", "No output file is opened for writing data!\n");
        return;
    }

    for (unsigned int i = 0; i < stream->count; ++i) {
        hal_vidpack *pack = &stream->pack[i];
        unsigned int pack_len = pack->length - pack->offset;
        unsigned char *pack_data = pack->data + pack->offset;

        for (char j = 0; j < pack->naluCnt; j++) {
            if ((pack->nalu[j].type == NalUnitType_SPS || pack->nalu[j].type == NalUnitType_SPS_HEVC) 
                && pack->nalu[j].length >= 4 && pack->nalu[j].length <= UINT16_MAX)
                mp4_set_sps(pack_data + pack->nalu[j].offset + 4, pack->nalu[j].length - 4, isH265);
            else if ((pack->nalu[j].type == NalUnitType_PPS || pack->nalu[j].type == NalUnitType_PPS_HEVC)
                && pack->nalu[j].length <= UINT16_MAX)
                mp4_set_pps(pack_data + pack->nalu[j].offset + 4, pack->nalu[j].length - 4, isH265);
            else if (pack->nalu[j].type == NalUnitType_VPS_HEVC && pack->nalu[j].length <= UINT16_MAX)
                mp4_set_vps(pack_data + pack->nalu[j].offset + 4, pack->nalu[j].length - 4);
            else if (pack->nalu[j].type == NalUnitType_CodedSliceIdr || pack->nalu[j].type == NalUnitType_CodedSliceAux)
                mp4_set_slice(pack_data + pack->nalu[j].offset + 4, pack->nalu[j].length - 4, 1);
            else if (pack->nalu[j].type == NalUnitType_CodedSliceNonIdr)
                mp4_set_slice(pack_data + pack->nalu[j].offset + 4, pack->nalu[j].length - 4, 0);
        }

        static enum BufError err;
        static char len_buf[50];
        if (!recordState.header_sent) {
            struct BitBuf header_buf;
            err = mp4_get_header(&header_buf); chk_err_continue
            record_check_segment_size(header_buf.offset);
            recordSize += header_buf.offset;
            fwrite(header_buf.buf, 1, header_buf.offset, recordFile);

            recordState.sequence_number = 0;
            recordState.base_data_offset = header_buf.offset;
            recordState.base_media_decode_time = 0;
            recordState.header_sent = true;
            recordState.nals_count = 0;
            recordState.default_sample_duration =
                default_sample_size;
        }

        err = mp4_set_state(&recordState); chk_err_continue
        {
            struct BitBuf moof_buf;
            err = mp4_get_moof(&moof_buf); chk_err_continue
            record_check_segment_size(moof_buf.offset);
            recordSize += moof_buf.offset;
            fwrite(moof_buf.buf, 1, moof_buf.offset, recordFile);
        }
        {
            struct BitBuf mdat_buf;
            err = mp4_get_mdat(&mdat_buf); chk_err_continue
            record_check_segment_size(mdat_buf.offset);
            recordSize += mdat_buf.offset;
            fwrite(mdat_buf.buf, 1, mdat_buf.offset, recordFile);
            
        }
    }

    record_check_segment_duration();
}