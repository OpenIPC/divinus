#if defined(__arm__)

#include "s3l_hal.h"

#include <math.h>
#include <pthread.h>
#include <stdio.h>

#define S3L_CFA_REC   12672
#define S3L_STAT_DATA 0x82

struct s3l_aaa_frame {
    unsigned int n_tiles;
    float gr[1024], gb[1024];
};

static inline unsigned int rd16(const unsigned char *p) { return p[0] | p[1] << 8; }

static void s3l_aaa_parse(const unsigned char *cfa, struct s3l_aaa_frame *f)
{
    f->n_tiles = 0;
    unsigned int slices = cfa[0x36] ? cfa[0x36] : 1;
    if (slices > 2) slices = 2;
    for (unsigned int k = 0; k < slices; k++) {
        const unsigned char *c = cfa + k * S3L_CFA_REC;
        unsigned int awb_n = c[0x30] * c[0x31];
        if (awb_n > 1024) continue;
        double awb_px = (double)rd16(c + 0x04) * rd16(c + 0x06);
        unsigned int awb_sh = rd16(c + 0x0c);
        if (awb_sh > 15) continue;
        for (unsigned int i = 0; i < awb_n; i++) {
            const unsigned char *t = c + S3L_STAT_DATA + 10 * i;
            if (rd16(t + 8)) continue;
            double r = (double)(rd16(t) << awb_sh) / awb_px, g = (double)(rd16(t + 2) << awb_sh) / awb_px,
                   b = (double)(rd16(t + 4) << awb_sh) / awb_px;
            if (g > 20 && r > 1 && b > 1 && f->n_tiles < 1024) {
                double q = g / r, p = g / b;
                if (q > 0.8 && q < 4 && p > 0.8 && p < 4) { f->gr[f->n_tiles] = q; f->gb[f->n_tiles] = p; f->n_tiles++; }
            }
        }
    }
}

int s3l_aaa_config(int fd)
{
    struct __attribute__((packed)) {
        uint32_t cmd_code, flags, fifo[4];
        uint16_t awb[10], ae[8], af[8];
    } c;
    memset(&c, 0, sizeof(c));
    c.flags = 1 | 1 << 8;
    uint16_t awb[10] = { 32, 32, 0, 0, 128, 128, 128, 128, 0, 16383 };
    uint16_t ae[8]   = { 12, 8, 0, 0, 341, 512, 0, 16383 };
    uint16_t af[8]   = { 12, 8, 0, 0, 341, 512, 341, 512 };
    memcpy(c.awb, awb, sizeof(awb));
    memcpy(c.ae, ae, sizeof(ae));
    memcpy(c.af, af, sizeof(af));
    if (ioctl(fd, S3L_IOC_IMG_CONFIG_STATISTICS, &c) < 0) {
        HAL_WARNING("s3l_aaa", "CONFIG_STATISTICS failed: %s\n", strerror(errno));
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}

/* Written by the netlink thread, read by the AE thread: the lock keeps both gains from one frame */
static pthread_mutex_t _dsp_mtx = PTHREAD_MUTEX_INITIALIZER;
static float _dsp_wr, _dsp_wb;
static unsigned int _dsp_seq, _dsp_tiles;

static int cmp_f(const void *a, const void *b)
{
    float x = *(const float *)a, y = *(const float *)b;
    return (x > y) - (x < y);
}

void s3l_aaa_frame(const unsigned char *cfa)
{
    struct s3l_aaa_frame f;

    s3l_aaa_parse(cfa, &f);

    if (f.n_tiles >= 32) {
        qsort(f.gr, f.n_tiles, sizeof(float), cmp_f);
        qsort(f.gb, f.n_tiles, sizeof(float), cmp_f);
        pthread_mutex_lock(&_dsp_mtx);
        _dsp_wr = f.gr[f.n_tiles / 2];
        _dsp_wb = f.gb[f.n_tiles / 2];
        _dsp_tiles = f.n_tiles;
        _dsp_seq++;
        pthread_mutex_unlock(&_dsp_mtx);
    }
}

static pthread_t _ae_tid;
static volatile char _ae_run;
static char _opt_ae = 1, _opt_awb = 1;
static int _opt_target, _opt_flicker;

void s3l_aaa_options(char ae, int target, char awb, int antiflicker)
{
    _opt_ae = ae; _opt_awb = awb; _opt_target = target; _opt_flicker = antiflicker;
}

#define VIN_GET_AAAINFO    0x80447313u
#define VIN_GET_AGC        0x80287307u
#define VIN_GET_SHUTTER    0x800c7306u
#define VIN_SET_AGC_SHUTTER 0x40107308u

static int _vin_fd = -1;
static unsigned int _line_ticks, _max_rows, _gain_max_idx;
static double _step_db;

static int sensor_init(int fd, unsigned int *rows, double *gain)
{
    uint32_t aaa[17] = { 0 }, agc[10] = { 0 }, sh[3] = { 0 };
    _vin_fd = fd;
    if (ioctl(fd, VIN_GET_AAAINFO, aaa) == 0 && ioctl(fd, VIN_GET_AGC, agc) == 0 && aaa[0x2c / 4] &&
        aaa[0x38 / 4] && agc[0x10 / 4] && agc[0x8 / 4] >= agc[0xc / 4]) {
        _line_ticks = aaa[0x2c / 4];
        _max_rows = aaa[0x38 / 4];
        _step_db = agc[0x10 / 4] / 16777216.0;
        _gain_max_idx = (agc[0x8 / 4] - agc[0xc / 4]) / agc[0x10 / 4];
        *rows = ioctl(fd, VIN_GET_SHUTTER, sh) == 0 && sh[1] ? sh[1] / _line_ticks : _max_rows / 2;
        *gain = pow(10, (int32_t)(agc[0x4 / 4] - agc[0xc / 4]) / 16777216.0 / 20);
        return 0;
    }
    return -1;
}

static void sensor_apply(unsigned int rows, double gain)
{
    int idx = (int)lround(20 * log10(gain) / _step_db);
    if (idx < 0) idx = 0;
    if ((unsigned int)idx > _gain_max_idx) idx = _gain_max_idx;
    uint32_t a[4] = { 0, rows, (uint32_t)idx, 2 };
    ioctl(_vin_fd, VIN_SET_AGC_SHUTTER, a);
}

static void *s3l_ae_thread(void *arg)
{
    int fd = (int)(intptr_t)arg >> 8, fps = (int)(intptr_t)arg & 0xff;
    int ae = _opt_ae, awb = _opt_awb;
    double target = _opt_target > 16 ? _opt_target : 55;
    int flicker = _opt_flicker == 50 || _opt_flicker == 60 ? _opt_flicker : 0;
    uint32_t qb[16] = { 0  };
    static uint32_t d[300];
    unsigned char *m = MAP_FAILED;
    if (ioctl(fd, S3L_IOC_QUERY_BUF, qb) == 0)
        m = mmap64(NULL, qb[1], PROT_READ, MAP_SHARED, fd, qb[2]);
    if (m == MAP_FAILED) {
        HAL_WARNING("s3l_aaa", "No preview buffer to measure, auto exposure and white balance off\n");
        /* Still the one thread that applies ISP changes (night mode, grayscale) */
        while (_ae_run) { usleep(100000); s3l_isp_flush(); }
        return NULL;
    }

    unsigned int lines = 1, max_lines = 1, flick_lines = 0;
    double gain = 1.0, max_gain = 1.0;
    if (sensor_init(fd, &lines, &gain) == 0) {
        max_lines = _max_rows;
        max_gain = pow(10, _gain_max_idx * _step_db / 20);

        if (flicker) flick_lines = (unsigned int)lround(512e6 / (2.0 * flicker) / _line_ticks);
    } else if (ae) {
        HAL_WARNING("s3l_aaa", "The sensor driver reports no shutter/gain limits, auto exposure off\n");
        ae = 0;
    }
    double total = lines * gain;

    double wr = 1.5, wb = 1.8;
    s3l_isp_get_wb(&wr, &wb);

    int dbp_base = s3l_isp_dbp_mode() ? s3l_isp_dbp_base() : 0, dbp_step = -1;

    HAL_INFO("s3l_aaa", "AE %s via %s (target Y %.0f, %d fps, max %u lines, line %u ticks, gain 1..%.1fx, "
        "anti-flicker %d Hz = %u lines), AWB %s, DBP-vs-gain %s (base %d)\n", ae ? "on" : "off",
        "VIN ioctls", target, fps, max_lines, _line_ticks, max_gain, flicker, flick_lines,
        awb ? "on" : "off", dbp_base ? "on" : "off", dbp_base);

    for (unsigned int n = 0; _ae_run; n++) {
        usleep(100000);
        memset(d, 0, sizeof(d));
        d[0] = 3;  d[2] = 0;
        if (ioctl(fd, S3L_IOC_QUERY_DESC, d) < 0) { s3l_isp_flush(); continue; }
        uint32_t y = d[3], uv = d[4], w = d[5], h = d[6], pitch = d[7];
        if (!w || !h || w > pitch || (uint64_t)y + (uint64_t)pitch * h > qb[1] ||
            (uint64_t)uv + (uint64_t)pitch * ((h + 1) / 2) > qb[1]) { s3l_isp_flush(); continue; }

        double wsum = 0, wcnt = 0;
        unsigned long cnt = 0, clip = 0, su = 0, sv = 0, cuv = 0, su2 = 0, sv2 = 0, cuv2 = 0;
        for (uint32_t r = 8; r < h; r += 16)
            for (uint32_t c = 8; c < w; c += 16) {
                unsigned int Y = m[y + r * pitch + c];
                double wt = (r > h / 4 && r < 3 * h / 4 && c > w / 4 && c < 3 * w / 4) ? 2 : 1;
                wsum += wt * Y; wcnt += wt; cnt++;
                if (Y >= 235) clip++;
                if (Y > 40 && Y < 220) {
                    unsigned int U = m[uv + (r / 2) * pitch + (c & ~1u)], V = m[uv + (r / 2) * pitch + (c & ~1u) + 1];
                    su2 += U; sv2 += V; cuv2++;
                    if (abs((int)U - 128) + abs((int)V - 128) < 24) { su += U; sv += V; cuv++; }
                }
            }
        double mean = wsum / wcnt, clip_frac = (double)clip / cnt;

        static unsigned int last_seq;
        pthread_mutex_lock(&_dsp_mtx);
        unsigned int seq = _dsp_seq;
        int dsp_fresh = seq != last_seq && _dsp_tiles >= 32;
        double tr = _dsp_wr, tb = _dsp_wb;
        pthread_mutex_unlock(&_dsp_mtx);
        if (awb && n % 3 == 0 && dsp_fresh) {
            last_seq = seq;
            tr = fmin(fmax(tr, 0.8), 3.5); tb = fmin(fmax(tb, 0.8), 3.5);
            if (tr / tb > 2.5) tr = tb * 2.5;
            if (tr / tb < 0.4) tb = tr / 0.4;
            if (fabs(tr - wr) > 0.005 || fabs(tb - wb) > 0.005) {
                wr += 0.3 * (tr - wr); wb += 0.3 * (tb - wb);
                s3l_isp_set_wb(wr, wb);
            }
        } else if (awb && n % 3 == 0) {
            if (cuv < 200) { su = su2; sv = sv2; cuv = cuv2; }
            if (cuv > 200) {
                double eu = (double)su / cuv - 128, ev = (double)sv / cuv - 128;
                if (fabs(eu) > 0.7 || fabs(ev) > 0.7) {
                    wr *= exp(-0.03 * ev); wb *= exp(-0.03 * eu);

                    wr = fmin(fmax(wr, 0.8), 3.5); wb = fmin(fmax(wb, 0.8), 3.5);
                    if (wr / wb > 2.5) wr = wb * 2.5;
                    if (wr / wb < 0.4) wb = wr / 0.4;
                    s3l_isp_set_wb(wr, wb);
                }
            }
        }

        if (!ae) { s3l_isp_flush(); continue; }
        /* Highlight protection only for scenes that are mostly blown out (a sunlit wall or sky routinely
         * clips 5 to 10% of the frame and must not darken the rest of it) */
        double tgt = target * fmin(1.0, fmax(0.7, 1 - 2 * (clip_frac - 0.15)));
        if (fabs(mean - tgt) < 3) { s3l_isp_flush(); continue; }
        double cur = pow(fmax(mean - 16, 0.5) / 219, 1 / 0.45), want = pow((tgt - 16) / 219, 1 / 0.45);
        double ratio = sqrt(fmin(1.6, fmax(0.6, want / cur)));
        total = fmin(fmax(total * ratio, 1), max_lines * max_gain);
        lines = total < max_lines ? (unsigned int)total : max_lines;
        if (flick_lines && flick_lines <= max_lines && lines >= flick_lines)
            lines = lines / flick_lines * flick_lines;
        if (lines < 1) lines = 1;
        gain = fmin(fmax(total / lines, 1.0), max_gain);
        sensor_apply(lines, gain);

        if (dbp_base) {
            int step = gain < 2 ? 0 : gain < 4 ? 1 : gain < 8 ? 2 : 3;
            if (step != dbp_step) {
                int thr = (int)lround(dbp_base * sqrt((double)(1 << step)));
                s3l_isp_set_dbp(thr);
                dbp_step = step;
            }
        }
        s3l_isp_flush();
    }
    munmap(m, qb[1]);
    return NULL;
}

int s3l_ae_start(int fd, int fps)
{
    _ae_run = 1;
    if (pthread_create(&_ae_tid, NULL, s3l_ae_thread, (void *)(intptr_t)(fd << 8 | (fps & 0xff)))) {
        _ae_run = 0;
        HAL_DANGER("s3l_aaa", "Starting the AE thread failed!\n");
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}

void s3l_ae_stop(void)
{
    if (!_ae_run) return;
    _ae_run = 0;
    pthread_join(_ae_tid, NULL);
}

#endif
