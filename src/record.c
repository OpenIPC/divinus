#include "record.h"

/* Guards the recording state: the encoder thread feeds fragments while the
 * server thread starts and stops sessions. */
static pthread_mutex_t recordMtx = PTHREAD_MUTEX_INITIALIZER;
/* Serializes starting and stopping with their storage remounts, which run
 * outside recordMtx */
static pthread_mutex_t recordModeMtx = PTHREAD_MUTEX_INITIALIZER;
static bool recordStorageRw;

static FILE *recordFile;
static struct Mp4State recordState;
static unsigned long long recordSize;
static time_t recordStartTime;
static time_t recordLastFlush;
static bool recordActive;
static bool recordWaitKey;
static time_t recordLastSync, recordLastRoom, recordLastTry;
static char recordPath[RECORD_PATH_MAX];
/* Monotonic second at which a timed recording ends, 0 for none */
static time_t recordStopAt;

#define RECORD_CARD_ROOT "/mnt/sdcard"
#define RECORD_CARD_TOOL "/usr/sbin/sdcard"

static time_t record_now(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return ts.tv_sec;
}

/* The card is kept read-only unless a recording is running; the helper
 * (re)mounts it in the requested mode. Must not be called with recordMtx held. */
static void record_storage_mode(const char *mode) {
    char cmd[96];

    if (strncmp(app_config.record_path, RECORD_CARD_ROOT, strlen(RECORD_CARD_ROOT))) return;
    if (access(RECORD_CARD_TOOL, X_OK)) return;
    snprintf(cmd, sizeof(cmd), RECORD_CARD_TOOL " mount %s >/dev/null 2>&1", mode);
    if (system(cmd)) HAL_WARNING("record", "Storage could not be switched to '%s'\n", mode);
    recordStorageRw = !strcmp(mode, "rw");
}

/* Recordings must land on mounted storage, never on the root filesystem:
 * with no card in the slot the path would be a plain directory in flash. */
static bool record_on_storage(struct statvfs *vfs) {
    char tmp[RECORD_PATH_MAX];
    struct stat st, root;
    size_t len = strlen(app_config.record_path);

    if (len == 0 || len >= sizeof(tmp)) return false;
    memcpy(tmp, app_config.record_path, len + 1);
    while (stat(tmp, &st)) {
        char *sep = strrchr(tmp, '/');
        if (!sep || sep == tmp) return false;
        *sep = '\0';
    }
    if (stat("/", &root) || st.st_dev == root.st_dev) return false;
    if (vfs && statvfs(tmp, vfs)) return false;
    return true;
}

/* Splits the configured file name into "<stem>" and ".<ext>". */
static void record_name_parts(char *stem, size_t stemSize, char *ext, size_t extSize) {
    strncpy(ext, ".mp4", extSize - 1);
    ext[extSize - 1] = '\0';
    stem[0] = '\0';

    if (!EMPTY(app_config.record_filename)) {
        char *dot;
        const char *name = strrchr(app_config.record_filename, '/');
        name = name ? name + 1 : app_config.record_filename;

        strncpy(stem, name, stemSize - 1);
        stem[stemSize - 1] = '\0';

        dot = strrchr(stem, '.');
        if (dot && dot != stem) {
            strncpy(ext, dot, extSize - 1);
            ext[extSize - 1] = '\0';
            *dot = '\0';
        }
    }
    if (EMPTY(stem)) strncpy(stem, "recording", stemSize - 1);
}

/* Whether a name is "<stem>_YYYYMMDD-HHMMSS[-N]<ext>", as record_build_path
 * makes them. */
static bool record_own_name(const char *name, const char *stem, size_t stemLen,
    const char *ext, size_t extLen) {
    size_t len = strlen(name);
    const char *p = name + stemLen + 1, *end = name + len - extLen;

    if (len < stemLen + 1 + 15 + extLen || strncmp(name, stem, stemLen) ||
        name[stemLen] != '_' || strcmp(end, ext)) return false;
    for (int i = 0; i < 15; i++, p++)
        if (i == 8 ? *p != '-' : (*p < '0' || *p > '9')) return false;
    if (p == end) return true;
    if (*p++ != '-' || p == end) return false;
    for (; p < end; p++)
        if (*p < '0' || *p > '9') return false;
    return true;
}

/* Loop recording: while free space is below the reserve, removes the oldest
 * segment. Only files this recorder named itself are ever touched. Runs
 * without recordMtx, so the encoder thread never waits for the card. */
static void record_make_room(const char *current) {
    char stem[sizeof(app_config.record_filename)], ext[16];
    size_t stemLen, extLen;

    record_name_parts(stem, sizeof(stem), ext, sizeof(ext));
    stemLen = strlen(stem);
    extLen = strlen(ext);

    for (int pass = 0; pass < 256; pass++) {
        struct statvfs vfs;
        struct dirent *ent;
        char oldest[256] = "", path[RECORD_PATH_MAX];
        DIR *dir;

        if (statvfs(app_config.record_path, &vfs)) return;
        if ((unsigned long long)vfs.f_bavail * vfs.f_frsize >=
            (unsigned long long)app_config.record_reserve << 20) return;

        if (!(dir = opendir(app_config.record_path))) return;
        while ((ent = readdir(dir))) {
            if (strlen(ent->d_name) >= sizeof(oldest)) continue;
            if (!record_own_name(ent->d_name, stem, stemLen, ext, extLen)) continue;
            if (!strcmp(ent->d_name, current)) continue;
            /* Names carry the start time, so the smallest one is the oldest */
            if (EMPTY(oldest) || strcmp(ent->d_name, oldest) < 0) strcpy(oldest, ent->d_name);
        }
        closedir(dir);

        if (EMPTY(oldest)) {
            HAL_WARNING("record", "Storage is low and there is no old segment left to remove\n");
            return;
        }
        snprintf(path, sizeof(path), "%s/%s", app_config.record_path, oldest);
        if (unlink(path)) {
            HAL_DANGER("record", "Cannot remove '%s': %s\n", path, strerror(errno));
            return;
        }
        HAL_INFO("record", "Removed oldest segment '%s' to free space\n", path);
    }
}

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
    char ext[16];
    char stamp[32];
    struct tm tm_buf, *tm_info;
    time_t now = recordStartTime ? recordStartTime : time(NULL);

    strncpy(dir, app_config.record_path, sizeof(dir) - 1);
    dir[sizeof(dir) - 1] = '\0';
    if (EMPTY(dir)) {
        HAL_DANGER("record", "Destination path is not set!\n");
        return false;
    }

    record_name_parts(stem, sizeof(stem), ext, sizeof(ext));

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
    if (!record_on_storage(NULL)) {
        HAL_DANGER("record", "No storage mounted for '%s'\n", app_config.record_path);
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
    recordState.audio_media_decode_time = 0;
    recordState.nals_count = 0;
    recordState.default_sample_duration = default_sample_size;
    recordLastFlush = recordLastSync = recordLastRoom = recordStartTime;
    recordWaitKey = true;

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

static bool record_emit_fragment_locked(void) {
    struct BitBuf buf;

    pthread_mutex_lock(&mp4_mtx);

    if (!recordState.header_sent) {
        mp4_get_header(&buf);
        /* No moov until the parameter sets arrive; nothing decodable to keep. */
        if (buf.offset == 0) {
            pthread_mutex_unlock(&mp4_mtx);
            return true;
        }

        if (!record_write_locked(buf.buf, buf.offset)) {
            pthread_mutex_unlock(&mp4_mtx);
            return false;
        }
        recordSize += buf.offset;

        recordState.sequence_number = 0;
        recordState.base_data_offset = buf.offset;
        recordState.base_media_decode_time = 0;
        recordState.audio_media_decode_time = 0;
        recordState.nals_count = 0;
        recordState.default_sample_duration = default_sample_size;
        recordState.header_sent = true;
    }

    if (mp4_set_state(&recordState) != BUF_OK) {
        pthread_mutex_unlock(&mp4_mtx);
        return false;
    }

    if (mp4_get_moof(&buf) != BUF_OK) {
        pthread_mutex_unlock(&mp4_mtx);
        return false;
    }
    if (!record_write_locked(buf.buf, buf.offset)) {
        pthread_mutex_unlock(&mp4_mtx);
        return false;
    }
    recordSize += buf.offset;

    if (mp4_get_mdat(&buf) != BUF_OK) {
        pthread_mutex_unlock(&mp4_mtx);
        return false;
    }
    if (!record_write_locked(buf.buf, buf.offset)) {
        pthread_mutex_unlock(&mp4_mtx);
        return false;
    }
    recordSize += buf.offset;

    pthread_mutex_unlock(&mp4_mtx);
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

    /* Keep what a power cut can lose down to a few seconds */
    if (now - recordLastSync >= 10) {
        recordLastSync = now;
        fdatasync(fileno(recordFile));
    }
}

/**
 * Starts a new recording session
 * @return true once recording, false if the output could not be opened
 */
bool record_start(void) {
    bool ok = true;

    if (record_active()) return true;

    pthread_mutex_lock(&recordModeMtx);
    record_storage_mode("rw");
    if (record_on_storage(NULL)) record_make_room("");

    pthread_mutex_lock(&recordMtx);
    recordLastTry = time(NULL);

    if (!recordActive) {
        recordStartTime = time(NULL);
        if (recordStartTime == (time_t)-1) recordStartTime = 0;
        if (record_open_locked()) recordActive = true;
        else ok = false;
    }
    pthread_mutex_unlock(&recordMtx);

    /* Nothing is being written: the card goes back to read-only */
    if (!ok) record_storage_mode("ro");
    pthread_mutex_unlock(&recordModeMtx);
    return ok;
}

/**
 * Stops the current recording session and finalizes its file
 */
void record_stop(void) {
    pthread_mutex_lock(&recordModeMtx);
    pthread_mutex_lock(&recordMtx);

    record_close_locked();
    recordActive = false;
    recordStartTime = 0;
    recordStopAt = 0;

    pthread_mutex_unlock(&recordMtx);

    record_storage_mode("ro");
    pthread_mutex_unlock(&recordModeMtx);
}

/**
 * Ends the current recording after the given number of seconds; calling it
 * again moves the end, 0 cancels it. Continuous recording ignores it.
 */
void record_stop_after(unsigned int seconds) {
    pthread_mutex_lock(&recordMtx);
    recordStopAt = seconds ? record_now() + seconds : 0;
    pthread_mutex_unlock(&recordMtx);
}

/**
 * Seconds left of a timed recording, 0 when none is set
 */
unsigned int record_stop_in(void) {
    time_t left;

    pthread_mutex_lock(&recordMtx);
    left = recordStopAt ? recordStopAt - record_now() : 0;
    pthread_mutex_unlock(&recordMtx);
    return left > 0 ? left : 0;
}

/**
 * Keeps continuous recording alive: retries when the card was missing at
 * start or a write failed (card pulled). Called once a second.
 */
void record_tick(void) {
    time_t now = time(NULL), stopAt;
    bool active = record_active();

    /* A write or rotation failure on the encoder thread ends the recording
     * there; the card goes back to read-only from here. */
    if (!active && recordStorageRw) {
        pthread_mutex_lock(&recordModeMtx);
        if (!record_active() && recordStorageRw) record_storage_mode("ro");
        pthread_mutex_unlock(&recordModeMtx);
    }

    /* Free space is checked here rather than on the encoder thread */
    if (active && now - recordLastRoom >= 30) {
        char current[RECORD_PATH_MAX] = "";
        const char *name;
        recordLastRoom = now;
        pthread_mutex_lock(&recordMtx);
        name = strrchr(recordPath, '/');
        if (recordFile && name) strncpy(current, name + 1, sizeof(current) - 1);
        pthread_mutex_unlock(&recordMtx);
        record_make_room(current);
    }

    pthread_mutex_lock(&recordMtx);
    stopAt = recordStopAt;
    pthread_mutex_unlock(&recordMtx);
    if (stopAt && record_now() >= stopAt) {
        if (app_config.record_continuous) {
            pthread_mutex_lock(&recordMtx);
            recordStopAt = 0;
            pthread_mutex_unlock(&recordMtx);
        } else record_stop();
    }

    if (!app_config.record_enable || !app_config.record_continuous) return;
    if (record_active()) return;
    if (now - recordLastTry < 15) return;
    record_start();
}

/**
 * Describes the storage behind the recording path
 * @param out Receives a short human readable summary
 */
void record_storage(char *out, size_t outSize) {
    struct statvfs vfs;

    if (!record_on_storage(&vfs)) {
        snprintf(out, outSize, "no card mounted");
        return;
    }
    snprintf(out, outSize, "%.1f GB, %.1f GB free, %s",
        (double)vfs.f_blocks * vfs.f_frsize / 1e9,
        (double)vfs.f_bavail * vfs.f_frsize / 1e9,
        (vfs.f_flag & ST_RDONLY) ? "read-only" : "writable");
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
 * Appends the fragment the muxer has just produced to the recording.
 * Called with the muxer lock held, right after mp4_set_slice().
 * @param isKey Whether the fragment starts with a keyframe
 */
void record_fragment(char isKey) {
    pthread_mutex_lock(&recordMtx);
    if (!recordActive || !recordFile) {
        pthread_mutex_unlock(&recordMtx);
        return;
    }

    /* Segments are cut on keyframes so each file plays from its first frame. */
    if (isKey && record_should_rotate_locked() && !record_rotate_locked()) {
        recordActive = false;
        recordStartTime = 0;
        pthread_mutex_unlock(&recordMtx);
        return;
    }
    if (recordWaitKey) {
        if (!isKey) {
            pthread_mutex_unlock(&recordMtx);
            return;
        }
        recordWaitKey = false;
    }

    if (!record_emit_fragment_locked()) {
        HAL_DANGER("record", "Stopping recording after a write failure!\n");
        record_close_locked();
        recordActive = false;
        recordStartTime = 0;
        recordLastTry = time(NULL);
        pthread_mutex_unlock(&recordMtx);
        return;
    }

    record_flush_locked();
    pthread_mutex_unlock(&recordMtx);
}
