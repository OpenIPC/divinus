#include "record.h"

/* Guards the recording state: the encoder thread feeds fragments while the
 * server thread starts and stops sessions. */
static pthread_mutex_t recordMtx = PTHREAD_MUTEX_INITIALIZER;

static FILE *recordFile;
static struct Mp4State recordState;
static unsigned long long recordSize;
static time_t recordStartTime;
static time_t recordLastFlush;
static bool recordActive;
static char recordPath[RECORD_PATH_MAX];

static bool record_ensure_dir(const char *path) {
    char tmp[RECORD_PATH_MAX];
    size_t len = strlen(path);

    if (len == 0 || len >= sizeof(tmp)) return false;
    memcpy(tmp, path, len + 1);
    if (tmp[len - 1] == '/') tmp[len - 1] = '\0';

    for (char *p = tmp + 1; *p; p++) {
        if (*p != '/') continue;
        *p = '\0';
        if (mkdir(tmp, 0755) && errno != EEXIST) {
            HAL_DANGER("record", "Cannot create directory '%s': %s\n", tmp, strerror(errno));
            return false;
        }
        *p = '/';
    }

    if (mkdir(tmp, 0755) && errno != EEXIST) {
        HAL_DANGER("record", "Cannot create directory '%s': %s\n", tmp, strerror(errno));
        return false;
    }
    return true;
}

/* Builds "<path>/<stem>_<timestamp><ext>", appending a suffix on collision. */
static bool record_build_path(char *out, size_t outSize) {
    char dir[sizeof(app_config.record_path)];
    char stem[sizeof(app_config.record_filename)];
    char ext[16] = ".mp4";
    char stamp[32];
    struct tm tm_buf, *tm_info;
    time_t now = recordStartTime ? recordStartTime : time(NULL);

    strncpy(dir, app_config.record_path, sizeof(dir) - 1);
    dir[sizeof(dir) - 1] = '\0';
    if (EMPTY(dir)) {
        HAL_DANGER("record", "Destination path is not set!\n");
        return false;
    }

    if (!EMPTY(app_config.record_filename)) {
        char *dot;
        const char *name = strrchr(app_config.record_filename, '/');
        name = name ? name + 1 : app_config.record_filename;

        strncpy(stem, name, sizeof(stem) - 1);
        stem[sizeof(stem) - 1] = '\0';

        dot = strrchr(stem, '.');
        if (dot && dot != stem) {
            strncpy(ext, dot, sizeof(ext) - 1);
            ext[sizeof(ext) - 1] = '\0';
            *dot = '\0';
        }
    } else
        strncpy(stem, "recording", sizeof(stem) - 1);

    if (EMPTY(stem)) strncpy(stem, "recording", sizeof(stem) - 1);

    tm_info = localtime_r(&now, &tm_buf);
    if (!tm_info || !strftime(stamp, sizeof(stamp), "%Y%m%d-%H%M%S", tm_info))
        strncpy(stamp, "unknown", sizeof(stamp) - 1);

    snprintf(out, outSize, "%s/%s_%s%s", dir, stem, stamp, ext);
    if (access(out, F_OK)) return true;

    for (unsigned int i = 1; i < 1000; i++) {
        snprintf(out, outSize, "%s/%s_%s-%u%s", dir, stem, stamp, i, ext);
        if (access(out, F_OK)) return true;
    }

    HAL_DANGER("record", "Could not find a free name for a '%s' segment!\n", stem);
    return false;
}

static void record_close_locked(void) {
    if (!recordFile) return;

    if (fflush(recordFile))
        HAL_DANGER("record", "Flushing '%s' failed: %s\n", recordPath, strerror(errno));
    if (fclose(recordFile))
        HAL_DANGER("record", "Closing '%s' failed: %s\n", recordPath, strerror(errno));
    else
        HAL_INFO("record", "Finalized segment '%s' (%llu bytes)\n", recordPath, recordSize);

    recordFile = NULL;
    recordSize = 0;
    recordState.header_sent = false;
}

static bool record_open_locked(void) {
    if (recordFile) return true;

    if (EMPTY(app_config.record_path)) {
        HAL_DANGER("record", "Destination path is not set!\n");
        return false;
    }
    if (!record_ensure_dir(app_config.record_path)) return false;
    if (!record_build_path(recordPath, sizeof(recordPath))) return false;

    if (!(recordFile = fopen(recordPath, "wb"))) {
        HAL_DANGER("record", "Failed to open '%s': %s\n", recordPath, strerror(errno));
        return false;
    }
    setvbuf(recordFile, NULL, _IOFBF, 64 * 1024);

    recordSize = 0;
    recordState.header_sent = false;
    recordState.sequence_number = 0;
    recordState.base_data_offset = 0;
    recordState.base_media_decode_time = 0;
    recordState.nals_count = 0;
    recordState.default_sample_duration = default_sample_size;
    recordLastFlush = recordStartTime;

    HAL_INFO("record", "Recording to '%s'\n", recordPath);
    return true;
}

static bool record_rotate_locked(void) {
    record_close_locked();
    recordStartTime = time(NULL);
    return record_open_locked();
}

static bool record_write_locked(const void *data, size_t len) {
    if (len && fwrite(data, 1, len, recordFile) != len) {
        HAL_DANGER("record", "Writing to '%s' failed: %s\n", recordPath, strerror(errno));
        return false;
    }
    return true;
}

/* Guards against NALU offsets and lengths that would read past the pack or
 * underflow "length - scLen". */
static void record_feed_nalu(unsigned char *data, unsigned int len,
    const hal_vidnalu *nalu, char isH265) {
    unsigned int offset = nalu->offset;
    unsigned int nalu_len = nalu->length;
    unsigned int scLen;
    const char *payload;
    unsigned int payload_len;

    if (offset > len || nalu_len > len - offset) return;
    if (nalu_len <= 3 || offset + 3 > len) return;

    scLen = (data[offset + 2] == 1) ? 3 : 4;
    if (nalu_len <= scLen) return;

    payload = (const char *)data + offset + scLen;
    payload_len = nalu_len - scLen;

    switch (nalu->type) {
        case NalUnitType_SPS:
        case NalUnitType_SPS_HEVC:
            if (payload_len <= UINT16_MAX) mp4_set_sps(payload, payload_len, isH265);
            break;
        case NalUnitType_PPS:
        case NalUnitType_PPS_HEVC:
            if (payload_len <= UINT16_MAX) mp4_set_pps(payload, payload_len, isH265);
            break;
        case NalUnitType_VPS_HEVC:
            if (payload_len <= UINT16_MAX) mp4_set_vps(payload, payload_len);
            break;
        case NalUnitType_CodedSliceIdr:
        case NalUnitType_CodedSliceAux:
            mp4_set_slice(payload, payload_len, 1);
            break;
        case NalUnitType_CodedSliceNonIdr:
            mp4_set_slice(payload, payload_len, 0);
            break;
        default:
            break;
    }
}

static bool record_emit_fragment_locked(void) {
    struct BitBuf buf;

    if (!recordState.header_sent) {
        mp4_get_header(&buf);
        /* No moov until the parameter sets arrive; nothing decodable to keep. */
        if (buf.offset == 0) return true;

        if (!record_write_locked(buf.buf, buf.offset)) return false;
        recordSize += buf.offset;

        recordState.sequence_number = 0;
        recordState.base_data_offset = buf.offset;
        recordState.base_media_decode_time = 0;
        recordState.nals_count = 0;
        recordState.default_sample_duration = default_sample_size;
        recordState.header_sent = true;
    }

    if (mp4_set_state(&recordState) != BUF_OK) return false;

    mp4_get_moof(&buf);
    if (!record_write_locked(buf.buf, buf.offset)) return false;
    recordSize += buf.offset;

    mp4_get_mdat(&buf);
    if (!record_write_locked(buf.buf, buf.offset)) return false;
    recordSize += buf.offset;

    return true;
}

static bool record_should_rotate_locked(void) {
    time_t now;

    if (app_config.record_segment_size > 0 &&
        recordSize >= (unsigned long long)app_config.record_segment_size)
        return true;

    if (app_config.record_segment_duration <= 0 || !recordStartTime) return false;

    now = time(NULL);
    if (now == (time_t)-1) return false;
    return now - recordStartTime >= app_config.record_segment_duration;
}

static void record_flush_locked(void) {
    time_t now;

    if (!recordFile) return;

    now = time(NULL);
    if (now == (time_t)-1 || now == recordLastFlush) return;
    recordLastFlush = now;

    if (fflush(recordFile))
        HAL_DANGER("record", "Flushing '%s' failed: %s\n", recordPath, strerror(errno));
}

/**
 * Starts a new recording session
 * @return true once recording, false if the output could not be opened
 */
bool record_start(void) {
    pthread_mutex_lock(&recordMtx);

    if (recordActive) {
        pthread_mutex_unlock(&recordMtx);
        return true;
    }

    recordStartTime = time(NULL);
    if (recordStartTime == (time_t)-1) recordStartTime = 0;

    if (!record_open_locked()) {
        pthread_mutex_unlock(&recordMtx);
        return false;
    }

    recordActive = true;
    pthread_mutex_unlock(&recordMtx);
    return true;
}

/**
 * Stops the current recording session and finalizes its file
 */
void record_stop(void) {
    pthread_mutex_lock(&recordMtx);

    record_close_locked();
    recordActive = false;
    recordStartTime = 0;

    pthread_mutex_unlock(&recordMtx);
}

/**
 * Reports whether a recording session is running
 * @return true if recording, false otherwise
 */
bool record_active(void) {
    bool active;

    pthread_mutex_lock(&recordMtx);
    active = recordActive;
    pthread_mutex_unlock(&recordMtx);

    return active;
}

/**
 * Reports when the current recording session started
 * @return the start time, or 0 when not recording
 */
time_t record_start_time(void) {
    time_t start;

    pthread_mutex_lock(&recordMtx);
    start = recordActive ? recordStartTime : 0;
    pthread_mutex_unlock(&recordMtx);

    return start;
}

/**
 * Ingests a video stream into the current recording
 * @param stream Pointer to the hal_vidstream carrying the NALU packs
 * @param isH265 1 if H.265 (HEVC), 0 for H.264 (AVC)
 */
void record_ingest_stream(hal_vidstream *stream, char isH265) {
    if (!stream) return;

    pthread_mutex_lock(&recordMtx);
    if (!recordActive || !recordFile) {
        pthread_mutex_unlock(&recordMtx);
        return;
    }

    for (unsigned int i = 0; i < stream->count; ++i) {
        hal_vidpack *pack = &stream->pack[i];
        unsigned char *pack_data;
        unsigned int pack_len;

        if (pack->length <= pack->offset) continue;
        pack_len = pack->length - pack->offset;
        pack_data = pack->data + pack->offset;

        for (int j = 0; j < pack->naluCnt; j++)
            record_feed_nalu(pack_data, pack_len, &pack->nalu[j], isH265);

        if (!record_emit_fragment_locked()) {
            HAL_DANGER("record", "Stopping recording after a write failure!\n");
            record_close_locked();
            recordActive = false;
            recordStartTime = 0;
            pthread_mutex_unlock(&recordMtx);
            return;
        }

        /* Rotate between fragments so a segment never splits a moof from its
         * mdat, and the next one starts with a fresh header. */
        if (record_should_rotate_locked() && !record_rotate_locked()) {
            recordActive = false;
            recordStartTime = 0;
            pthread_mutex_unlock(&recordMtx);
            return;
        }
    }

    record_flush_locked();
    pthread_mutex_unlock(&recordMtx);
}
