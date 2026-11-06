#if defined(__arm__)

#include "s3l_hal.h"

#include <sys/socket.h>
#include <linux/netlink.h>
#include <pthread.h>
#include <signal.h>
#include <sys/time.h>
#include <time.h>

hal_chnstate s3l_state[S3L_VENC_CHN_NUM] = {0};
int (*s3l_aud_cb)(hal_audframe*);
int (*s3l_vid_cb)(char, hal_vidstream*);

static int _s3l_fd = -1;
static unsigned char *_s3l_bsb;
static unsigned int _s3l_bsb_len;
static unsigned short _s3l_main_w, _s3l_main_h;
static unsigned short _s3l_sub_w = 640, _s3l_sub_h = 480;
static char _s3l_mjpeg_buf = 1;

static char _s3l_isp_path[128];
static unsigned char *_s3l_img;
static unsigned int _s3l_img_len, _s3l_img_phys, _s3l_liso_off, _s3l_upd_slot;

static int _s3l_nl_sock = -1;
static pthread_t _s3l_nl_tid, _s3l_stats_tid;
static volatile char _s3l_aaa_run, _s3l_previewing;

static unsigned int s3l_state_get(void)
{
    unsigned int state = 0xdeadbeef;
    ioctl(_s3l_fd, S3L_IOC_GET_IAV_STATE, &state);
    return state;
}

/* IAV waits for the DSP interruptibly: a signal there leaves it stuck until a reboot */
static int s3l_ioctl_nosig(unsigned long cmd, void *arg)
{
    sigset_t all, old;
    int ret, err;

    sigfillset(&all);
    pthread_sigmask(SIG_BLOCK, &all, &old);
    ret = ioctl(_s3l_fd, cmd, arg);
    err = errno;
    pthread_sigmask(SIG_SETMASK, &old, NULL);
    errno = err;
    return ret;
}

static int s3l_ioctl(unsigned long cmd, void *arg, const char *what)
{
    int state_change = cmd == S3L_IOC_ENTER_IDLE || cmd == S3L_IOC_ENABLE_PREVIEW;

    if ((state_change ? s3l_ioctl_nosig(cmd, arg) : ioctl(_s3l_fd, cmd, arg)) < 0) {
        HAL_DANGER("s3l_iav", "%s failed: %s\n", what, strerror(errno));
        return -1;
    }
    return 0;
}

void s3l_hal_deinit(void)
{
    if (_s3l_fd >= 0) close(_s3l_fd);
    _s3l_fd = -1;
}

int s3l_hal_init(void)
{
    if ((_s3l_fd = open("/dev/iav", O_RDWR)) < 0)
        HAL_ERROR("s3l_iav", "Cannot open /dev/iav: %s\n", strerror(errno));
    return EXIT_SUCCESS;
}

static int s3l_nl_send(uint32_t type, uint32_t dir, uint32_t cmd, uint32_t status)
{
    struct { struct nlmsghdr hdr; struct s3l_nl_msg msg; } buf;
    struct sockaddr_nl to = { .nl_family = AF_NETLINK };

    memset(&buf, 0, sizeof(buf));
    buf.hdr.nlmsg_len = NLMSG_LENGTH(sizeof(buf.msg));
    buf.hdr.nlmsg_pid = getpid();
    buf.msg.pid = getpid();
    buf.msg.port = S3L_NL_PORT_IMAGE;
    buf.msg.type = type;
    buf.msg.dir = dir;
    buf.msg.cmd = cmd;
    buf.msg.status = status;
    return sendto(_s3l_nl_sock, &buf, buf.hdr.nlmsg_len, 0,
        (struct sockaddr *)&to, sizeof(to));
}

static void *s3l_nl_thread(void *arg)
{
    (void)arg;
    while (_s3l_aaa_run) {
        struct { struct nlmsghdr hdr; struct s3l_nl_msg msg; unsigned char extra[256]; } buf;
        ssize_t n = recv(_s3l_nl_sock, &buf, sizeof(buf), 0);
        if (n < 0) {
            /* ENOBUFS: messages were dropped while this thread was starved; the session stands */
            if (errno == EINTR || errno == EAGAIN || errno == ENOBUFS) continue;
            HAL_WARNING("s3l_aaa", "Image netlink receive failed: %s\n", strerror(errno));
            break;
        }
        if (n < (ssize_t)NLMSG_LENGTH(24)) continue;

        if (buf.msg.type == 1 && buf.msg.dir == 0)
            s3l_nl_send(1, 1, buf.msg.cmd, 0);
    }
    return NULL;
}

static void *s3l_stats_thread(void *arg)
{
    static unsigned char rgb[262144], cfa[262144], hist[65536], hdr[65536];
    unsigned char rv, cv, hv, dv;
    unsigned int pts;
    unsigned long long mpts;
    void *stats[10] = { &rv, &cv, &hv, &dv, rgb, cfa, hist, hdr, &pts, &mpts };
    (void)arg;

    while (_s3l_aaa_run) {
        /* Asked outside preview, the driver waits its 5 s out even if preview starts meanwhile */
        if (!_s3l_previewing) { usleep(20000); continue; }
        rv = cv = 0;
        if (ioctl(_s3l_fd, S3L_IOC_GET_STATISTICS, stats) < 0)
            usleep(30000);
        else if (cv && rv)
            s3l_aaa_frame(cfa);
    }
    return NULL;
}

static int s3l_aaa_start(void)
{
    struct sockaddr_nl me = { .nl_family = AF_NETLINK, .nl_pid = getpid() };
    struct timeval tv = { .tv_sec = 0, .tv_usec = 500000 };

    if ((_s3l_nl_sock = socket(AF_NETLINK, SOCK_RAW, S3L_NL_PORT_IMAGE)) < 0)
        HAL_ERROR("s3l_aaa", "Image netlink socket failed: %s\n", strerror(errno));
    if (bind(_s3l_nl_sock, (struct sockaddr *)&me, sizeof(me)) < 0)
        HAL_ERROR("s3l_aaa", "Image netlink bind failed: %s\n", strerror(errno));
    setsockopt(_s3l_nl_sock, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
    if (s3l_nl_send(0, 0, 0 , 0) < 0)
        HAL_ERROR("s3l_aaa", "Image netlink connect failed: %s\n", strerror(errno));

    _s3l_aaa_run = 1;
    if (pthread_create(&_s3l_nl_tid, NULL, s3l_nl_thread, NULL) ||
        pthread_create(&_s3l_stats_tid, NULL, s3l_stats_thread, NULL))
        HAL_ERROR("s3l_aaa", "Starting the 3A threads failed\n");
    return EXIT_SUCCESS;
}

static void s3l_aaa_stop(void)
{
    if (!_s3l_aaa_run) return;
    _s3l_aaa_run = 0;
    pthread_join(_s3l_nl_tid, NULL);
    pthread_join(_s3l_stats_tid, NULL);
    close(_s3l_nl_sock);
    _s3l_nl_sock = -1;
}

/* Below the rings the HAL keeps its own blocks (LISO update slots, DBP sets, the mono curve) */
#define S3L_IMG_SCRATCH_OFF (S3L_IMG_RINGS_OFF - 0x5000)

static int s3l_img_load(unsigned char *img, unsigned int len, uint32_t phys, const char *path)
{
    struct s3l_isp_hdr hdr;
    struct s3l_isp_op *ops = NULL;
    uint32_t *rels = NULL;
    static unsigned char page[4096];
    unsigned int written = 0, liso = 0;
    int ret = EXIT_FAILURE;
    FILE *file = fopen(path, "rb");

    _s3l_liso_off = 0;

    if (!file) {
        int e = errno;
        HAL_WARNING("s3l_isp", "Cannot open %s: %s\n", path, strerror(e));
        return EXIT_FAILURE;
    }
    if (fread(&hdr, sizeof(hdr), 1, file) != 1 || memcmp(hdr.magic, S3L_ISP_MAGIC, 8) ||
        !hdr.ops || hdr.ops > 256 || hdr.rels > S3L_ISP_RELS_MAX ||
        hdr.img_off > S3L_IMG_SCRATCH_OFF || hdr.img_len > S3L_IMG_SCRATCH_OFF - hdr.img_off ||
        hdr.img_off + hdr.img_len > len) {
        HAL_WARNING("s3l_isp", "%s is not an S3L ISP configuration (%s)\n", path, S3L_ISP_MAGIC);
        goto out;
    }
    uint32_t img_off = hdr.img_off;
    if (!(ops = malloc(hdr.ops * sizeof(*ops))) ||
        fread(ops, sizeof(*ops), hdr.ops, file) != hdr.ops ||
        (hdr.rels && (!(rels = malloc(hdr.rels * sizeof(*rels))) ||
            fread(rels, sizeof(*rels), hdr.rels, file) != hdr.rels)))
        goto out;

    for (unsigned int o = 0; o < hdr.img_len; o += sizeof(page)) {
        size_t want = MIN(sizeof(page), hdr.img_len - o), got = fread(page, 1, want, file);
        if (got != want) {
            HAL_WARNING("s3l_isp", "%s is cut short\n", path);
            goto out;
        }

        memcpy(img + img_off + o, page, got);
        written++;
    }

    /* Table pointers are IMG offsets in the file: point them at the buffer where the driver put it */
    for (unsigned int i = 0; i < hdr.rels; i++) {
        uint32_t p;
        if (rels[i] & 3 || rels[i] < img_off || rels[i] + 4 > img_off + hdr.img_len) goto bad_rel;
        memcpy(&p, img + rels[i], 4);
        if (p < img_off || p >= img_off + hdr.img_len) goto bad_rel;
        p += phys;
        memcpy(img + rels[i], &p, 4);
        continue;
bad_rel:
        HAL_WARNING("s3l_isp", "%s: relocation %u is outside the configuration\n", path, i);
        goto out;
    }

    for (unsigned int i = 0; i < hdr.ops; i++) {
        uintptr_t blk = (uintptr_t)img + ops[i].offset;
        int r;
        if (ops[i].offset < img_off || ops[i].offset >= img_off + hdr.img_len ||
            ops[i].offset >= S3L_IMG_SCRATCH_OFF || (ops[i].kind != 's' && ops[i].kind != 'u')) goto out;
        /* The LISO block is read and rewritten here later: all 0x400 bytes must be in the image */
        if (ops[i].kind == 's' && ops[i].nr == 0x60) {
            if (ops[i].offset + 0x400 > img_off + hdr.img_len || ops[i].offset + 0x400 > S3L_IMG_SCRATCH_OFF) goto out;
            liso = ops[i].offset;
        }
        if (ops[i].kind == 's')
            r = ioctl(_s3l_fd, S3L_IOC_IMG_SET(ops[i].nr), blk);
        else {
            uint32_t arg[8] = { (uint32_t)blk, ops[i].flags };
            r = ioctl(_s3l_fd, S3L_IOC_IMG_UPDATE(ops[i].nr), arg);
        }
        if (r < 0) {
            HAL_WARNING("s3l_isp", "ISP op %u ('%c' %#x) failed: %s\n",
                i, ops[i].kind, ops[i].nr, strerror(errno));
            goto out;
        }
    }
    HAL_INFO("s3l_isp", "Loaded %s (%u ops, %u pages)\n", path, hdr.ops, written);
    _s3l_liso_off = liso;
    ret = EXIT_SUCCESS;
out:
    free(rels);
    free(ops);
    fclose(file);
    return ret;
}

static int s3l_img_prepare(void)
{
    uint32_t qb[16] = { S3L_BUFFER_IMG };
    unsigned char *img;

    if (s3l_ioctl(S3L_IOC_QUERY_BUF, qb, "QUERY_BUF(IMG)"))
        return EXIT_FAILURE;
    if (qb[1] < S3L_IMG_RINGS_OFF + S3L_IMG_RINGS_LEN)
        HAL_ERROR("s3l_isp", "IMG buffer too small (%#x)\n", qb[1]);
    img = mmap64(NULL, qb[1], PROT_READ | PROT_WRITE, MAP_SHARED, _s3l_fd, qb[2]);
    if (img == MAP_FAILED)
        HAL_ERROR("s3l_isp", "Mapping the IMG buffer failed: %s\n", strerror(errno));

    /* After a cold boot the DSP would read uninitialised DRAM here as commands */
    memset(img + S3L_IMG_RINGS_OFF, 0, S3L_IMG_RINGS_LEN);

    /* Otherwise the one for the sensor the VIN reports, as OpenIPC's S3L package installs them,
       if the VIN named one (a plain name: it becomes part of a path) */
    char def[64] = "";
    int named = *sensor && strcmp(sensor, "unidentified");
    for (const char *c = sensor; named && *c; c++)
        named = isalnum((unsigned char)*c) || *c == '_' || *c == '-';
    if (named)
        snprintf(def, sizeof(def), S3L_ISP_DIR "%s.bin", sensor);
    /* Without one the DSP produces no frames at all, and a preview started that way cannot be left
       until a reboot: stop here, with the DSP still idle */
    if ((!*_s3l_isp_path || s3l_img_load(img, qb[1], qb[2], _s3l_isp_path)) &&
        (!*def || !strcmp(_s3l_isp_path, def) || s3l_img_load(img, qb[1], qb[2], def))) {
        munmap(img, qb[1]);
        HAL_ERROR("s3l_isp", "No ISP configuration loaded (system.sensor_config%s%s); "
            "the DSP produces no image without one\n", *def ? ", " : "", def);
    }

    _s3l_img = img;
    _s3l_img_len = qb[1];
    _s3l_img_phys = qb[2];
    return EXIT_SUCCESS;
}

#define S3L_UPD_LISO_SLOTS (S3L_IMG_RINGS_OFF - 0x2000)
#define S3L_UPD_DBP_SETS   (S3L_IMG_RINGS_OFF - 0x4000)
#define S3L_UPD_MONO_CURVE (S3L_IMG_RINGS_OFF - 0x5000)

static unsigned int _s3l_dbp_set;
static uint8_t _s3l_pend_15c, _s3l_pend_1f4;

static int s3l_isp_commit(uint8_t upd_15c, uint8_t upd_1f4)
{
    if (!_s3l_img || !_s3l_liso_off) return EXIT_FAILURE;
    unsigned char *blk = _s3l_img + S3L_UPD_LISO_SLOTS + (_s3l_upd_slot++ & 1) * 0x400;
    memcpy(blk, _s3l_img + _s3l_liso_off, 0x400);
    blk[0x15c] |= upd_15c;
    blk[0x1f4] |= upd_1f4;
    uint32_t arg[8] = { (uint32_t)(uintptr_t)blk, 0x1 };
    if (ioctl(_s3l_fd, S3L_IOC_IMG_UPDATE(0x60), arg) < 0) {
        static char told;   /* retried every flush until it works: say it once */
        if (!told++) HAL_WARNING("s3l_isp", "LISO update failed: %s\n", strerror(errno));
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}

int s3l_isp_flush(void)
{
    /* Acquire: the block edits made before a bit was set are seen with it */
    uint8_t a = __atomic_exchange_n(&_s3l_pend_15c, 0, __ATOMIC_ACQUIRE);
    uint8_t b = __atomic_exchange_n(&_s3l_pend_1f4, 0, __ATOMIC_ACQUIRE);
    if (!a && !b) return EXIT_SUCCESS;
    if (s3l_isp_commit(a, b)) {
        /* Kept for the next flush: some changes (grayscale) are made only once */
        __atomic_fetch_or(&_s3l_pend_15c, a, __ATOMIC_RELAXED);
        __atomic_fetch_or(&_s3l_pend_1f4, b, __ATOMIC_RELAXED);
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}

int s3l_isp_get_wb(double *r, double *b)
{
    uint32_t g[4];
    if (!_s3l_img || !_s3l_liso_off) return EXIT_FAILURE;
    memcpy(g, _s3l_img + _s3l_liso_off + 0xe4, sizeof(g));
    *r = g[0] / 65536.0; *b = g[3] / 65536.0;
    return EXIT_SUCCESS;
}

int s3l_isp_set_wb(double r, double b)
{
    if (!_s3l_img || !_s3l_liso_off) return EXIT_FAILURE;
    uint32_t g[4] = { (uint32_t)(r * 65536), 0x10000, 0x10000, (uint32_t)(b * 65536) };
    memcpy(_s3l_img + _s3l_liso_off + 0xe4, g, sizeof(g));
    __atomic_fetch_or(&_s3l_pend_15c, 0x02, __ATOMIC_RELEASE);
    return EXIT_SUCCESS;
}

int s3l_isp_dbp_mode(void)
{
    return _s3l_img && _s3l_liso_off ? _s3l_img[_s3l_liso_off + 0x1f0] & 7 : 0;
}

int s3l_isp_dbp_base(void)
{
    if (!_s3l_img || !_s3l_liso_off || !_s3l_img_phys) return 0;
    uint32_t p;
    memcpy(&p, _s3l_img + _s3l_liso_off + 0x160, 4);
    if (p < _s3l_img_phys || p - _s3l_img_phys + 2 > _s3l_img_len) return 0;
    return _s3l_img[p - _s3l_img_phys] | _s3l_img[p - _s3l_img_phys + 1] << 8;
}

int s3l_isp_set_dbp(int thr)
{
    if (!_s3l_img || !_s3l_liso_off || !_s3l_img_phys) return EXIT_FAILURE;
    if (thr < 0) thr = 0;
    if (thr > 255) thr = 255;
    unsigned int set = S3L_UPD_DBP_SETS + (_s3l_dbp_set++ & 1) * 0x800;
    uint16_t *dark = (uint16_t *)(_s3l_img + set), *hot = (uint16_t *)(_s3l_img + set + 0x300);
    for (int i = 0; i < 384; i++) dark[i] = hot[i] = thr;
    uint32_t pd = _s3l_img_phys + set, ph = _s3l_img_phys + set + 0x300;
    memcpy(_s3l_img + _s3l_liso_off + 0x160, &pd, 4);
    memcpy(_s3l_img + _s3l_liso_off + 0x164, &ph, 4);
    __atomic_fetch_or(&_s3l_pend_1f4, 0x03, __ATOMIC_RELEASE);
    return EXIT_SUCCESS;
}

static uint32_t _s3l_chroma_curve;

void s3l_channel_grayscale(char enable)
{
    if (!_s3l_img || !_s3l_liso_off || !_s3l_img_phys) return;
    uint32_t cur, mono = _s3l_img_phys + S3L_UPD_MONO_CURVE;
    memcpy(&cur, _s3l_img + _s3l_liso_off + 0x120, 4);
    if (enable) {
        if (cur == mono) return;
        _s3l_chroma_curve = cur;
        memset(_s3l_img + S3L_UPD_MONO_CURVE, 0, 256);
        memcpy(_s3l_img + _s3l_liso_off + 0x120, &mono, 4);
    } else {
        if (cur != mono || !_s3l_chroma_curve) return;
        memcpy(_s3l_img + _s3l_liso_off + 0x120, &_s3l_chroma_curve, 4);
    }
    __atomic_fetch_or(&_s3l_pend_15c, 0x10, __ATOMIC_RELEASE);
}

int s3l_channel_bind(char index)
{
    (void)index;
    return EXIT_SUCCESS;
}

int s3l_channel_unbind(char index)
{
    (void)index;
    return EXIT_SUCCESS;
}

void s3l_sub_size(short width, short height)
{
    if (width > 0 && height > 0) {
        _s3l_sub_w = (width + 15) & ~15;
        _s3l_sub_h = (height + 7) & ~7;
    }
}

int s3l_pipeline_create(short width, short height, char mirror, char flip, char framerate)
{
    static unsigned char buf[4096];
    struct s3l_vin_mode mode = { 0, 0, 0, 0 };
    struct s3l_vin_fps fps = { 0, S3L_VIN_FPS(framerate > 0 ? framerate : 30) };
    struct s3l_vin_info info = { 0 };
    unsigned int state;
    uint32_t v[2];

    if (s3l_aaa_start())
        return EXIT_FAILURE;

    if ((state = s3l_state_get()) != S3L_IAV_STATE_IDLE &&
        s3l_ioctl(S3L_IOC_ENTER_IDLE, 0, "ENTER_IDLE"))
        return EXIT_FAILURE;

    if (s3l_ioctl(S3L_IOC_VIN_SET_MODE, &mode, "VIN_SET_MODE") ||
        s3l_ioctl(S3L_IOC_VIN_SET_FPS, &fps, "VIN_SET_FPS") ||
        s3l_ioctl(S3L_IOC_VIN_GET_VIDEOINFO, &info, "VIN_GET_VIDEOINFO"))
        return EXIT_FAILURE;
    if (!info.width || !info.height)
        HAL_ERROR("s3l_iav", "The sensor driver reports no video mode\n");

    {
        uint32_t mir[3] = { 0, (uint32_t)((flip ? 1 : 0) | (mirror ? 2 : 0)), 255 };
        /* Every time, off included: the VIN keeps the last pattern across runs */
        s3l_ioctl(S3L_IOC_VIN_SET_MIRROR, mir, "VIN_SET_MIRROR");
    }

    _s3l_main_w = (width > 0 && width < (int)info.width) ? width : info.width;
    _s3l_main_h = (height > 0 && height < (int)info.height) ? height : info.height;
    HAL_INFO("s3l_iav", "VIN %ux%u, main buffer %ux%u\n",
        info.width, info.height, _s3l_main_w, _s3l_main_h);

    /* A sub-buffer must be smaller than the main one, and the second one at most 720x720 */
    if (_s3l_sub_w >= _s3l_main_w || _s3l_sub_h >= _s3l_main_h)
        _s3l_mjpeg_buf = 0;
    else if (_s3l_sub_w > S3L_SRCBUF1_MAX || _s3l_sub_h > S3L_SRCBUF1_MAX)
        _s3l_mjpeg_buf = 2;
    else
        _s3l_mjpeg_buf = 1;
    unsigned short b1w = _s3l_mjpeg_buf == 1 ? _s3l_sub_w : MIN(640, (_s3l_main_w / 2) & ~15);
    unsigned short b1h = _s3l_mjpeg_buf == 1 ? _s3l_sub_h : MIN(480, (_s3l_main_h / 2) & ~7);
    if (!_s3l_mjpeg_buf) {
        _s3l_sub_w = _s3l_main_w;
        _s3l_sub_h = _s3l_main_h;
    }

    memset(buf, 0, sizeof(buf));
    if (s3l_ioctl(S3L_IOC_GET_SYSRES, buf, "GET_SYSTEM_RESOURCE"))
        return EXIT_FAILURE;
    buf[S3L_SYSRES_ENC_MODE] = 0;
    buf[S3L_SYSRES_MAX_NUM_ENC] = 2;
    v[0] = _s3l_main_w; v[1] = _s3l_main_h;
    memcpy(buf + S3L_SYSRES_BUF_MAX(0), v, 8);
    memcpy(buf + S3L_SYSRES_STREAM_MAX(0), v, 8);
    memset(buf + S3L_SYSRES_BUF_MAX(2), 0, 24);
    memset(buf + S3L_SYSRES_STREAM_MAX(2), 0, 16);
    v[0] = b1w; v[1] = b1h;
    memcpy(buf + S3L_SYSRES_BUF_MAX(1), v, 8);
    v[0] = _s3l_sub_w; v[1] = _s3l_sub_h;
    if (_s3l_mjpeg_buf == 2)
        memcpy(buf + S3L_SYSRES_BUF_MAX(2), v, 8);
    memcpy(buf + S3L_SYSRES_STREAM_MAX(1), v, 8);
    v[0] = 1; memcpy(buf + S3L_SYSRES_C0, v, 4);
    v[0] = 0; memcpy(buf + S3L_SYSRES_DC, v, 4);
    memcpy(buf + S3L_SYSRES_EXTRA_A4, v, 4);
    memcpy(buf + S3L_SYSRES_EXTRA_D8, v, 4);
    if (s3l_ioctl(S3L_IOC_SET_SYSRES, buf, "SET_SYSTEM_RESOURCE"))
        return EXIT_FAILURE;

    memset(buf, 0, sizeof(buf));
    if (s3l_ioctl(S3L_IOC_GET_SRCBUF_SETUP, buf, "GET_SRCBUF_SETUP"))
        return EXIT_FAILURE;
    for (int i = 0; i < 4; i++) {
        uint32_t type;
        memcpy(&type, buf + S3L_SRCBUF_TYPE(i), 4);
        if (type == S3L_SRCBUF_TYPE_PREVIEW) {
            type = 0;
            memcpy(buf + S3L_SRCBUF_TYPE(i), &type, 4);
        }
    }
    v[0] = _s3l_main_w; v[1] = _s3l_main_h;
    memcpy(buf + S3L_SRCBUF_SIZE(0), v, 8);
    memcpy(buf + S3L_SRCBUF_MAIN_WIN_A, v, 8);

    for (int k = 0; k < 4; k++)
        memcpy(buf + S3L_SRCBUF_SUB_INPUT(k), v, 8);
    v[0] = info.width; v[1] = info.height;
    memcpy(buf + S3L_SRCBUF_MAIN_WIN_B, v, 8);
    v[0] = b1w; v[1] = b1h;
    memcpy(buf + S3L_SRCBUF_SIZE(1), v, 8);
    for (int i = 2; i < 4; i++) {
        char used = i == 2 && _s3l_mjpeg_buf == 2;
        v[0] = used ? _s3l_sub_w : 0; v[1] = used ? _s3l_sub_h : 0;
        memcpy(buf + S3L_SRCBUF_SIZE(i), v, 8);
        v[0] = used ? S3L_SRCBUF_TYPE_ENCODE : 0;
        memcpy(buf + S3L_SRCBUF_TYPE(i), v, 4);
    }
    if (s3l_ioctl(S3L_IOC_SET_SRCBUF_SETUP, buf, "SET_SRCBUF_SETUP"))
        return EXIT_FAILURE;

    if (s3l_img_prepare())
        return EXIT_FAILURE;

    if (s3l_ioctl(S3L_IOC_ENABLE_PREVIEW, 0, "ENABLE_PREVIEW"))
        return EXIT_FAILURE;

    /* The DSP takes the 3A statistics setup into effect only when preview starts, and it can
       only be sent once preview runs: in the first preview after the microcode is loaded, the
       SDK's 284712 sends no statistics and this camera's own 267837 sends stale ones. So the
       setup is sent, left in place for a frame, and preview started again, before any stream
       is encoding. Every later preview would do without it; it costs about 0.15 s. If the
       driver refuses to go back to idle (an emulated DSP does), preview is still running:
       carry on with the statistics as they are. */
    s3l_aaa_config(_s3l_fd);
    usleep(100000);
    if (s3l_ioctl_nosig(S3L_IOC_ENTER_IDLE, 0) < 0) {
        int err = errno;
        HAL_WARNING("s3l_iav", "Restarting preview for the 3A statistics was refused (%s); "
            "they may stay stale until the next start\n", strerror(err));
    } else {
        if (s3l_ioctl(S3L_IOC_ENABLE_PREVIEW, 0, "ENABLE_PREVIEW"))
            return EXIT_FAILURE;
        s3l_aaa_config(_s3l_fd);
    }
    _s3l_previewing = 1;
    s3l_ae_start(_s3l_fd, framerate > 0 ? framerate : 30);

    {
        uint32_t qb[16] = { S3L_BUFFER_BSB };
        if (s3l_ioctl(S3L_IOC_QUERY_BUF, qb, "QUERY_BUF(BSB)"))
            return EXIT_FAILURE;
        _s3l_bsb_len = qb[1];
        _s3l_bsb = mmap64(NULL, qb[1], PROT_READ, MAP_SHARED, _s3l_fd, qb[2]);
        if (_s3l_bsb == MAP_FAILED) {
            _s3l_bsb = NULL;
            HAL_ERROR("s3l_iav", "Mapping the bitstream buffer failed: %s\n", strerror(errno));
        }
    }

    return EXIT_SUCCESS;
}

void s3l_pipeline_destroy(void)
{
    s3l_ae_stop();
    if (_s3l_bsb) munmap(_s3l_bsb, _s3l_bsb_len);
    _s3l_bsb = NULL;
    _s3l_previewing = 0;
    if (s3l_state_get() != S3L_IAV_STATE_IDLE)
        s3l_ioctl_nosig(S3L_IOC_ENTER_IDLE, 0);
    s3l_aaa_stop();
}

int s3l_video_create(char index, hal_vidconfig *config)
{
    static unsigned char buf[256];
    uint32_t v[4];

    if (index < 0 || index >= S3L_VENC_CHN_NUM)
        HAL_ERROR("s3l_venc", "Only %d encoder channel(s) are supported!\n", S3L_VENC_CHN_NUM);
    if (config->codec != HAL_VIDCODEC_H264 && config->codec != HAL_VIDCODEC_MJPG)
        HAL_ERROR("s3l_venc", "Only H.264 and MJPEG are supported so far!\n");
    char mjpeg = config->codec == HAL_VIDCODEC_MJPG;

    memset(buf, 0, sizeof(buf));
    buf[S3L_STRFMT_ID] = index;
    if (s3l_ioctl(S3L_IOC_GET_STREAM_FORMAT, buf, "GET_STREAM_FORMAT"))
        return EXIT_FAILURE;
    v[0] = mjpeg ? S3L_STREAM_TYPE_MJPEG : S3L_STREAM_TYPE_H264;
    memcpy(buf + S3L_STRFMT_TYPE, v, 4);
    char sub = mjpeg ? _s3l_mjpeg_buf : 0;
    v[0] = sub;
    memcpy(buf + S3L_STRFMT_BUF_ID, v, 4);
    v[0] = sub ? _s3l_sub_w : _s3l_main_w;
    v[1] = sub ? _s3l_sub_h : _s3l_main_h;
    v[2] = 0; v[3] = 0;
    memcpy(buf + S3L_STRFMT_ENC_WIN, v, 16);
    if (s3l_ioctl(S3L_IOC_SET_STREAM_FORMAT, buf, "SET_STREAM_FORMAT"))
        return EXIT_FAILURE;

    memset(buf, 0, sizeof(buf));
    buf[0] = index;
    if (mjpeg) {
        if (s3l_ioctl(S3L_IOC_GET_MJPEG_CFG, buf, "GET_MJPEG_CFG"))
            return EXIT_FAILURE;
        unsigned char quality = MAX(config->minQual, config->maxQual);
        if (quality >= 1 && quality <= 100)
            buf[5] = quality;
        if (s3l_ioctl(S3L_IOC_SET_MJPEG_CFG, buf, "SET_MJPEG_CFG"))
            return EXIT_FAILURE;
    } else {
        uint32_t *w = (uint32_t *)buf;
        if (s3l_ioctl(S3L_IOC_GET_H264_CFG, buf, "GET_H264_CFG"))
            return EXIT_FAILURE;
        if (config->gop > 0)
            w[S3L_H264_W_GOP] = (w[S3L_H264_W_GOP] & 0xffff) | (uint32_t)config->gop << 16;
        /* IAV's 1 Main and 2 High: the S3L's has no Baseline (0 is refused) */
        if (config->profile == HAL_VIDPROFILE_BASELINE)
            HAL_WARNING("s3l_venc", "No H.264 Baseline profile on S3L, encoding Main\n");
        w[S3L_H264_W_IDR_PROFILE] = (w[S3L_H264_W_IDR_PROFILE] & ~0xffffu) | 1 |
            (uint32_t)(config->profile == HAL_VIDPROFILE_HIGH ? 2 : 1) << 8;
        if (s3l_ioctl(S3L_IOC_SET_H264_CFG, buf, "SET_H264_CFG"))
            return EXIT_FAILURE;

        uint32_t rc[32] = { (uint32_t)index, S3L_STMCFG_H264_BITRATE };
        if (config->bitrate > 0 && !s3l_ioctl(S3L_IOC_GET_STREAM_CFG, rc, "GET_STREAM_CFG(bitrate)")) {
            rc[3] = config->bitrate * 1000;
            if (config->mode == HAL_VIDMODE_CBR || config->mode == HAL_VIDMODE_VBR)
                rc[4] = (rc[4] & ~0xffu) | (config->mode == HAL_VIDMODE_CBR ? S3L_BRC_CBR : S3L_BRC_VBR);
            else
                HAL_WARNING("s3l_venc", "Only CBR and VBR on S3L, keeping the encoder's own rate control\n");
            s3l_ioctl(S3L_IOC_SET_STREAM_CFG, rc, "SET_STREAM_CFG(bitrate)");
        }
    }

    if (s3l_ioctl_nosig(S3L_IOC_START_ENCODE, (void *)(1u << index)) < 0)
        HAL_ERROR("s3l_venc", "Starting encoder %d failed: %s\n", index, strerror(errno));

    s3l_state[index].payload = config->codec;
    s3l_state[index].enable = 1;
    return EXIT_SUCCESS;
}

int s3l_video_destroy(char index)
{
    if (index < 0 || index >= S3L_VENC_CHN_NUM) return EXIT_FAILURE;
    if (s3l_state[index].enable)
        s3l_ioctl_nosig(S3L_IOC_STOP_ENCODE, (void *)(1u << index));
    s3l_state[index].enable = 0;
    s3l_state[index].payload = HAL_VIDCODEC_UNSPEC;
    return EXIT_SUCCESS;
}

int s3l_video_destroy_all(void)
{
    for (char i = 0; i < S3L_VENC_CHN_NUM; i++)
        s3l_video_destroy(i);
    return EXIT_SUCCESS;
}

void s3l_video_request_idr(char index)
{
    uint32_t cfg[32] = { (uint32_t)index, S3L_STMCFG_H264_FORCE_IDR, 1 };

    if (index < 0 || index >= S3L_VENC_CHN_NUM || !s3l_state[index].enable ||
        s3l_state[index].payload != HAL_VIDCODEC_H264)
        return;
    s3l_ioctl(S3L_IOC_SET_STREAM_CFG, cfg, "SET_STREAM_CFG(force IDR)");
}

void *s3l_video_thread(void)
{
    static uint32_t desc[S3L_SZ_QUERYDESC / 4 + 64];
    unsigned char *wrapBuf = NULL;
    unsigned int wrapLen = 0;

    while (keepRunning) {
        if (!_s3l_bsb || !(s3l_state[0].enable || s3l_state[1].enable)) { usleep(100000); continue; }

        memset(desc, 0, sizeof(desc));
        desc[0] = S3L_DESC_FRAME;
        desc[S3L_DESC_W_ID] = 0xffffffff;
        desc[S3L_DESC_W_TIMEOUT_MS] = 500;
        if (ioctl(_s3l_fd, S3L_IOC_QUERY_DESC, desc) < 0) {
            if (errno != EAGAIN && errno != ETIMEDOUT && errno != EINTR) {
                HAL_WARNING("s3l_venc", "Querying a frame failed: %s\n", strerror(errno));
                usleep(100000);
            }
            continue;
        }

        unsigned int off = desc[S3L_DESC_W_DATA_OFF], size = desc[S3L_DESC_W_SIZE];
        if (off >= _s3l_bsb_len || !size || size > _s3l_bsb_len) continue;

        unsigned char *data = _s3l_bsb + off;
        if (off + size > _s3l_bsb_len) {
            if (wrapLen < size) {
                free(wrapBuf);
                wrapBuf = malloc(wrapLen = size);
                if (!wrapBuf) { wrapLen = 0; continue; }
            }
            unsigned int first = _s3l_bsb_len - off;
            memcpy(wrapBuf, data, first);
            memcpy(wrapBuf + first, _s3l_bsb, size - first);
            data = wrapBuf;
        }

        unsigned int id = desc[S3L_DESC_W_ID];
        if (!s3l_vid_cb || id >= S3L_VENC_CHN_NUM || !s3l_state[id].enable) continue;

        struct timespec ts;
        clock_gettime(CLOCK_MONOTONIC, &ts);

        hal_vidstream outStrm;
        hal_vidpack outPack[1];
        memset(outPack, 0, sizeof(outPack));
        outStrm.count = 1;
        outStrm.seq = desc[S3L_DESC_W_FRAME_NUM];
        outStrm.pack = outPack;
        outPack[0].data = data;
        outPack[0].length = size;
        outPack[0].offset = 0;
        outPack[0].timestamp = ts.tv_sec * 1000000ull + ts.tv_nsec / 1000;

        unsigned int n = 0, sc, scanOff = 0;
        while (desc[S3L_DESC_W_STREAM_TYPE] == S3L_STREAM_TYPE_H264 &&
               n < 7 && (sc = nal_find_startcode(data, scanOff, size)) < size) {
            unsigned int scLen = (data[sc + 2] == 1) ? 3 : 4;
            if (sc + scLen >= size) break;
            outPack[0].nalu[n].type = data[sc + scLen] & 0x1F;
            outPack[0].nalu[n].offset = sc;
            n++;
            scanOff = sc + scLen;
        }
        outPack[0].naluCnt = n;
        outPack[0].nalu[n].offset = size;
        for (unsigned int i = 0; i < n; i++)
            outPack[0].nalu[i].length = outPack[0].nalu[i + 1].offset - outPack[0].nalu[i].offset;

        (*s3l_vid_cb)(id, &outStrm);
    }

    free(wrapBuf);
    HAL_INFO("s3l_venc", "Shutting down encoding thread...\n");
    return NULL;
}

void s3l_system_deinit(void)
{
}

int s3l_system_init(char *snrConfig)
{
    unsigned int state = s3l_state_get();
    snprintf(_s3l_isp_path, sizeof(_s3l_isp_path), "%s", snrConfig ? snrConfig : "");
    if (state == 0xdeadbeef)
        HAL_ERROR("s3l_iav", "Cannot read the IAV state; are the IAV modules and DSP ucode loaded?\n");
    HAL_INFO("s3l_iav", "IAV state %#x\n", state);

    unsigned char info[60] = { 0 };
    if (!ioctl(_s3l_fd, S3L_IOC_VIN_GET_DEVINFO, info) && info[8])
        snprintf(sensor, sizeof(sensor), "%.*s", (int)sizeof(sensor) - 1, (char *)info + 8);
    return EXIT_SUCCESS;
}

int s3l_system_identify(void)
{
    FILE *file;
    char line[64], soc[16] = "";

    if (access("/dev/iav", F_OK) || access("/dev/ucode", F_OK))
        return 0;
    if (!(file = fopen("/proc/ambarella/iav", "r")))
        return 0;
    while (fgets(line, sizeof(line), file))
        if (sscanf(line, "AMBARELLA_CHIP=%15s", soc) == 1)
            break;
    fclose(file);
    if (strncmp(soc, "S3L", 3))
        return 0;

    plat = HAL_PLATFORM_S3L;
    strcpy(family, "ambarella");
    strcpy(chip, soc);

    chnCount = S3L_VENC_CHN_NUM;
    chnState = (hal_chnstate*)s3l_state;
    vid_thread = s3l_video_thread;
    return 1;
}

#endif
