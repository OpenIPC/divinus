#if defined(__arm__) && !defined(__ARM_PCS_VFP) && __ARM_ARCH == 6

#include "f2_hal.h"

f2_sys_impl  f2_sys;
f2_isp_impl  f2_isp;
f2_vpss_impl f2_vpss;
f2_venc_impl f2_venc;
f2_osd_impl  f2_osd;
f2_aud_impl  f2_aud;
f2_snr_impl  f2_snr;

hal_chnstate f2_state[F2_VENC_CHN_NUM] = {0};
int (*f2_aud_cb)(hal_audframe*);
int (*f2_vid_cb)(char, hal_vidstream*);

unsigned int _f2_aud_frame;
char _f2_aud_on;
char _f2_gray_want;
int _f2_isp_busy;
int _f2_isp_on;
f2_osd_rgn _f2_osd[F2_OSD_MAX];
f2_osd_logo _f2_osd_logo_desc;
int (*_f2_osd_logo_fn)(void *);
unsigned short *_f2_osd_virt;
unsigned char _f2_sat_colour[256];
char _f2_sat_ok;
char _f2_smartir;
f2_common_dim _f2_snr_dim;
int _f2_vpss_count = F2_VPSS_CHN_NUM;
f2_common_dim _f2_vpss_dim[F2_VPSS_CHN_NUM];
char _f2_vpss_open[F2_VPSS_CHN_NUM];
signed char _f2_vpss_src[F2_VENC_CHN_NUM];
f2_common_dim _f2_want_dim[F2_VENC_CHN_NUM];

void f2_hal_deinit(void) {
    f2_aud_unload(&f2_aud);
    f2_osd_unload(&f2_osd);
    f2_venc_unload(&f2_venc);
    f2_vpss_unload(&f2_vpss);
    f2_isp_unload(&f2_isp);
    f2_sys_unload(&f2_sys);
}

int f2_hal_init(void) {
    int ret;

    if (ret = f2_sys_load(&f2_sys))
        return ret;
    if (ret = f2_isp_load(&f2_isp))
        return ret;
    if (ret = f2_vpss_load(&f2_vpss))
        return ret;
    if (ret = f2_venc_load(&f2_venc))
        return ret;
    if (f2_osd_load(&f2_osd, f2_sys.handleVmm))
        HAL_WARNING("f2_osd", "Overlay library unavailable, OSD disabled\n");
    if (f2_aud_load(&f2_aud))
        HAL_WARNING("f2_aud", "Audio library unavailable, audio disabled\n");

    return EXIT_SUCCESS;
}

void f2_audio_deinit(void) {
    if (!_f2_aud_on) return;
    _f2_aud_on = 0;
    f2_aud.fnDisable();
    f2_aud.fnDeinit();
}

int f2_audio_init(int samplerate) {
    int ret;

    if (!f2_aud.handle)
        HAL_ERROR("f2_aud", "Audio library is not loaded!\n");

    if (ret = f2_aud.fnInit())
        return ret;

    if (!samplerate) samplerate = 8000;
    _f2_aud_frame = samplerate / 25;
    {
        f2_aud_cnf config = { .io = 0, .rate = samplerate, .bit = 16,
            .fmt = 0, .chnNum = 1, .frmNum = _f2_aud_frame, .volume = 85 };
        if (ret = f2_aud.fnSetConfig(&config))
            return ret;
    }
    if (ret = f2_aud.fnEnable())
        return ret;
    f2_aud.fnSetMicVolume(2);
    f2_aud.fnSetVolume(85);

    _f2_aud_on = 1;
    return EXIT_SUCCESS;
}

void *f2_audio_thread(void) {
    unsigned char *buf = malloc(_f2_aud_frame * 2 + 64);
    unsigned int seq = 0;

    while (keepRunning && audioOn && _f2_aud_on) {
        f2_aud_frm frame = { .length = 0, .data = buf };
        unsigned long long pts = 0;
        int ret = f2_aud.fnGetFrame(&frame, &pts);
        if (ret || !frame.length) {
            usleep(5000);
            continue;
        }
        if (f2_aud_cb) {
            hal_audframe outFrame;
            memset(&outFrame, 0, sizeof(outFrame));
            outFrame.channelCnt = 1;
            outFrame.data[0] = buf;
            outFrame.length[0] = frame.length;
            outFrame.seq = seq++;
            outFrame.timestamp = pts / 1000;
            (f2_aud_cb)(&outFrame);
        }
    }
    free(buf);
    HAL_INFO("f2_aud", "Shutting down capture thread...\n");
    return NULL;
}

static char f2_vpss_pick(char index) {
    for (char i = 0; i < _f2_vpss_count; i++)
        if (_f2_vpss_open[i] && _f2_vpss_dim[i].width == _f2_want_dim[index].width &&
            _f2_vpss_dim[i].height == _f2_want_dim[index].height)
            return i;
    return 0;
}

int f2_channel_bind(char index) {
    int ret;
    char src = _f2_vpss_src[index] < 0 ? f2_vpss_pick(index) : _f2_vpss_src[index];

    for (int i = 0; i < 5; i++) {
        if (!(ret = f2_sys.fnBind(src, index)))
            return EXIT_SUCCESS;
        usleep(100000);
    }
    HAL_DANGER("f2_venc", "Binding the encoder channel %d failed with %#x!\n", index, ret);
    return ret;
}

static int f2_vpss_setup(char index, short width, short height, char framerate) {
    static f2_common_dim made[F2_VPSS_CHN_NUM];
    static char fps[F2_VPSS_CHN_NUM];
    int ret;

    if (_f2_vpss_open[index] && _f2_vpss_dim[index].width == width &&
        _f2_vpss_dim[index].height == height && fps[index] == framerate)
        return EXIT_SUCCESS;

    if (_f2_vpss_open[index]) {
        f2_vpss.fnDisableChannel(index);
        _f2_vpss_open[index] = 0;
    }

    {
        f2_common_dim dim = { .width = width, .height = height };
        if (made[index].width) {
            if (dim.width > made[index].width || dim.height > made[index].height)
                HAL_ERROR("f2_vpss", "Channel %d was sized %ux%u; %ux%u needs a restart\n", index,
                    made[index].width, made[index].height, dim.width, dim.height);
        } else {
            if (f2_vpss.fnInitChannelMemory &&
                (ret = f2_vpss.fnInitChannelMemory(index, dim.width, dim.height)))
                HAL_WARNING("f2_vpss", "Allocating channel %d memory failed with %#x!\n", index, ret);
            else made[index] = dim;
        }
        if (ret = f2_vpss.fnSetChannelConfig(index, &dim))
            return ret;
        _f2_vpss_dim[index] = dim;
    }

    {
        f2_vpss_framectrl ctrl = { .srcFps = framerate, .dstFps = 1 };
        if (ret = f2_vpss.fnSetFrameControl(index, &ctrl))
            return ret;
        fps[index] = framerate;
    }

    if (ret = f2_vpss.fnEnableChannel(index))
        return ret;
    _f2_vpss_open[index] = 1;

    return EXIT_SUCCESS;
}

int f2_channel_create(char index, short width, short height, char framerate, char jpeg) {
    if (index >= F2_VENC_CHN_NUM)
        HAL_ERROR("f2_venc", "Only %d encoder channels are available!\n", F2_VENC_CHN_NUM);

    _f2_want_dim[index].width = width;
    _f2_want_dim[index].height = height;
    if (jpeg || index >= _f2_vpss_count) {
        _f2_vpss_src[index] = -1;
        return EXIT_SUCCESS;
    }
    _f2_vpss_src[index] = index;

    if (width > _f2_snr_dim.width || height > _f2_snr_dim.height) {
        HAL_WARNING("f2_hal", "Channel %d: requested %dx%d too large, using sensor %dx%d\n",
            index, width, height, _f2_snr_dim.width, _f2_snr_dim.height);
        width = _f2_snr_dim.width;
        height = _f2_snr_dim.height;
    }

    return f2_vpss_setup(index, width, height, framerate);
}

static void f2_grayscale_apply(void) {
    unsigned char zero[32] = {0};
    if (_f2_gray_want)
        f2_isp.fnSetSaturation(zero);
    else if (_f2_sat_ok)
        f2_isp.fnSetSaturation(_f2_sat_colour);
}

int f2_channel_grayscale(char enable) {
    _f2_gray_want = enable ? 1 : 0;
    f2_grayscale_apply();
    return EXIT_SUCCESS;
}

int f2_channel_unbind(char index) {
    f2_sys.fnUnbind(index);
    return EXIT_SUCCESS;
}

int f2_config_load(char *path) {
    static unsigned char param[8192];
    FILE *file;
    size_t len;

    if (!(file = fopen(path, "rb")))
        return EXIT_FAILURE;
    memset(param, 0, sizeof(param));
    len = fread(param, 1, sizeof(param), file);
    fclose(file);
    if (len < 0x2c)
        return EXIT_FAILURE;

    {
        int lr = f2_isp.fnLoadParam(param);
        if (lr)
            return EXIT_FAILURE;
    }
    {
        unsigned char sat[256] = {0};
        int gs = f2_isp.fnGetSaturation(sat);
        if (!gs && sat[5]) {
            memcpy(_f2_sat_colour, sat, sizeof(_f2_sat_colour));
            _f2_sat_ok = 1;
            f2_grayscale_apply();
        } else
            HAL_WARNING("f2_isp", "Can't read the saturation record\n");
    }
    return EXIT_SUCCESS;
}

void *f2_image_thread(void) {
    while (keepRunning) {
        if (_f2_isp_on) {
            _f2_isp_busy = 1;
            f2_isp.fnRun();
            _f2_isp_busy = 0;
            usleep(10000);
        } else usleep(100000);
    }
    HAL_INFO("f2_isp", "Shutting down ISP thread...\n");
    return NULL;
}

static void f2_proc_write(const char *path, const char *value) {
    FILE *file = fopen(path, "w");
    if (!file) {
        HAL_WARNING("f2_hal", "Cannot write %s to %s!\n", value, path);
        return;
    }
    fputs(value, file);
    fclose(file);
}

int f2_night_available(void) {
    return _f2_smartir;
}

int f2_night_status(void) {
    static char prev;
    if (!_f2_smartir) return 0;
    prev = f2_isp.fnSmartIrStatus(prev) ? 1 : 0;
    return prev;
}

int f2_night_thresholds(unsigned short night, unsigned short day) {
    (void)night; (void)day;
    return EXIT_FAILURE;
}


int f2_pipeline_create(short width, short height, char mirror, char flip, char framerate, char antiflicker) {
    int snrFmt;
    int ret;
    char path[128];

    /* The drivers take their buffer sizes before the SDK starts. */
    {
        char value[64];
        snprintf(value, sizeof(value), "vi_%u_%u", _f2_snr_dim.width, _f2_snr_dim.height);
        f2_proc_write("/proc/driver/vpu", value);
        for (int i = 0; i < F2_VPSS_CHN_NUM; i++) {
            snprintf(value, sizeof(value), "cap_%d_0_0", i);
            f2_proc_write("/proc/driver/vpu", value);
            snprintf(value, sizeof(value), "buf_%d_2", i);
            f2_proc_write("/proc/driver/vpu", value);
        }
        f2_proc_write("/proc/driver/vpu", "support4k_off");
        f2_proc_write("/proc/driver/isp", "support4k_off");
        f2_proc_write("/proc/driver/vpu", "sublimit_off");
        f2_proc_write("/proc/driver/isp", "sublimit_off");
        f2_proc_write("/proc/driver/isp", "wdr_off");
        f2_proc_write("/proc/driver/isp", "cir_off");
        f2_proc_write("/proc/driver/enc", "stm_2621440");
        snprintf(value, sizeof(value), "mem_1_%u", ALIGN_UP((unsigned int)width * height / 2 + 8, 4096));
        f2_proc_write("/proc/driver/jpeg", value);
        {
            unsigned int jpgSize = 512 * 1024;
            snprintf(value, sizeof(value), "mjpg_%u_%u", jpgSize * 3, jpgSize);
            f2_proc_write("/proc/driver/jpeg", value);
        }
    }

    if (ret = f2_sys.fnInit())
        return ret;

    if (f2_vpss.fnGetChannelCapability) {
        unsigned int cap[8];
        _f2_vpss_count = 0;
        while (_f2_vpss_count < F2_VPSS_CHN_NUM && !f2_vpss.fnGetChannelCapability(_f2_vpss_count, cap))
            _f2_vpss_count++;
        if (!_f2_vpss_count) _f2_vpss_count = 1;
    }

    if (ret = f2_vpss.fnSetInputConfig(&_f2_snr_dim))
        return ret;
    if (ret = f2_vpss.fnEnable(0))
        return ret;

    /* The ISP only streams once a VPSS channel is open */
    if (width > _f2_snr_dim.width) width = _f2_snr_dim.width;
    if (height > _f2_snr_dim.height) height = _f2_snr_dim.height;
    if (ret = f2_vpss_setup(0, width, height, framerate)) {
        HAL_DANGER("f2_vpss", "Operation failed with %#x!\n", ret);
        return ret;
    }

    f2_snr.fnCreate();

    if (f2_isp.fnAdvInit) {
        int ar = f2_isp.fnAdvInit();
    }
    if (f2_isp.fnAdvSensorInit) {
        int sr = f2_isp.fnAdvSensorInit();
    }

    if (ret = f2_isp.fnMemInit(_f2_snr_dim.width, _f2_snr_dim.height)) {
        HAL_DANGER("f2_isp", "Operation failed with %#x!\n", ret);
        return ret;
    }
    /* Hand the sensor's callbacks to the ISP, then wake the sensor and the ISP
     * up in order: nothing streams until all of it has happened. */
    if (ret = f2_isp.fnRegisterSensor(0, f2_snr.ops)) {
        HAL_DANGER("f2_isp", "Operation failed with %#x!\n", ret);
        return ret;
    }
    if (ret = f2_isp.fnSensorInit()) {
        HAL_DANGER("f2_isp", "Operation failed with %#x!\n", ret);
        return ret;
    }

    snrFmt = F2_SNR_FMT;
    if (ret = f2_isp.fnSetSensorFormat(snrFmt)) {
        HAL_DANGER("f2_isp", "Operation failed with %#x!\n", ret);
        return ret;
    }
    if (ret = f2_isp.fnInit()) {
        HAL_DANGER("f2_isp", "Operation failed with %#x!\n", ret);
        return ret;
    }
    if (f2_isp.fnEnableSensor)
        f2_isp.fnEnableSensor();

    snprintf(path, sizeof(path), "/etc/sensors/%s_attr.hex", f2_snr.name);
    if (f2_config_load(path)) {
        snprintf(path, sizeof(path), "/usr/lib/sensors/params/%s_attr.hex", f2_snr.name);
        if (f2_config_load(path))
            HAL_WARNING("f2_isp", "Can't load the ISP config for %s\n", f2_snr.name);
    }

    if (mirror || flip)
        f2_isp.fnSetFlipMirrorEx(mirror ? 1 : 0, flip ? 1 : 0, 0);

    if (f2_isp.fnAeSendCommand) {
        unsigned int mode = antiflicker >= 60 ? 2 : antiflicker >= 50 ? 1 : 0;
        f2_isp.fnAeSendCommand(0x0a, &mode);
    }

    if (f2_isp.fnSmartIrInit && !f2_isp.fnSmartIrInit()) {
        _f2_smartir = f2_isp.fnSmartIrStatus != NULL;
    }
    fh_lamp_enable(HAL_LIGHT_NONE);

    _f2_isp_on = 1;

    return EXIT_SUCCESS;
}

void f2_pipeline_destroy(void) {
    _f2_isp_on = 0;
    for (int i = 0; i < 50 && _f2_isp_busy; i++)
        usleep(10000);

    f2_isp.fnExit();
    f2_isp.fnUnregisterSensor(0);
    if (f2_snr.fnDestroy)
        f2_snr.fnDestroy();
    f2_snr_unload(&f2_snr);

    for (int i = 0; i < F2_VPSS_CHN_NUM; i++) {
        if (_f2_vpss_open[i]) f2_vpss.fnDisableChannel(i);
    }
    f2_vpss.fnDisable(0);
}

/* One full-frame overlay plane; regions are just rectangles drawn into it. */
static int f2_region_setup(void) {
    static unsigned int osdPhys, osdSize;
    static char osdOn;
    f2_osd_logo logo;
    int ir, lr;
    struct { unsigned int phys, size; } out = { 0 };
    void *virt = NULL;

    if (osdOn) return EXIT_SUCCESS;
    if (!f2_osd.fnInit || !f2_osd.fnSetLogo || !f2_osd.fnAlloc)
        return EXIT_FAILURE;

    ir = f2_osd.fnInit((void *)1, 1, 0, 0);
    if (ir) return EXIT_FAILURE;

    osdSize = _f2_snr_dim.width * _f2_snr_dim.height * 2;
    if (f2_osd.fnAlloc(&out, &virt, "divinus-osd", "", osdSize) || !out.phys || !virt)
        return EXIT_FAILURE;
    osdPhys = out.phys;
    _f2_osd_virt = virt;
    memset(_f2_osd_virt, 0, osdSize);

    memset(&logo, 0, sizeof(logo));
    logo.enable = 1;
    logo.width = _f2_snr_dim.width;
    logo.height = _f2_snr_dim.height;
    logo.alpha = 0xff;
    logo.rectWidth = _f2_snr_dim.width;
    logo.rectHeight = _f2_snr_dim.height;
    logo.bitmap = (unsigned int)(unsigned long)_f2_osd_virt;
    logo.show = 1;
    _f2_osd_logo_desc = logo;
    _f2_osd_logo_fn = f2_osd.fnSetLogo;
    lr = f2_osd.fnSetLogo(&logo);
    if (lr) return EXIT_FAILURE;

    osdOn = 1;
    return EXIT_SUCCESS;
}

static void f2_region_clear(hal_rect rect) {
    unsigned int w;
    if (!_f2_osd_virt) return;
    if (rect.x >= _f2_snr_dim.width || rect.y >= _f2_snr_dim.height) return;
    w = rect.width;
    if ((unsigned int)rect.x + w > _f2_snr_dim.width)
        w = _f2_snr_dim.width - rect.x;
    for (unsigned int y = 0; y < rect.height; y++) {
        unsigned int py = (unsigned int)rect.y + y;
        if (py >= _f2_snr_dim.height) break;
        memset(_f2_osd_virt + py * _f2_snr_dim.width + rect.x, 0, w * 2);
    }
    if (_f2_osd_logo_fn)
        _f2_osd_logo_fn(&_f2_osd_logo_desc);
}

int f2_region_create(int *handle, hal_rect rect, short opacity) {
    int ret;
    if (ret = f2_region_setup())
        return ret;
    if (*handle < 0 || *handle >= F2_OSD_MAX) {
        for (int i = 0; i < F2_OSD_MAX; i++)
            if (!_f2_osd[i].used) { *handle = i; break; }
        if (*handle < 0 || *handle >= F2_OSD_MAX)
            HAL_ERROR("f2_osd", "No free overlay region!\n");
    }
    if (_f2_osd[*handle].used)
        f2_region_clear(_f2_osd[*handle].rect);
    _f2_osd[*handle].used = 1;
    _f2_osd[*handle].rect = rect;
    _f2_osd[*handle].alpha = opacity < 0 ? 0 : (opacity > 255 ? 255 : (unsigned char)opacity);
    return EXIT_SUCCESS;
}

void f2_region_destroy(int *handle) {
    if (*handle < 0 || *handle >= F2_OSD_MAX || !_f2_osd[*handle].used) return;
    f2_region_clear(_f2_osd[*handle].rect);
    _f2_osd[*handle].used = 0;
    *handle = -1;
}

int f2_region_setbitmap(int *handle, hal_bitmap *bitmap) {
    if (*handle < 0 || *handle >= F2_OSD_MAX || !_f2_osd[*handle].used || !_f2_osd_virt)
        return EXIT_FAILURE;
    f2_osd_rgn *rgn = &_f2_osd[*handle];
    unsigned int bw = bitmap->dim.width, bh = bitmap->dim.height;
    unsigned short *src = bitmap->data;

    f2_region_clear(rgn->rect);
    if (rgn->rect.x >= _f2_snr_dim.width || rgn->rect.y >= _f2_snr_dim.height)
        return EXIT_SUCCESS;
    if (!bw || !bh) return EXIT_SUCCESS;

    rgn->rect.width = bw;
    rgn->rect.height = bh;
    for (unsigned int y = 0; y < bh; y++) {
        unsigned int py = (unsigned int)rgn->rect.y + y;
        if (py >= _f2_snr_dim.height) break;
        unsigned short *dst = _f2_osd_virt + py * _f2_snr_dim.width + rgn->rect.x;
        for (unsigned int x = 0; x < bw && (unsigned int)rgn->rect.x + x < _f2_snr_dim.width; x++) {
            unsigned short p = src[y * bw + x];
            dst[x] = p ? (p | 0x8000) : 0;
        }
    }
    if (_f2_osd_logo_fn)
        _f2_osd_logo_fn(&_f2_osd_logo_desc);
    return EXIT_SUCCESS;
}

int f2_video_create(char index, hal_vidconfig *config) {
    static f2_common_dim made[F2_VENC_CHN_NUM];
    int ret;
    unsigned int type;

    if (index >= F2_VENC_CHN_NUM)
        HAL_ERROR("f2_venc", "Only %d encoder channels are available!\n", F2_VENC_CHN_NUM);

    switch (config->codec) {
        case HAL_VIDCODEC_JPG:  type = F2_VENC_CODEC_JPEG; break;
        case HAL_VIDCODEC_MJPG: type = F2_VENC_CODEC_MJPEG; break;
        case HAL_VIDCODEC_H264: type = F2_VENC_CODEC_H264; break;
        case HAL_VIDCODEC_H265: type = F2_VENC_CODEC_H265; break;
        default: HAL_ERROR("f2_venc", "This codec is not supported by the hardware!\n");
    }

    if (config->width > _f2_snr_dim.width || config->height > _f2_snr_dim.height) {
        config->width = _f2_snr_dim.width;
        config->height = _f2_snr_dim.height;
    }

    if (_f2_vpss_src[index] >= 0) {
        if (ret = f2_vpss.fnSetOutputMode(index, type == F2_VENC_CODEC_H265 ? 1 : 0))
            HAL_DANGER("f2_vpss", "Setting output mode on channel %d failed with %#x!\n", index, ret);
    } else {
        char src = f2_vpss_pick(index);
        config->width = _f2_vpss_dim[src].width;
        config->height = _f2_vpss_dim[src].height;
    }

    if (made[index].width) {
        if (config->width > made[index].width || config->height > made[index].height)
            HAL_ERROR("f2_venc", "Channel %d was created at %ux%u; %ux%u needs a restart\n", index,
                made[index].width, made[index].height, config->width, config->height);
    } else {
        f2_venc_cap cfg = { .types = (type & (F2_VENC_CODEC_H264 | F2_VENC_CODEC_H265)) ?
            (F2_VENC_CODEC_H264 | F2_VENC_CODEC_H265) : type, .width = config->width, .height = config->height };
        if (ret = f2_venc.fnCreateChannel(index, &cfg)) {
            HAL_DANGER("f2_venc", "Creating channel %d (%ux%u) failed with %#x!\n", index, cfg.width, cfg.height, ret);
            return ret;
        }
        made[index].width = cfg.width;
        made[index].height = cfg.height;
    }

    if (type == F2_VENC_CODEC_JPEG) {
        f2_venc_jpgattr attr;
        memset(&attr, 0, sizeof(attr));
        attr.codec = type;
        attr.quality = MAX(config->maxQual, 1);
        attr.rateIndex = 4;
        if (ret = f2_venc.fnSetChannelConfig(index, &attr))
            return ret;
    } else if (type == F2_VENC_CODEC_MJPEG) {
        /* MJPEG reuses the JPEG encoder, so it takes the JPEG attribute. */
        f2_venc_mjpegattr attr;
        memset(&attr, 0, sizeof(attr));
        attr.codec = type;
        attr.width = config->width;
        attr.height = config->height;
        attr.encSpeed = 4;
        attr.rotate = 0;
        attr.mode = 0;
        attr.qp = MAX(config->maxQual, 1);
        attr.frameNum = 1;
        attr.frameTime = config->framerate ? config->framerate : 25;
        if (ret = f2_venc.fnSetChannelConfig(index, &attr)) {
            HAL_DANGER("f2_venc", "Configuring MJPEG channel %d failed with %#x!\n", index, ret);
            return ret;
        }
    } else {
        f2_venc_attr attr;
        f2_venc_rc *rate;
        int h265 = type == F2_VENC_CODEC_H265;

        memset(&attr, 0, sizeof(attr));
        attr.codec = type;
        if (h265)
            attr.profile = 1;
        else {
            if (config->profile != HAL_VIDPROFILE_MAIN)
                HAL_WARNING("f2_venc", "Channel %d: only the H.264 Main profile is supported, using it\n", index);
            attr.profile = 77;
        }
        attr.gop = config->gop ? config->gop : config->framerate * 2;
        attr.width = config->width;
        attr.height = config->height;

        rate = &attr.rate;
        memset(rate, 0, sizeof(*rate));
        switch (config->mode) {
            case HAL_VIDMODE_QP:
                if (h265) goto cbr;
                rate->mode = F2_VENC_RATEMODE_H264FIXQP;
                rate->param[0] = config->maxQual;
                rate->param[1] = config->maxQual;
                rate->param[2] = (unsigned int)config->framerate | (1u << 16);
                break;
            case HAL_VIDMODE_VBR:
            case HAL_VIDMODE_ABR:
            case HAL_VIDMODE_AVBR:
                rate->mode = h265 ? F2_VENC_RATEMODE_H265VBR : F2_VENC_RATEMODE_H264VBR;
                rate->param[0] = 35;
                rate->param[1] = (unsigned int)MAX(config->bitrate, config->maxBitrate) << 10;
                rate->param[2] = (unsigned int)config->framerate | (1u << 16);
                rate->param[3] = 120;
                rate->param[6] = 5;
                rate->param[7] = 1;
                break;
            default:
cbr:
                rate->mode = h265 ? F2_VENC_RATEMODE_H265CBR : F2_VENC_RATEMODE_H264CBR;
                rate->param[0] = 35;
                rate->param[1] = (unsigned int)config->bitrate << 10;
                rate->param[2] = 28;
                rate->param[3] = 50;
                rate->param[4] = 35;
                rate->param[5] = 50;
                rate->param[6] = (unsigned int)config->framerate | (1u << 16);
                rate->param[7] = 120;
                rate->param[10] = 5;
                rate->param[11] = 1;
                break;
        }

        if (ret = f2_venc.fnSetChannelConfig(index, &attr))
            return ret;
    }


    if (type != F2_VENC_CODEC_JPEG && (ret = f2_venc.fnStartReceiving(index))) {
        HAL_DANGER("f2_venc", "Starting channel %d failed with %#x!\n", index, ret);
        return ret;
    }

    f2_state[index].payload = config->codec;

    return EXIT_SUCCESS;
}

int f2_video_destroy(char index) {
    f2_state[index].enable = 0;
    f2_state[index].payload = HAL_VIDCODEC_UNSPEC;

    f2_venc.fnStopReceiving(index);
    f2_sys.fnUnbind(index);

    return EXIT_SUCCESS;
}

int f2_video_destroy_all(void) {
    for (char i = 0; i < F2_VENC_CHN_NUM; i++)
        if (f2_state[i].enable)
            f2_video_destroy(i);

    return EXIT_SUCCESS;
}

void f2_video_request_idr(char index) {
    f2_venc.fnRequestIdr(index);
}

int f2_video_snapshot_grab(signed char index, hal_jpegdata *jpeg) {
    static pthread_mutex_t mjpegMtx = PTHREAD_MUTEX_INITIALIZER;
    static unsigned char *mjpegBuf;
    static unsigned int mjpegLen, mjpegCap;
    int ret = EXIT_FAILURE;
    f2_venc_strm strm;
    unsigned int type = F2_VENC_CODEC_JPEG;

    if (index < 0) {
        for (int w = 0; w < 100; w++) {
            pthread_mutex_lock(&mjpegMtx);
            if (mjpegLen) {
                if (mjpegLen > jpeg->length) {
                    unsigned char *grown = realloc(jpeg->data, mjpegLen);
                    if (!grown) {
                        pthread_mutex_unlock(&mjpegMtx);
                        HAL_ERROR("f2_venc", "Growing the snapshot buffer to %u bytes failed!\n", mjpegLen);
                    }
                    jpeg->data = grown;
                    jpeg->length = mjpegLen;
                }
                memcpy(jpeg->data, mjpegBuf, mjpegLen);
                jpeg->jpegSize = mjpegLen;
                pthread_mutex_unlock(&mjpegMtx);
                return EXIT_SUCCESS;
            }
            pthread_mutex_unlock(&mjpegMtx);
            usleep(20000);
        }
        HAL_ERROR("f2_venc", "No MJPEG frame available for snapshot!\n");
    } else if (f2_state[index].payload != HAL_VIDCODEC_JPG)
        HAL_ERROR("f2_venc", "Channel %d is not a JPEG encoder!\n", index);

    pthread_mutex_lock(&mjpegMtx);

    {
        char src = _f2_vpss_src[index] < 0 ? f2_vpss_pick(index) : _f2_vpss_src[index];
        if (ret = f2_sys.fnBind(src, index)) {
            HAL_DANGER("f2_venc", "Binding the encoder channel %d failed with %#x!\n", index, ret);
            goto fallback;
        }
    }

    for (int i = 0; i < 100; i++) {
        memset(&strm, 0, sizeof(strm));
        strm.type = type;
        if (!(ret = f2_venc.fnGetStream(type, &strm)))
            break;
        usleep(20000);
    }
    if (ret) {
        HAL_DANGER("f2_venc", "Getting a JPEG frame timed out (%#x)!\n", ret);
        goto fallback;
    }

    {
        unsigned char *data = (unsigned char*)strm.field08;
        unsigned int len = strm.field0c;
        if (len > jpeg->length) {
            unsigned char *grown = realloc(jpeg->data, len);
            if (!grown) {
                HAL_DANGER("f2_venc", "Growing the snapshot buffer to %u bytes failed!\n", len);
                f2_venc.fnFreeStream(strm.field04);
                ret = EXIT_FAILURE;
                goto abort;
            }
            jpeg->data = grown;
            jpeg->length = len;
        }
        memcpy(jpeg->data, data, len);
        jpeg->jpegSize = len;
    }
    f2_venc.fnFreeStream(strm.field04);
    ret = EXIT_SUCCESS;

abort:
    f2_sys.fnUnbind(index);
    pthread_mutex_unlock(&mjpegMtx);
    return ret;

fallback:
    f2_sys.fnUnbind(index);
    pthread_mutex_unlock(&mjpegMtx);
    HAL_WARNING("f2_venc", "Falling back to the MJPEG sub-stream for the snapshot\n");
    return f2_video_snapshot_grab(-1, jpeg);
}

void *f2_video_thread(void) {
    unsigned int mask = F2_VENC_CODEC_H264 | F2_VENC_CODEC_H265 | F2_VENC_CODEC_MJPEG;
    f2_venc_strm strm;
    int logged = 0;

    while (keepRunning) {
        char active = 0;
        for (int i = 0; i < F2_VENC_CHN_NUM; i++)
            if (f2_state[i].enable && f2_state[i].mainLoop) active = 1;
        if (!active) {
            usleep(100000);
            continue;
        }

        memset(&strm, 0, sizeof(strm));
        strm.type = mask;
        if (f2_venc.fnGetStream(mask, &strm)) {
            usleep(10000);
            continue;
        }

        char index = strm.field04;
        if (strm.type == F2_VENC_CODEC_MJPEG) {
            strm.nalu[0].type = 0;
            strm.nalu[0].length = strm.field0c;
            strm.nalu[0].addr = strm.field08;
            strm.naluCnt = 1;
        }
        unsigned int count = strm.naluCnt;

        if (!logged) {
            logged = 1;
        }

        int copied = 0;
        {
            static unsigned char *vbuf; static unsigned int vcap;
            unsigned int need = 0, off = 0;
            for (unsigned int i = 0; i < count && i < 28; i++) need += strm.nalu[i].length;
            if (need) {
                if (need > vcap) {
                    unsigned char *n = realloc(vbuf, need + 4096);
                    if (n) { vbuf = n; vcap = need + 4096; }
                }
                if (vbuf && need <= vcap) {
                    for (unsigned int i = 0; i < count && i < 28; i++) {
                        memcpy(vbuf + off, (void*)strm.nalu[i].addr, strm.nalu[i].length);
                        strm.nalu[i].addr = (unsigned int)(vbuf + off);
                        off += strm.nalu[i].length;
                    }
                    copied = 1;
                }
            }
        }
        f2_venc.fnFreeStream(strm.field04);

        if (copied && index < F2_VENC_CHN_NUM && f2_state[index].enable && f2_vid_cb) {
            hal_vidstream outStrm;
            hal_vidpack outPack[28];
            unsigned int n = count > 28 ? 28 : count;
            memset(outPack, 0, sizeof(outPack));
            outStrm.count = n;
            outStrm.seq = 0;
            outStrm.pack = outPack;
            for (unsigned int i = 0; i < n; i++) {
                outPack[i].data = (unsigned char*)strm.nalu[i].addr;
                outPack[i].length = strm.nalu[i].length;
                outPack[i].timestamp = 0;
                outPack[i].nalu[0].type = strm.nalu[i].type;
                outPack[i].nalu[0].offset = 0;
                outPack[i].nalu[0].length = strm.nalu[i].length;
                outPack[i].naluCnt = 1;
            }
            (*f2_vid_cb)(index, &outStrm);
        }
    }
    HAL_INFO("f2_venc", "Shutting down encoding thread...\n");
    return NULL;
}

void f2_system_deinit(void) {
    f2_sys.fnExit();
}

/* Sensors are found by scanning the sensor directory, or by $SENSOR. */
static int f2_sensor_select(void) {
    static const char *dirs[] = { "/usr/lib/sensors", "/lib/sensors" };
    static char names[F2_SNR_MAX][32];
    static int foundNum;
    const char *name = getenv("SENSOR");
    int found = 0;

    if (name && f2_snr_load(&f2_snr, name))
        name = NULL;
    if (!name) {
        for (unsigned int d = 0; d < sizeof(dirs) / sizeof(dirs[0]); d++) {
            struct dirent *entry;
            DIR *dir;
            if (!(dir = opendir(dirs[d]))) continue;
            while ((entry = readdir(dir)) && foundNum < F2_SNR_MAX) {
                size_t len = strlen(entry->d_name);
                if (len < 6 || strncmp(entry->d_name, "lib", 3) ||
                    strcmp(entry->d_name + len - 3, ".so"))
                    continue;
                snprintf(names[foundNum++], sizeof(names[0]),
                    "%.*s", (int)(len - 6), entry->d_name + 3);
            }
            closedir(dir);
        }
        for (int i = 0; i < foundNum && !found; i++) {
            found = !f2_snr_load(&f2_snr, names[i]);
            if (!found) f2_snr_unload(&f2_snr);
        }
        if (!found)
            return EXIT_FAILURE;
    }

    for (unsigned int i = 0; i < sizeof(f2_snrs) / sizeof(*f2_snrs); i++) {
        if (strcmp(f2_snrs[i].name, f2_snr.name)) continue;
        _f2_snr_dim = f2_snrs[i].dim;
        strncpy(sensor, f2_snr.name, sizeof(sensor) - 1);
        return EXIT_SUCCESS;
    }

    HAL_ERROR("f2_hal", "Unknown sensor %s, please update the app!\n", f2_snr.name);
}

int f2_system_init(void) {
    if (f2_sensor_select())
        HAL_ERROR("f2_snr", "Failed to load the sensor driver\n");

    return EXIT_SUCCESS;
}

#endif
