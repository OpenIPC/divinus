#if defined(__arm__) && !defined(__ARM_PCS_VFP) && __ARM_ARCH == 6

#include "f1_hal.h"

f1_aud_impl  f1_aud;
f1_isp_impl  f1_isp;
f1_sys_impl  f1_sys;
f1_venc_impl f1_venc;
f1_vpss_impl f1_vpss;

hal_chnstate f1_state[F1_VENC_CHN_NUM] = {0};
int (*f1_aud_cb)(hal_audframe*);
int (*f1_vid_cb)(char, hal_vidstream*);

f1_snr_impl f1_snr;
static f1_common_dim _f1_snr_dim;
static char _f1_isp_on, _f1_isp_busy;
static unsigned int _f1_venc_codec[F1_VENC_CHN_NUM];
static f1_common_dim _f1_venc_made[F1_VENC_CHN_NUM]; /* size FH_VENC_CreateChn reserved */
static f1_common_dim _f1_vpss_made[F1_VPSS_CHN_NUM]; /* size FH_VPSS_ChnInitMem reserved */
static char _f1_vpss_open[F1_VPSS_CHN_NUM];
static f1_common_dim _f1_vpss_dim[F1_VPSS_CHN_NUM];
static char _f1_vpss_fps[F1_VPSS_CHN_NUM];
static char _f1_param_path[128];
static int _f1_vpss_count = F1_VPSS_CHN_NUM;
static f1_common_dim _f1_want_dim[F1_VENC_CHN_NUM];
static signed char _f1_vpss_src[F1_VENC_CHN_NUM];   /* scaler channel feeding each encoder */
static char f1_vpss_pick(char index);

/* Graphic overlay: one 16-bit ARGB1555 plane at sensor resolution, blended before scaling */
static f1_osd_rgn _f1_osd[F1_OSD_MAX];
static unsigned int _f1_osd_phys, _f1_osd_size;
static unsigned short *_f1_osd_virt;
static char _f1_osd_on;
static unsigned char _f1_osd_alpha = 255;
static pthread_mutex_t _f1_strm_mtx = PTHREAD_MUTEX_INITIALIZER;
static char _f1_aud_on;
static unsigned int _f1_aud_frame;
static char _f1_gain_night, _f1_night_prev;
static unsigned char *_f1_mjpeg_cache; static unsigned int _f1_mjpeg_len, _f1_mjpeg_cap; static pthread_mutex_t _f1_mjpeg_mtx = PTHREAD_MUTEX_INITIALIZER;

/* libc compatibility shims for the SDK libraries live in fh_compat.c */

static void f1_proc_write(const char *path, const char *value)
{
    FILE *file = fopen(path, "w");
    if (!file) {
        HAL_WARNING("f1_hal", "Cannot write %s to %s!\n", value, path);
        return;
    }
    fputs(value, file);
    fclose(file);
}

void f1_hal_deinit(void)
{
    f1_aud_unload(&f1_aud);
    f1_venc_unload(&f1_venc);
    f1_vpss_unload(&f1_vpss);
    f1_isp_unload(&f1_isp);
    f1_sys_unload(&f1_sys);
}

int f1_hal_init(void)
{
    int ret;

    if (ret = f1_sys_load(&f1_sys))
        return ret;
    if (ret = f1_isp_load(&f1_isp))
        return ret;
    if (ret = f1_vpss_load(&f1_vpss))
        return ret;
    if (ret = f1_venc_load(&f1_venc))
        return ret;
    if (f1_aud_load(&f1_aud))
        HAL_WARNING("f1_hal", "Audio library unavailable, audio disabled\n");

    return EXIT_SUCCESS;
}

void f1_audio_deinit(void)
{
    if (!_f1_aud_on) return;
    _f1_aud_on = 0;
    f1_aud.fnDisable();
    f1_aud.fnDeinit();
}

int f1_audio_init(int samplerate)
{
    int ret;

    if (!f1_aud.handle)
        HAL_ERROR("f1_aud", "Audio library is not loaded!\n");

    if (ret = f1_aud.fnInit()) {
        HAL_DANGER("f1_aud", "FH_AC_Init failed with %#x\n", ret);
        return ret;
    }

    {
        /* A sample rate the config parser rejected arrives here as 0 (a saved
         * "srate: 0" is outside the 8000..96000 range); fall back to 8 kHz rather
         * than asking the codec for zero-length frames */
        if (!samplerate) samplerate = 8000;
        /* 40 ms frames of 16-bit mono PCM, as the vendor application does */
        _f1_aud_frame = samplerate / 25;
        f1_aud_cnf config = { .io = 0, .rate = samplerate, .bit = 16,
            .fmt = 0, .chnNum = 1, .frmNum = _f1_aud_frame, .volume = 85 };
        if (ret = f1_aud.fnSetConfig(&config)) {
            HAL_DANGER("f1_aud", "FH_AC_Set_Config(%d Hz, %u samples) failed with %#x\n", samplerate, _f1_aud_frame, ret);
            return ret;
        }
    }
    if (ret = f1_aud.fnEnable()) {
        HAL_DANGER("f1_aud", "FH_AC_AI_Enable failed with %#x\n", ret);
        return ret;
    }
    f1_aud.fnSetMicVolume(2);
    f1_aud.fnSetVolume(85);

    _f1_aud_on = 1;
    return EXIT_SUCCESS;
}

void *f1_audio_thread(void)
{
    unsigned char *buf = malloc(_f1_aud_frame * 2 + 64);
    unsigned int seq = 0;

    while (keepRunning && audioOn && _f1_aud_on) {
        f1_aud_frm frame = { .length = 0, .data = buf };
        unsigned long long pts = 0;
        int ret = f1_aud.fnGetFrame(&frame, &pts);
        if (ret || !frame.length) {
            usleep(5000);
            continue;
        }
        if (f1_aud_cb) {
            hal_audframe outFrame;
            memset(&outFrame, 0, sizeof(outFrame));
            outFrame.channelCnt = 1;
            outFrame.data[0] = buf;
            outFrame.length[0] = frame.length;
            outFrame.seq = seq++;
            outFrame.timestamp = pts / 1000;
            (f1_aud_cb)(&outFrame);
        }
    }
    free(buf);
    HAL_INFO("f1_aud", "Shutting down capture thread...\n");
    return NULL;
}

int f1_channel_bind(char index)
{
    int ret;
    char src = _f1_vpss_src[index] < 0 ? f1_vpss_pick(index) : _f1_vpss_src[index];

    for (int i = 0; i < 5; i++) {
        if (!(ret = f1_sys.fnBindVpu2Enc(src, index)))
            return EXIT_SUCCESS;
        usleep(100000);
    }
    HAL_DANGER("f1_hal", "Binding scaler channel %d to encoder %d failed with %#x!\n", src, index, ret);
    return ret;
}

static int f1_vpss_setup(char index, short width, short height, char framerate)
{
    int ret;

    if (_f1_vpss_open[index] && _f1_vpss_dim[index].width == width &&
        _f1_vpss_dim[index].height == height && _f1_vpss_fps[index] == framerate)
        return EXIT_SUCCESS;

    if (_f1_vpss_open[index]) {
        f1_vpss.fnCloseChannel(index);
        _f1_vpss_open[index] = 0;
    }

    {
        f1_common_dim dim = { .width = width, .height = height };
        /* Like the encoder pools, a scaler channel's memory cannot be released
         * again (a second ChnInitMem fails with -1), so allocate once and keep
         * reconfiguring within that size */
        if (_f1_vpss_made[index].width) {
            if (dim.width > _f1_vpss_made[index].width || dim.height > _f1_vpss_made[index].height)
                HAL_ERROR("f1_hal", "Scaler channel %d was sized %ux%u; %ux%u needs a restart\n", index,
                    _f1_vpss_made[index].width, _f1_vpss_made[index].height, dim.width, dim.height);
        } else {
            if (f1_vpss.fnInitChannelMemory &&
                (ret = f1_vpss.fnInitChannelMemory(index, dim.width, dim.height)))
                HAL_WARNING("f1_hal", "Allocating scaler channel %d memory failed with %#x!\n", index, ret);
            else _f1_vpss_made[index] = dim;
        }
        if (ret = f1_vpss.fnSetChannelConfig(index, &dim))
            return ret;
        _f1_vpss_dim[index] = dim;
    }

    {
        f1_vpss_framectrl ctrl = { .srcRate = framerate, .dstRate = 1 };
        if (ret = f1_vpss.fnSetFrameControl(index, &ctrl))
            return ret;
        _f1_vpss_fps[index] = framerate;
    }

    if (ret = f1_vpss.fnOpenChannel(index))
        return ret;
    _f1_vpss_open[index] = 1;

    return EXIT_SUCCESS;
}

static char f1_vpss_pick(char index)
{
    /* Prefer an open scaler channel with the same size, else the main one */
    for (char i = 0; i < _f1_vpss_count; i++)
        if (_f1_vpss_open[i] && _f1_vpss_dim[i].width == _f1_want_dim[index].width &&
            _f1_vpss_dim[i].height == _f1_want_dim[index].height)
            return i;
    return 0;
}

int f1_channel_create(char index, short width, short height, char framerate, char jpeg)
{
    if (index >= F1_VENC_CHN_NUM)
        HAL_ERROR("f1_hal", "Only %d encoder channels are available!\n", F1_VENC_CHN_NUM);

    _f1_want_dim[index].width = width;
    _f1_want_dim[index].height = height;
    if (index >= _f1_vpss_count) {
        _f1_vpss_src[index] = -1;
        HAL_INFO("f1_hal", "Channel %d: no free scaler channel, it will share one at bind time\n", index);
        return EXIT_SUCCESS;
    }
    _f1_vpss_src[index] = index;

    if (width > _f1_snr_dim.width || height > _f1_snr_dim.height) {
        HAL_WARNING("f1_hal", "Channel %d: requested %dx%d too large, using sensor %dx%d\n",
            index, width, height, _f1_snr_dim.width, _f1_snr_dim.height);
        width = _f1_snr_dim.width;
        height = _f1_snr_dim.height;
    }

    return f1_vpss_setup(index, width, height, framerate);
}

/*
 * Colour vs monochrome is the ISP saturation record (20 bytes: enable word,
 * then the curve). The tuning file sets a valid colour record; a zeroed record
 * is monochrome (what the vendor's mono path writes). libadvapi's
 * FHAdv_Isp_SetColorMode() cannot be used here: its "restore colour" writes
 * the library's own copy, which is never filled on this build, so it zeroed
 * the record instead. Keep the record from the tuning load and write it back.
 */
static unsigned char _f1_sat_colour[32];
static char _f1_sat_ok, _f1_gray_want;

static void f1_grayscale_apply(void)
{
    unsigned char zero[32] = {0};
    if (_f1_gray_want)
        f1_isp.fnSetSaturation(zero);
    else if (_f1_sat_ok)
        f1_isp.fnSetSaturation(_f1_sat_colour);
    /* else: nothing captured yet, the ISP is still in its tuned colour state */
}

int f1_channel_grayscale(char enable)
{
    _f1_gray_want = enable ? 1 : 0;
    f1_grayscale_apply();
    return EXIT_SUCCESS;
}

int f1_channel_unbind(char index)
{
    f1_sys.fnUnbindByDst(index);

    return EXIT_SUCCESS;
}

/*
 * ISP tuning ("<sensor>_attr.hex", 3268 bytes): the vendor files are
 * generated for a newer SDK build; the older library only warns about the
 * version word and still loads them.
 */
int f1_config_load(char *path)
{
    static unsigned char param[4096];
    FILE *file;
    size_t len;

    if (EQUALS(path, _f1_param_path))
        return EXIT_SUCCESS;
    if (!(file = fopen(path, "rb")))
        return EXIT_FAILURE;
    memset(param, 0, sizeof(param));
    len = fread(param, 1, sizeof(param), file);
    fclose(file);
    if (len < 0x2c)
        return EXIT_FAILURE;

    HAL_INFO("f1_hal", "Loading ISP parameters from %s (%zu bytes)\n", path, len);
    if (f1_isp.fnLoadParam(param))
        return EXIT_FAILURE;
    {
        unsigned char sat[32] = {0};
        if (!f1_isp.fnGetSaturation(sat) && sat[5]) {
            memcpy(_f1_sat_colour, sat, sizeof(_f1_sat_colour));
            _f1_sat_ok = 1;
            f1_grayscale_apply();       /* honour a mode requested before the tuning load */
        } else
            HAL_WARNING("f1_hal", "Could not read the colour saturation record; grayscale off will be a no-op\n");
    }
    strncpy(_f1_param_path, path, sizeof(_f1_param_path) - 1);
    {
        int ret = f1_isp.fnAdvInit();
        return ret;
    }
}

void *f1_image_thread(void)
{
    while (keepRunning) {
        if (_f1_isp_on) {
            _f1_isp_busy = 1;
            f1_isp.fnRun();
            _f1_isp_busy = 0;
            usleep(10000);
        } else usleep(100000);
    }
    HAL_INFO("f1_hal", "Shutting down ISP thread...\n");
    return NULL;
}

/* Anti-banding: the ISP AE stores a 2-bit flicker mode at param offset 0x3a
 * bits 6-7 (0 off, 1 = 50 Hz, 2 = 60 Hz), set through AE command 0xa. The
 * newer vendor SDK adds dedicated commands 0x1b-0x1d, absent from these libs. */
static int f1_set_antiflicker(char hz)
{
    unsigned int mode = hz >= 60 ? 2 : hz >= 50 ? 1 : 0;
    if (!f1_isp.fnAeSendCmd)
        return EXIT_FAILURE;
    return f1_isp.fnAeSendCmd(0x0a, &mode);
}

static unsigned int f1_sensor_format(char framerate)
{
    if (framerate <= 15) return F1_SNR_FMT_15;
    if (framerate <= 20) return F1_SNR_FMT_20;
    if (framerate <= 25) return F1_SNR_FMT_25;
    return F1_SNR_FMT_30;
}

int f1_pipeline_create(short width, short height, char mirror, char flip, char framerate, char antiflicker)
{
    int ret;
    char path[128];

    /* Kernel-side buffer sizing must precede FH_SYS_Init() */
    {
        char value[64];
        snprintf(value, sizeof(value), "vi_%u_%u", _f1_snr_dim.width, _f1_snr_dim.height);
        f1_proc_write("/proc/driver/vpu", value);
        /* Scaler channel memory is allocated on demand in f1_channel_create() */
        for (int i = 0; i < F1_VPSS_CHN_NUM; i++) {
            snprintf(value, sizeof(value), "cap_%d_0_0", i);
            f1_proc_write("/proc/driver/vpu", value);
            snprintf(value, sizeof(value), "buf_%d_2", i);
            f1_proc_write("/proc/driver/vpu", value);
        }
        f1_proc_write("/proc/driver/vpu", "support4k_off");
        f1_proc_write("/proc/driver/isp", "support4k_off");
        f1_proc_write("/proc/driver/vpu", "sublimit_off");
        f1_proc_write("/proc/driver/isp", "sublimit_off");
        f1_proc_write("/proc/driver/isp", "wdr_off");
        f1_proc_write("/proc/driver/isp", "cir_off");
        f1_proc_write("/proc/driver/enc", "stm_2621440");
        f1_proc_write("/proc/driver/hevc", "stm_2621440");
        f1_proc_write("/proc/driver/hevc", "usebfrm_0");
        /* JPEG snapshot buffer: the driver's own rule is width*height/2, forcing 128 KiB
         * (the vendor value) overruns at 2560x1440 and crashes the JPEG thread */
        snprintf(value, sizeof(value), "mem_1_%u", ALIGN_UP((unsigned int)width * height / 2 + 8, 4096));
        f1_proc_write("/proc/driver/jpeg", value);
        /* Motion JPEG pool: three 512 KiB slots (larger pools fail to ioremap on 32 MiB systems) */
        {
            unsigned int jpgSize = 512 * 1024;
            snprintf(value, sizeof(value), "mjpg_%u_%u", jpgSize * 3, jpgSize);
            f1_proc_write("/proc/driver/jpeg", value);
        }
    }

    if (ret = f1_sys.fnInit())
        return ret;

    {
        unsigned int cap[8];
        _f1_vpss_count = 0;
        while (f1_vpss.fnGetChannelCapability && _f1_vpss_count < F1_VPSS_CHN_NUM &&
               !f1_vpss.fnGetChannelCapability(_f1_vpss_count, cap))
            _f1_vpss_count++;
        if (!_f1_vpss_count) _f1_vpss_count = 1;
        HAL_INFO("f1_hal", "%d scaler channels available\n", _f1_vpss_count);
    }

    if (ret = f1_vpss.fnSetInputConfig(&_f1_snr_dim))
        return ret;
    if (ret = f1_vpss.fnEnable(0))
        return ret;

    /* The ISP only starts delivering frames when a scaler channel is already open */
    if (width > _f1_snr_dim.width) width = _f1_snr_dim.width;
    if (height > _f1_snr_dim.height) height = _f1_snr_dim.height;
    if (ret = f1_vpss_setup(0, width, height, framerate))
        return ret;

    if (ret = f1_isp.fnMemInit(_f1_snr_dim.width, _f1_snr_dim.height))
        return ret;
    if (ret = f1_isp.fnRegisterSensor(0, f1_snr.ops))
        return ret;
    if (ret = f1_isp.fnSensorInit())
        return ret;
    if (ret = f1_isp.fnSetSensorFormat(f1_sensor_format(framerate)))
        return ret;
    if (ret = f1_isp.fnInit())
        return ret;

    snprintf(path, sizeof(path), "/etc/sensors/%s_attr.hex", f1_snr.name);
    if (f1_config_load(path)) {
        snprintf(path, sizeof(path), "/usr/lib/sensors/params/%s_attr.hex", f1_snr.name);
        if (f1_config_load(path))
            HAL_WARNING("f1_hal", "No ISP tuning file found for %s, image quality will suffer!\n",
                f1_snr.name);
    }

    if (mirror || flip)
        f1_isp.fnSetFlipMirrorEx(mirror ? 1 : 0, flip ? 1 : 0, 0);

    f1_set_antiflicker(antiflicker);

    /* Image-gain based day/night detection (no external light sensor needed) */
    if (f1_isp.fnGainNightInit && !f1_isp.fnGainNightInit()) {
        if (f1_isp.fnGainNightSetAttr)
            f1_isp.fnGainNightSetAttr(0);
        _f1_gain_night = f1_isp.fnGainNightStatus != NULL;
        HAL_INFO("f1_hal", "Image-gain day/night detection %s\n", _f1_gain_night ? "enabled" : "unavailable");
    }
    fh_lamp_enable(HAL_LIGHT_NONE);

    _f1_isp_on = 1;

    return EXIT_SUCCESS;
}

void f1_pipeline_destroy(void)
{
    _f1_isp_on = 0;
    for (int i = 0; i < 50 && _f1_isp_busy; i++)
        usleep(10000);

    f1_isp.fnExit();
    f1_isp.fnUnregisterSensor(0);
    if (f1_snr.fnDestroy)
        f1_snr.fnDestroy();
    f1_snr_unload(&f1_snr);
    f1_vpss.fnDisable(0);
}

static void f1_venc_fill_rc(f1_venc_rc *rc, hal_vidconfig *config, int h265, int framerate)
{
    unsigned int bitrate = (unsigned int)config->bitrate << 10;
    unsigned int maxBitrate = (unsigned int)MAX(config->bitrate, config->maxBitrate) << 10;
    unsigned int fps = (unsigned int)framerate | (1u << 16);

    memset(rc, 0, sizeof(*rc));
    switch (config->mode) {
        case HAL_VIDMODE_QP:
            if (h265) goto cbr;
            rc->mode = F1_VENC_RATEMODE_H264_FIXQP;
            rc->param[0] = config->maxQual;
            rc->param[1] = config->maxQual;
            rc->param[2] = fps;
            break;
        case HAL_VIDMODE_VBR:
        case HAL_VIDMODE_ABR:
        case HAL_VIDMODE_AVBR:
            rc->mode = h265 ? F1_VENC_RATEMODE_H265_VBR : F1_VENC_RATEMODE_H264_VBR;
            rc->param[0] = 35;
            rc->param[1] = maxBitrate;
            rc->param[2] = fps;
            rc->param[3] = 120;
            rc->param[6] = 5;
            rc->param[7] = 1;
            break;
        default:
cbr:
            rc->mode = h265 ? F1_VENC_RATEMODE_H265_CBR : F1_VENC_RATEMODE_H264_CBR;
            rc->param[0] = 35;
            rc->param[1] = bitrate;
            rc->param[2] = 28;
            rc->param[3] = 50;
            rc->param[4] = 35;
            rc->param[5] = 50;
            rc->param[6] = fps;
            rc->param[7] = 120;
            rc->param[10] = 5;
            rc->param[11] = 1;
            break;
    }
}

static int f1_osd_setup(void)
{
    int ret;
    f1_vpss_graph graph;
    f1_sys_buf buf = { 0 };

    if (_f1_osd_on) return EXIT_SUCCESS;
    if (!f1_sys.fnAlloc || !f1_vpss.fnSetGraph)
        HAL_ERROR("f1_osd", "Overlay symbols are unavailable!\n");

    _f1_osd_size = _f1_snr_dim.width * _f1_snr_dim.height * 2;
    if (ret = f1_sys.fnAlloc(&buf, _f1_osd_size, 8, "divinus-osd"))
        HAL_ERROR("f1_osd", "Allocating the overlay plane failed with %#x!\n", ret);
    if (!buf.virt)
        HAL_ERROR("f1_osd", "The overlay plane has no user mapping!\n");
    _f1_osd_phys = buf.phys;
    _f1_osd_virt = buf.virt;
    HAL_INFO("f1_osd", "Overlay plane %ux%u at %#x\n", _f1_snr_dim.width, _f1_snr_dim.height, _f1_osd_phys);
    memset(_f1_osd_virt, 0, _f1_osd_size);

    memset(&graph, 0, sizeof(graph));
    graph.enable = 1;
    graph.physAddr = _f1_osd_phys;
    graph.alpha = _f1_osd_alpha;
    graph.width = _f1_snr_dim.width;
    graph.height = _f1_snr_dim.height;
    graph.stride = _f1_snr_dim.width * 2;
    if (ret = f1_vpss.fnSetGraph(&graph))
        HAL_ERROR("f1_osd", "Enabling the overlay plane failed with %#x!\n", ret);
    _f1_osd_on = 1;
    return EXIT_SUCCESS;
}

/*
 * The hardware carries one alpha for the whole graphics plane, and the pixel
 * format is ARGB1555 whose alpha bit only says opaque or transparent, so a
 * per-region opacity cannot be honoured individually. Apply the highest
 * opacity any active region asks for: a region is then never more transparent
 * than configured, and with the usual single OSD region it is exact.
 */
static void f1_osd_apply_alpha(void)
{
    f1_vpss_graph graph;
    unsigned char alpha = 0;
    int any = 0;

    for (int i = 0; i < F1_OSD_MAX; i++)
        if (_f1_osd[i].used) { any = 1; if (_f1_osd[i].opal > alpha) alpha = _f1_osd[i].opal; }
    if (!any) alpha = 255;
    if (!_f1_osd_on || !f1_vpss.fnSetGraph || alpha == _f1_osd_alpha) { _f1_osd_alpha = alpha; return; }

    _f1_osd_alpha = alpha;
    memset(&graph, 0, sizeof(graph));
    graph.enable = 1;
    graph.physAddr = _f1_osd_phys;
    graph.alpha = _f1_osd_alpha;
    graph.width = _f1_snr_dim.width;
    graph.height = _f1_snr_dim.height;
    graph.stride = _f1_snr_dim.width * 2;
    if (f1_vpss.fnSetGraph(&graph))
        HAL_WARNING("f1_osd", "Could not set the overlay alpha to %u\n", _f1_osd_alpha);
}

/* divinus positions regions in main-stream pixels; the plane is at sensor size */
static void f1_osd_scale(hal_rect *rect)
{
    unsigned int sw = _f1_vpss_dim[0].width ? _f1_vpss_dim[0].width : _f1_snr_dim.width;
    unsigned int sh = _f1_vpss_dim[0].height ? _f1_vpss_dim[0].height : _f1_snr_dim.height;
    rect->x = (unsigned int)rect->x * _f1_snr_dim.width / sw;
    rect->y = (unsigned int)rect->y * _f1_snr_dim.height / sh;
}

static void f1_osd_clear(hal_rect rect)
{
    unsigned int w;

    /* rect.x/y are unsigned short and OSD positions are only range-checked
     * against SHRT_MAX, so a region placed past the plane made
     * _f1_snr_dim.width - rect.x wrap and memset most of memory. Reject the
     * rectangle before it is used to derive any width or offset. */
    if (!_f1_osd_virt) return;
    if (rect.x >= _f1_snr_dim.width || rect.y >= _f1_snr_dim.height) return;

    w = rect.width;
    if ((unsigned int)rect.x + w > _f1_snr_dim.width)
        w = _f1_snr_dim.width - rect.x;

    for (unsigned int y = 0; y < rect.height; y++) {
        unsigned int py = (unsigned int)rect.y + y;
        if (py >= _f1_snr_dim.height) break;
        memset(_f1_osd_virt + py * _f1_snr_dim.width + rect.x, 0, w * 2);
    }
}

int f1_region_create(int *handle, hal_rect rect, short opacity)
{
    int ret;
    if (ret = f1_osd_setup())
        return ret;
    if (*handle < 0 || *handle >= F1_OSD_MAX) {
        for (int i = 0; i < F1_OSD_MAX; i++)
            if (!_f1_osd[i].used) { *handle = i; break; }
        if (*handle < 0 || *handle >= F1_OSD_MAX)
            HAL_ERROR("f1_osd", "No free overlay region!\n");
    }
    if (_f1_osd[*handle].used)
        f1_osd_clear(_f1_osd[*handle].rect);
    f1_osd_scale(&rect);
    _f1_osd[*handle].used = 1;
    _f1_osd[*handle].rect = rect;
    _f1_osd[*handle].opal = opacity < 0 ? 0 : (opacity > 255 ? 255 : (unsigned char)opacity);
    f1_osd_apply_alpha();
    return EXIT_SUCCESS;
}

void f1_region_destroy(int *handle)
{
    if (*handle < 0 || *handle >= F1_OSD_MAX || !_f1_osd[*handle].used) return;
    f1_osd_clear(_f1_osd[*handle].rect);
    _f1_osd[*handle].used = 0;
    *handle = -1;
    f1_osd_apply_alpha();
}

/* Bitmaps arrive as BGR555LE; set the alpha bit for opaque pixels (ARGB1555) */
int f1_region_setbitmap(int *handle, hal_bitmap *bitmap)
{

    if (*handle < 0 || *handle >= F1_OSD_MAX || !_f1_osd[*handle].used || !_f1_osd_virt)
        return EXIT_FAILURE;
    f1_osd_rgn *rgn = &_f1_osd[*handle];
    f1_osd_clear(rgn->rect);
    if (rgn->rect.x >= _f1_snr_dim.width || rgn->rect.y >= _f1_snr_dim.height)
        return EXIT_SUCCESS;

    /* The bitmap is rendered in main-stream pixels while this plane covers the
     * whole sensor frame, so it is scaled by the same ratio f1_osd_scale()
     * applies to the position. Scaling only the position left the overlay
     * placed for one resolution and drawn at another whenever the main stream
     * was smaller than the sensor. Nearest neighbour is enough for OSD text. */
    unsigned int sw = _f1_vpss_dim[0].width ? _f1_vpss_dim[0].width : _f1_snr_dim.width;
    unsigned int sh = _f1_vpss_dim[0].height ? _f1_vpss_dim[0].height : _f1_snr_dim.height;
    unsigned int bw = bitmap->dim.width, bh = bitmap->dim.height;
    unsigned int dw = bw * _f1_snr_dim.width / sw;
    unsigned int dh = bh * _f1_snr_dim.height / sh;
    if (!bw || !bh) return EXIT_SUCCESS;
    if (!dw) dw = 1;
    if (!dh) dh = 1;
    if (dw > 0xffff) dw = 0xffff;
    if (dh > 0xffff) dh = 0xffff;

    rgn->rect.width = dw;
    rgn->rect.height = dh;
    unsigned short *src = bitmap->data;
    for (unsigned int y = 0; y < dh; y++) {
        unsigned int py = (unsigned int)rgn->rect.y + y;
        if (py >= _f1_snr_dim.height) break;
        unsigned int sy = y * bh / dh;
        unsigned short *dst = _f1_osd_virt + py * _f1_snr_dim.width + rgn->rect.x;
        for (unsigned int x = 0; x < dw && (unsigned int)rgn->rect.x + x < _f1_snr_dim.width; x++) {
            unsigned short p = src[sy * bw + (x * bw / dw)];
            dst[x] = p ? (p | 0x8000) : 0;
        }
    }
    return EXIT_SUCCESS;
}

int f1_video_create(char index, hal_vidconfig *config)
{
    int ret;
    unsigned int type;

    if (index >= F1_VENC_CHN_NUM)
        HAL_ERROR("f1_hal", "Only %d encoder channels are available!\n", F1_VENC_CHN_NUM);

    switch (config->codec) {
        case HAL_VIDCODEC_JPG:  type = F1_VENC_CODEC_JPEG; break;
        case HAL_VIDCODEC_MJPG: type = F1_VENC_CODEC_MJPEG; break;
        case HAL_VIDCODEC_H264: type = F1_VENC_CODEC_H264; break;
        case HAL_VIDCODEC_H265: type = F1_VENC_CODEC_H265; break;
        default: HAL_ERROR("f1_venc", "This codec is not supported by the hardware!\n");
    }

    if (config->width > _f1_snr_dim.width || config->height > _f1_snr_dim.height) {
        config->width = _f1_snr_dim.width;
        config->height = _f1_snr_dim.height;
    }

    /* The scaler output mode must match the encoder before it is configured */
    if (_f1_vpss_src[index] >= 0) {
        if (ret = f1_vpss.fnSetOutputMode(index, type == F1_VENC_CODEC_H265 ? 1 : 0))
            HAL_WARNING("f1_vpss", "Setting output mode on channel %d failed with %#x!\n", index, ret);
    } else {
        /* Shared scaler channel: the encoder must use that channel's size */
        char src = f1_vpss_pick(index);
        config->width = _f1_vpss_dim[src].width;
        config->height = _f1_vpss_dim[src].height;
    }

    if (_f1_venc_made[index].width) {
        /* There is no FH_VENC_DestroyChn: a channel's memory pool stays allocated
         * until FH_SYS_Exit and a second CreateChn fails (-0x3f3). The vendor
         * application creates each channel once with both codecs reserved and
         * switches with SetChnAttr; do the same, but never grow past the pool. */
        if (config->width > _f1_venc_made[index].width || config->height > _f1_venc_made[index].height)
            HAL_ERROR("f1_venc", "Channel %d was created at %ux%u; %ux%u needs a restart\n", index,
                _f1_venc_made[index].width, _f1_venc_made[index].height, config->width, config->height);
    } else {
        /* The vendor application always reserves memory for both H.264 and H.265 */
        f1_venc_cfg cfg = { .types = (type & (F1_VENC_CODEC_H264 | F1_VENC_CODEC_H265)) ?
            (F1_VENC_CODEC_H264 | F1_VENC_CODEC_H265) : type, .width = config->width, .height = config->height };
        if (ret = f1_venc.fnCreateChannel(index, &cfg)) {
            HAL_DANGER("f1_venc", "Creating channel %d (%ux%u) failed with %#x!\n", index, cfg.width, cfg.height, ret);
            return ret;
        }
        _f1_venc_made[index].width = cfg.width;
        _f1_venc_made[index].height = cfg.height;
    }

    if (type == F1_VENC_CODEC_JPEG) {
        f1_venc_jpgattr attr;
        memset(&attr, 0, sizeof(attr));
        attr.type = type;
        attr.quality = MAX(config->maxQual, 1);
        attr.rateIndex = 4;
        if (ret = f1_venc.fnSetChannelConfig(index, &attr))
            return ret;
    } else if (type == F1_VENC_CODEC_MJPEG) {
        /* { type, width, height, rotation(0..3), rateIndex(0..9), ..., rc } */
        f1_venc_attr attr;
        unsigned int quality = MAX(config->maxQual, 1);
        memset(&attr, 0, sizeof(attr));
        attr.type = type;
        attr.profile = config->width;
        attr.gop = config->height;
        attr.width = 0;                                /* rotation 0..3 */
        attr.height = 4;                               /* rate table index */
        if (config->mode == HAL_VIDMODE_QP) {
            /* fixed quality: { 0, quality(1..99), fps | den << 16 } */
            attr.rc.mode = 0;
            attr.rc.param[0] = quality > 99 ? 99 : quality;
            attr.rc.param[1] = (unsigned int)config->framerate | (1u << 16);
        } else {
            /* rate controlled: { 1, bitrate, ?, fps(u16) | den(u16) << 16 } */
            attr.rc.mode = 1;
            attr.rc.param[0] = (unsigned int)config->bitrate << 10;
            attr.rc.param[1] = (unsigned int)config->bitrate << 10;
            attr.rc.param[2] = (unsigned int)config->framerate | (1u << 16);
        }
        if (ret = f1_venc.fnSetChannelConfig(index, &attr)) {
            HAL_DANGER("f1_venc", "Configuring MJPEG channel %d failed with %#x!\n", index, ret);
            return ret;
        }
    } else {
        f1_venc_attr attr;
        memset(&attr, 0, sizeof(attr));
        attr.type = type;
        if (type == F1_VENC_CODEC_H265)
            attr.profile = 1;
        else {
            /* The vendor application only ever sets Main (77); High (100) is
             * rejected by FH_VENC_SetChnAttr with 0x19d and would leave the
             * channel closed, so every H.264 profile maps to Main here */
            if (config->profile != HAL_VIDPROFILE_MAIN)
                HAL_WARNING("f1_venc", "Channel %d: only the H.264 Main profile is supported, using it\n", index);
            attr.profile = 77;
        }
        attr.gop = config->gop ? config->gop : config->framerate * 2;
        attr.width = config->width;
        attr.height = config->height;
        f1_venc_fill_rc(&attr.rc, config, type == F1_VENC_CODEC_H265, config->framerate);
        if (ret = f1_venc.fnSetChannelConfig(index, &attr)) {
            HAL_DANGER("f1_venc", "Configuring channel %d failed with %#x!\n", index, ret);
            return ret;
        }
    }

    _f1_venc_codec[index] = type;

    if (type != F1_VENC_CODEC_JPEG && (ret = f1_venc.fnStartReceiving(index))) {
        HAL_DANGER("f1_venc", "Starting channel %d failed with %#x!\n", index, ret);
        return ret;
    }

    f1_state[index].payload = config->codec;

    return EXIT_SUCCESS;
}

int f1_video_destroy(char index)
{
    f1_state[index].enable = 0;
    f1_state[index].payload = HAL_VIDCODEC_UNSPEC;

    f1_venc.fnStopReceiving(index);
    f1_sys.fnUnbindByDst(index);
    _f1_venc_codec[index] = 0;

    /* The vendor has no FH_VENC_DestroyChn; the channel's pool is released
     * only by FH_SYS_Exit(), so stop it and leave the pool allocated. */

    return EXIT_SUCCESS;
}

int f1_video_destroy_all(void)
{
    for (char i = 0; i < F1_VENC_CHN_NUM; i++)
        if (f1_state[i].enable)
            f1_video_destroy(i);

    return EXIT_SUCCESS;
}

void f1_video_request_idr(char index)
{
    f1_venc.fnRequestIdr(index);
}

int f1_video_snapshot_grab(signed char index, hal_jpegdata *jpeg)
{
    int ret = EXIT_FAILURE;
    f1_venc_strm strm;
    unsigned int type = F1_VENC_CODEC_JPEG;

    /* index < 0: serve the most recent MJPEG frame cached by the video thread.
     * A real JPEG encoder shares the main scaler channel at bind time (the VPU
     * only has main + one sub), so snapshots come out at the main resolution;
     * this path is the fallback if that encoder cannot deliver a frame. */
    if (index < 0) {
        for (int w = 0; w < 100; w++) {
            pthread_mutex_lock(&_f1_mjpeg_mtx);
            if (_f1_mjpeg_len) {
                if (_f1_mjpeg_len > jpeg->length) {
                    /* keep the old buffer if this fails; assigning the result
                     * straight to jpeg->data leaked it and then memcpy'd NULL */
                    unsigned char *grown = realloc(jpeg->data, _f1_mjpeg_len);
                    if (!grown) {
                        pthread_mutex_unlock(&_f1_mjpeg_mtx);
                        HAL_ERROR("f1_venc", "Growing the snapshot buffer to %u bytes failed!\n",
                            _f1_mjpeg_len);
                    }
                    jpeg->data = grown;
                    jpeg->length = _f1_mjpeg_len;
                }
                memcpy(jpeg->data, _f1_mjpeg_cache, _f1_mjpeg_len);
                jpeg->jpegSize = _f1_mjpeg_len;
                pthread_mutex_unlock(&_f1_mjpeg_mtx);
                return EXIT_SUCCESS;
            }
            pthread_mutex_unlock(&_f1_mjpeg_mtx);
            usleep(20000);
        }
        HAL_ERROR("f1_venc", "No MJPEG frame available for snapshot!\n");
    } else if (_f1_venc_codec[index] != F1_VENC_CODEC_JPEG)
        HAL_ERROR("f1_venc", "Channel %d is not a JPEG encoder!\n", index);

    pthread_mutex_lock(&_f1_strm_mtx);

    {
        char src = _f1_vpss_src[index] < 0 ? f1_vpss_pick(index) : _f1_vpss_src[index];
        if (ret = f1_sys.fnBindVpu2Enc(src, index)) {
            HAL_DANGER("f1_venc", "Binding the encoder channel %d failed with %#x!\n", index, ret);
            goto fallback;
        }
    }

    for (int i = 0; i < 100; i++) {
        memset(&strm, 0, sizeof(strm));
        strm.type = type;
        if (!(ret = f1_venc.fnGetStream(type, &strm)))
            break;
        usleep(20000);
    }
    if (ret) {
        HAL_DANGER("f1_venc", "Getting a JPEG frame timed out (%#x)!\n", ret);
        goto fallback;
    }

    {
        /* JPEG: base = data address, frameType = length. MJPEG: NALU-style packets. */
        if (type == F1_VENC_CODEC_JPEG) {
            unsigned int len = strm.frameType;
            if (len > jpeg->length) {
                unsigned char *grown = realloc(jpeg->data, len);
                if (!grown) {
                    HAL_DANGER("f1_venc", "Growing the snapshot buffer to %u bytes failed!\n", len);
                    f1_venc.fnReleaseStream(strm.channel);
                    ret = EXIT_FAILURE;
                    goto abort;
                }
                jpeg->data = grown;
                jpeg->length = len;
            }
            memcpy(jpeg->data, (void*)strm.base, len);
            jpeg->jpegSize = len;
        } else {
            unsigned int total = 0;
            for (unsigned int i = 0; i < strm.naluCount && i < 28; i++)
                total += strm.nalu[i].length;
            if (total > jpeg->length) {
                unsigned char *grown = realloc(jpeg->data, total);
                if (!grown) {
                    HAL_DANGER("f1_venc", "Growing the snapshot buffer to %u bytes failed!\n", total);
                    f1_venc.fnReleaseStream(strm.channel);
                    ret = EXIT_FAILURE;
                    goto abort;
                }
                jpeg->data = grown;
                jpeg->length = total;
            }
            jpeg->jpegSize = 0;
            for (unsigned int i = 0; i < strm.naluCount && i < 28; i++) {
                memcpy(jpeg->data + jpeg->jpegSize, (void*)strm.nalu[i].addr, strm.nalu[i].length);
                jpeg->jpegSize += strm.nalu[i].length;
            }
        }
    }
    f1_venc.fnReleaseStream(strm.channel);
    ret = EXIT_SUCCESS;

abort:
    f1_sys.fnUnbindByDst(index);
    pthread_mutex_unlock(&_f1_strm_mtx);

    return ret;

fallback:
    f1_sys.fnUnbindByDst(index);
    pthread_mutex_unlock(&_f1_strm_mtx);
    HAL_WARNING("f1_venc", "Falling back to the MJPEG sub-stream for the snapshot\n");

    return f1_video_snapshot_grab(-1, jpeg);
}

void *f1_video_thread(void)
{
    unsigned int mask = F1_VENC_CODEC_H264 | F1_VENC_CODEC_H265 | F1_VENC_CODEC_MJPEG;
    f1_venc_strm strm;

    while (keepRunning) {
        char active = 0;
        for (int i = 0; i < F1_VENC_CHN_NUM; i++)
            if (f1_state[i].enable && f1_state[i].mainLoop) active = 1;
        if (!active) {
            usleep(100000);
            continue;
        }

        memset(&strm, 0, sizeof(strm));
        strm.type = mask;
        if (f1_venc.fnGetStreamBlocking(mask, &strm)) {
            usleep(10000);
            continue;
        }

        int index = strm.channel;
        if (index < 0 || index >= F1_VENC_CHN_NUM) {
            f1_venc.fnReleaseStream(strm.channel);
            continue;
        }
        /* The stream lives in the SDK's DMA buffer, which is mapped uncached:
         * every pass over it (start-code scans, RTP payload copies) reads at
         * bus speed and a 300 KB keyframe cost ~400 ms of CPU. Copy each frame
         * once into a cached buffer, hand the SDK buffer back at once, and let
         * every consumer work on the copy. */
        {
            static unsigned char *vbuf; static unsigned int vcap;
            unsigned int need = 0, off = 0;
            if (strm.type == F1_VENC_CODEC_MJPEG || _f1_venc_codec[index] == F1_VENC_CODEC_MJPEG)
                need = strm.frameType;
            else for (unsigned int i = 0; i < strm.naluCount && i < 28; i++) need += strm.nalu[i].length;
            if (need > vcap) {
                unsigned char *n = realloc(vbuf, need + 65536);
                if (n) { vbuf = n; vcap = need + 65536; }
            }
            if (vbuf && need <= vcap) {
                if (strm.type == F1_VENC_CODEC_MJPEG || _f1_venc_codec[index] == F1_VENC_CODEC_MJPEG) {
                    memcpy(vbuf, (void*)strm.base, need);
                    strm.base = (unsigned int)vbuf;
                } else for (unsigned int i = 0; i < strm.naluCount && i < 28; i++) {
                    memcpy(vbuf + off, (void*)strm.nalu[i].addr, strm.nalu[i].length);
                    strm.nalu[i].addr = (unsigned int)(vbuf + off);
                    off += strm.nalu[i].length;
                }
                f1_venc.fnReleaseStream(strm.channel);
                strm.channel |= 0x100;   /* released: skip the release below */
            }
        }
        if (index < F1_VENC_CHN_NUM && f1_state[index].enable && f1_vid_cb) {
            /* The stream record's type field does not match the f1_venc_codec the
             * channel was configured with for HEVC (VPS/SPS/PPS were parsed with
             * H.264 rules and never recognised), so go by what was configured */
            unsigned int chnType = _f1_venc_codec[index];
            char h265 = chnType == F1_VENC_CODEC_H265;
            hal_vidstream outStrm;
            hal_vidpack outPack[28];
            unsigned long long pts = ((unsigned long long)strm.ptsHigh << 32) | strm.ptsLow;
            memset(outPack, 0, sizeof(outPack));
            outStrm.seq = 0;
            outStrm.pack = outPack;

            if (chnType == F1_VENC_CODEC_MJPEG || strm.type == F1_VENC_CODEC_MJPEG) {
                /* MJPEG/JPEG frames arrive whole at base/frameType, not as NALUs */
                unsigned int mlen = strm.frameType;
                pthread_mutex_lock(&_f1_mjpeg_mtx);
                if (mlen > _f1_mjpeg_cap) {
                    unsigned char *grown = realloc(_f1_mjpeg_cache, mlen);
                    if (grown) {
                        _f1_mjpeg_cache = grown;
                        _f1_mjpeg_cap = mlen;
                    } else
                        HAL_WARNING("f1_venc", "Growing the MJPEG cache to %u bytes failed!\n", mlen);
                }
                if (_f1_mjpeg_cache && mlen <= _f1_mjpeg_cap) {
                    memcpy(_f1_mjpeg_cache, (void*)strm.base, mlen);
                    _f1_mjpeg_len = mlen;
                }
                pthread_mutex_unlock(&_f1_mjpeg_mtx);
                outStrm.count = 1;
                outPack[0].data = (unsigned char*)strm.base;
                outPack[0].length = strm.frameType;
                outPack[0].offset = 0;
                outPack[0].timestamp = pts;
            } else {
                outStrm.count = strm.naluCount > 28 ? 28 : strm.naluCount;
                for (unsigned int i = 0; i < outStrm.count; i++) {
                    unsigned char *data = (unsigned char*)strm.nalu[i].addr;
                    unsigned int len = strm.nalu[i].length;
                    outPack[i].data = data;
                    outPack[i].length = len;
                    outPack[i].offset = 0;
                    outPack[i].timestamp = pts;
                    /* The HEVC encoder hands VPS, SPS and PPS bundled into the IDR
                     * entry (3-byte start codes inside it), and the MP4 and raw
                     * consumers trust these per-NALU entries, so split each entry
                     * on start codes; RTP splits on its own anyway */
                    unsigned int cnt = 0, pos = 0;
                    while (pos + 3 < len && cnt < 8) {
                        unsigned int sc = 0, next = pos + 3;
                        if (data[pos] == 0 && data[pos + 1] == 0 && data[pos + 2] == 1) sc = 3;
                        else if (pos + 4 < len && data[pos] == 0 && data[pos + 1] == 0 &&
                            data[pos + 2] == 0 && data[pos + 3] == 1) sc = 4;
                        if (!sc) break;
                        for (next = pos + sc + 1; next + 2 < len; next++)
                            if (data[next] == 0 && data[next + 1] == 0 &&
                                (data[next + 2] == 1 || (next + 3 < len && data[next + 2] == 0 && data[next + 3] == 1)))
                                break;
                        if (next + 2 >= len) next = len;
                        outPack[i].nalu[cnt].offset = pos;
                        outPack[i].nalu[cnt].length = next - pos;
                        outPack[i].nalu[cnt].type = h265 ?
                            ((data[pos + sc] >> 1) & 0x3f) : (data[pos + sc] & 0x1f);
                        cnt++;
                        pos = next;
                    }
                    if (!cnt) {
                        /* no start code found: pass the entry through as one unit */
                        outPack[i].nalu[0].offset = 0;
                        outPack[i].nalu[0].length = len;
                        outPack[i].nalu[0].type = 0;
                        cnt = 1;
                    }
                    outPack[i].naluCnt = cnt;
                }
            }
            (*f1_vid_cb)(index, &outStrm);
        }

        if (!(strm.channel & 0x100))
            f1_venc.fnReleaseStream(strm.channel);
    }
    HAL_INFO("f1_venc", "Shutting down encoding thread...\n");
    return NULL;
}

void f1_system_deinit(void)
{
    /* FH_SYS_Exit() releases the encoder and scaler pools. */
    f1_sys.fnExit();
}

static int f1_sensor_select(void)
{
    static const char *dirs[] = { "/usr/lib/sensors", "/lib/sensors" };
    char names[F1_SNR_MAX][32];
    int foundNum = 0;
    const char *name = getenv("SENSOR");
    int found = 0;

    f1_snr_unload(&f1_snr);
    if (name && f1_snr_load(&f1_snr, name)) {
        f1_snr_unload(&f1_snr);
        name = NULL;
    }
    if (!name) {
        for (unsigned int d = 0; d < sizeof(dirs) / sizeof(dirs[0]); d++) {
            struct dirent *entry;
            DIR *dir;
            if (!(dir = opendir(dirs[d]))) continue;
            while ((entry = readdir(dir)) && foundNum < F1_SNR_MAX) {
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
            found = !f1_snr_load(&f1_snr, names[i]);
            if (!found) f1_snr_unload(&f1_snr);
        }
        if (!found)
            return EXIT_FAILURE;
    }

    if (f1_snr.fnCreate())
        goto fail;
    {
        f1_isp_viattr attr = { 0 };
        if (!f1_snr.ops->fnGetInputAttr || f1_snr.ops->fnGetInputAttr(&attr) ||
            !attr.width || !attr.height)
            goto fail;
        _f1_snr_dim.width = attr.width;
        _f1_snr_dim.height = attr.height;
        f1_snr.dim = _f1_snr_dim;
    }
    strncpy(sensor, f1_snr.name, sizeof(sensor) - 1);
    return EXIT_SUCCESS;

fail:
    if (f1_snr.fnDestroy) f1_snr.fnDestroy();
    f1_snr_unload(&f1_snr);
    return EXIT_FAILURE;
}

int f1_system_init(void)
{
    {
        f1_sys_ver version;
        if (!f1_sys.fnGetVersion(&version))
            HAL_INFO("f1_hal", "SDK build %08x (%08x), package id %#x\n",
                version.date, version.commit, version.package);
    }

    if (f1_sensor_select())
        HAL_ERROR("f1_snr", "Failed to load the sensor driver\n");

    HAL_INFO("f1_hal", "Detected sensor %s (%ux%u)\n", f1_snr.name,
        _f1_snr_dim.width, _f1_snr_dim.height);

    return EXIT_SUCCESS;
}

int f1_night_available(void)
{
    return _f1_gain_night;
}

int f1_night_status(void)
{
    if (!_f1_gain_night) return 0;
    _f1_night_prev = f1_isp.fnGainNightStatus(_f1_night_prev) ? 1 : 0;
    return _f1_night_prev;
}

#endif
