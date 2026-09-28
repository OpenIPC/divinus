#if defined(__arm__) && !defined(__ARM_PCS_VFP) && __ARM_ARCH == 6

#include "fh_hal.h"
#include "../../gpio.h"
#include <math.h>

#include <errno.h>
#include <fcntl.h>
#include <pthread.h>

fh_aud_impl  fh_aud;
fh_isp_impl  fh_isp;
fh_sys_impl  fh_sys;
fh_venc_impl fh_venc;
fh_vpss_impl fh_vpss;

hal_chnstate fh_state[FH_VENC_CHN_NUM] = {0};
int (*fh_aud_cb)(hal_audframe*);
int (*fh_vid_cb)(char, hal_vidstream*);

static fh_snr_driver *_fh_snr;
static fh_isp_snrops *_fh_snr_ops;
static fh_common_dim _fh_snr_dim;
static int _fh_snr_fmt;
static char _fh_isp_on, _fh_isp_busy;
static unsigned int _fh_venc_type[FH_VENC_CHN_NUM];
static fh_common_dim _fh_venc_made[FH_VENC_CHN_NUM]; /* size FH_VENC_CreateChn reserved */
static fh_common_dim _fh_vpss_made[FH_VPSS_CHN_NUM]; /* size FH_VPSS_ChnInitMem reserved */
static char _fh_vpss_open[FH_VPSS_CHN_NUM];
static fh_common_dim _fh_vpss_dim[FH_VPSS_CHN_NUM];
static char _fh_vpss_fps[FH_VPSS_CHN_NUM];
static char _fh_param_path[128];
static int _fh_vpss_count = FH_VPSS_CHN_NUM;
static fh_common_dim _fh_want_dim[FH_VENC_CHN_NUM];
static signed char _fh_vpss_src[FH_VENC_CHN_NUM];   /* scaler channel feeding each encoder */
static char fh_vpss_pick(char index);

/* Graphic overlay: one 16-bit ARGB1555 plane at sensor resolution, blended before scaling */
typedef struct {
    unsigned int enable;
    unsigned int physAddr;
    unsigned int alpha;
    unsigned int reserved3, reserved4;
    unsigned int width, height;
    unsigned int x, y;
    unsigned int reserved9, reserved10;
    unsigned int stride;        /* bytes per line */
} fh_vpss_graph;
#define FH_OSD_MAX 8
typedef struct { char used; hal_rect rect; unsigned char opal; } fh_osd_rgn;
static fh_osd_rgn _fh_osd[FH_OSD_MAX];
static unsigned int _fh_osd_phys, _fh_osd_size;
static unsigned short *_fh_osd_virt;
static char _fh_osd_on;
static unsigned char _fh_osd_alpha = 255;
static pthread_mutex_t _fh_strm_mtx = PTHREAD_MUTEX_INITIALIZER;
static char _fh_aud_on;
static unsigned int _fh_aud_frame, _fh_aud_rate;
static char _fh_smartir, _fh_night_prev;
static unsigned char *_fh_mjpeg_cache; static unsigned int _fh_mjpeg_len, _fh_mjpeg_cap; static pthread_mutex_t _fh_mjpeg_mtx = PTHREAD_MUTEX_INITIALIZER;

/* libc compatibility shims for the SDK libraries live in fh_compat.c */

static void fh_proc_write(const char *path, const char *value)
{
    FILE *file = fopen(path, "w");
    if (!file) {
        HAL_WARNING("fh_hal", "Cannot write %s to %s!\n", value, path);
        return;
    }
    fputs(value, file);
    fclose(file);
}

void fh_hal_deinit(void)
{
    fh_aud_unload(&fh_aud);
    fh_venc_unload(&fh_venc);
    fh_vpss_unload(&fh_vpss);
    fh_isp_unload(&fh_isp);
    fh_sys_unload(&fh_sys);
}

int fh_hal_init(void)
{
    int ret;

    if (ret = fh_sys_load(&fh_sys))
        return ret;
    if (ret = fh_isp_load(&fh_isp))
        return ret;
    if (ret = fh_vpss_load(&fh_vpss))
        return ret;
    if (ret = fh_venc_load(&fh_venc))
        return ret;
    if (fh_aud_load(&fh_aud))
        HAL_WARNING("fh_hal", "Audio library unavailable, audio disabled\n");

    return EXIT_SUCCESS;
}

void fh_audio_deinit(void)
{
    if (!_fh_aud_on) return;
    _fh_aud_on = 0;
    fh_aud.fnDisable();
    fh_aud.fnDeinit();
}

int fh_audio_init(int samplerate)
{
    int ret;

    if (!fh_aud.handle)
        HAL_ERROR("fh_aud", "Audio library is not loaded!\n");

    if (ret = fh_aud.fnInit()) {
        HAL_DANGER("fh_aud", "FH_AC_Init failed with %#x\n", ret);
        return ret;
    }

    {
        /* A sample rate the config parser rejected arrives here as 0 (a saved
         * "srate: 0" is outside the 8000..96000 range); fall back to 8 kHz rather
         * than asking the codec for zero-length frames */
        if (!samplerate) samplerate = 8000;
        /* 40 ms frames of 16-bit mono PCM, as the vendor application does */
        _fh_aud_frame = samplerate / 25;
        _fh_aud_rate = samplerate;
        fh_aud_cnf config = { .ioType = 0, .sampleRate = samplerate, .bitWidth = 16,
            .encFormat = 0, .channels = 1, .frameSamples = _fh_aud_frame, .volume = 85 };
        if (ret = fh_aud.fnSetConfig(&config)) {
            HAL_DANGER("fh_aud", "FH_AC_Set_Config(%d Hz, %u samples) failed with %#x\n", samplerate, _fh_aud_frame, ret);
            return ret;
        }
    }
    if (ret = fh_aud.fnEnable()) {
        HAL_DANGER("fh_aud", "FH_AC_AI_Enable failed with %#x\n", ret);
        return ret;
    }
    fh_aud.fnSetMicVolume(2);
    fh_aud.fnSetVolume(85);

    _fh_aud_on = 1;
    return EXIT_SUCCESS;
}

void *fh_audio_thread(void)
{
    unsigned char *buf = malloc(_fh_aud_frame * 2 + 64);
    unsigned int seq = 0;

    while (keepRunning && audioOn && _fh_aud_on) {
        fh_aud_frm frame = { .length = 0, .data = buf };
        unsigned long long pts = 0;
        int ret = fh_aud.fnGetFrame(&frame, &pts);
        if (ret || !frame.length) {
            usleep(5000);
            continue;
        }
        {
            /* The board picks up mains hum (50 Hz and harmonics) on the mic
             * path; a 2nd-order Butterworth high-pass at ~120 Hz removes most
             * of it and leaves speech intact. Coefficients follow the sample rate. */
            static float x1, x2, y1, y2, b0, b1, b2, a1, a2; static unsigned int forRate;
            if (forRate != _fh_aud_rate) {
                float w0 = 2.0f * 3.14159265f * 120.0f / (float)_fh_aud_rate, c = cosf(w0);
                float alpha = sinf(w0) / (2.0f * 0.7071f), a0 = 1.0f + alpha;
                b0 = (1.0f + c) / 2.0f / a0; b1 = -(1.0f + c) / a0; b2 = b0;
                a1 = -2.0f * c / a0; a2 = (1.0f - alpha) / a0;
                x1 = x2 = y1 = y2 = 0; forRate = _fh_aud_rate;
            }
            short *pcm = (short*)buf;
            for (unsigned int i = 0; i < frame.length / 2; i++) {
                float x0 = pcm[i];
                float y0 = b0 * x0 + b1 * x1 + b2 * x2 - a1 * y1 - a2 * y2;
                x2 = x1; x1 = x0; y2 = y1; y1 = y0;
                pcm[i] = y0 > 32767 ? 32767 : y0 < -32768 ? -32768 : (short)y0;
            }
        }
        if (fh_aud_cb) {
            hal_audframe outFrame;
            memset(&outFrame, 0, sizeof(outFrame));
            outFrame.channelCnt = 1;
            outFrame.data[0] = buf;
            outFrame.length[0] = frame.length;
            outFrame.seq = seq++;
            outFrame.timestamp = pts / 1000;
            (fh_aud_cb)(&outFrame);
        }
    }
    free(buf);
    HAL_INFO("fh_aud", "Shutting down capture thread...\n");
    return NULL;
}

int fh_channel_bind(char index)
{
    int ret;
    char src = _fh_vpss_src[index] < 0 ? fh_vpss_pick(index) : _fh_vpss_src[index];

    for (int i = 0; i < 5; i++) {
        if (!(ret = fh_sys.fnBindVpu2Enc(src, index)))
            return EXIT_SUCCESS;
        usleep(100000);
    }
    HAL_DANGER("fh_hal", "Binding scaler channel %d to encoder %d failed with %#x!\n", src, index, ret);
    return ret;
}

static int fh_vpss_setup(char index, short width, short height, char framerate)
{
    int ret;
    int snrFps = _fh_snr->fnFramerateForFormat(_fh_snr_fmt);
    if (framerate > snrFps) framerate = snrFps;

    if (_fh_vpss_open[index] && _fh_vpss_dim[index].width == width &&
        _fh_vpss_dim[index].height == height && _fh_vpss_fps[index] == framerate)
        return EXIT_SUCCESS;

    if (_fh_vpss_open[index]) {
        fh_vpss.fnCloseChannel(index);
        _fh_vpss_open[index] = 0;
    }

    {
        fh_common_dim dim = { .width = width, .height = height };
        /* Like the encoder pools, a scaler channel's memory cannot be released
         * again (a second ChnInitMem fails with -1), so allocate once and keep
         * reconfiguring within that size */
        if (_fh_vpss_made[index].width) {
            if (dim.width > _fh_vpss_made[index].width || dim.height > _fh_vpss_made[index].height)
                HAL_ERROR("fh_hal", "Scaler channel %d was sized %ux%u; %ux%u needs a restart\n", index,
                    _fh_vpss_made[index].width, _fh_vpss_made[index].height, dim.width, dim.height);
        } else {
            int (*fnInitChannelMem)(unsigned int, unsigned int, unsigned int) =
                dlsym(fh_vpss.handle, "FH_VPSS_ChnInitMem");
            if (fnInitChannelMem && (ret = fnInitChannelMem(index, dim.width, dim.height)))
                HAL_WARNING("fh_hal", "Allocating scaler channel %d memory failed with %#x!\n", index, ret);
            else _fh_vpss_made[index] = dim;
        }
        if (ret = fh_vpss.fnSetChannelConfig(index, &dim))
            return ret;
        _fh_vpss_dim[index] = dim;
    }

    {
        fh_vpss_framectrl ctrl = { .srcRate = framerate, .dstRate = 1 };
        if (ret = fh_vpss.fnSetFrameControl(index, &ctrl))
            return ret;
        _fh_vpss_fps[index] = framerate;
    }

    if (ret = fh_vpss.fnOpenChannel(index))
        return ret;
    _fh_vpss_open[index] = 1;

    return EXIT_SUCCESS;
}

static char fh_vpss_pick(char index)
{
    /* Prefer an open scaler channel with the same size, else the main one */
    for (char i = 0; i < _fh_vpss_count; i++)
        if (_fh_vpss_open[i] && _fh_vpss_dim[i].width == _fh_want_dim[index].width &&
            _fh_vpss_dim[i].height == _fh_want_dim[index].height)
            return i;
    return 0;
}

int fh_channel_create(char index, short width, short height, char framerate, char jpeg)
{
    if (index >= FH_VENC_CHN_NUM)
        HAL_ERROR("fh_hal", "Only %d encoder channels are available!\n", FH_VENC_CHN_NUM);

    _fh_want_dim[index].width = width;
    _fh_want_dim[index].height = height;
    if (index >= _fh_vpss_count) {
        _fh_vpss_src[index] = -1;
        HAL_INFO("fh_hal", "Channel %d: no free scaler channel, it will share one at bind time\n", index);
        return EXIT_SUCCESS;
    }
    _fh_vpss_src[index] = index;

    if (width > _fh_snr_dim.width || height > _fh_snr_dim.height) {
        HAL_WARNING("fh_hal", "Channel %d: requested %dx%d too large, using sensor %dx%d\n",
            index, width, height, _fh_snr_dim.width, _fh_snr_dim.height);
        width = _fh_snr_dim.width;
        height = _fh_snr_dim.height;
    }

    return fh_vpss_setup(index, width, height, framerate);
}

void fh_channel_destroy(char index)
{
    /* Keep the scaler channel open: closing and reopening it around a codec or
     * bitrate change left it delivering no frames on the FH8856 (encoder frame
     * count stayed at 0, main-channel snapshots timed out), and fh_vpss_setup()
     * reuses an open channel of the same size and rate anyway. */
}

/*
 * Colour vs monochrome is the ISP saturation record (20 bytes: enable word,
 * then the curve). The tuning file sets a valid colour record; a zeroed record
 * is monochrome (what the vendor's mono path writes). libadvapi's
 * FHAdv_Isp_SetColorMode() cannot be used here: its "restore colour" writes
 * the library's own copy, which is never filled on this build, so it zeroed
 * the record instead. Keep the record from the tuning load and write it back.
 */
static unsigned char _fh_sat_colour[32];
static char _fh_sat_ok, _fh_gray_want;

static void fh_grayscale_apply(void)
{
    unsigned char zero[32] = {0};
    if (_fh_gray_want)
        fh_isp.fnSetSaturation(zero);
    else if (_fh_sat_ok)
        fh_isp.fnSetSaturation(_fh_sat_colour);
    /* else: nothing captured yet, the ISP is still in its tuned colour state */
}

int fh_channel_grayscale(char enable)
{
    _fh_gray_want = enable ? 1 : 0;
    fh_grayscale_apply();
    return EXIT_SUCCESS;
}

int fh_channel_unbind(char index)
{
    fh_sys.fnUnbindByDst(index);

    return EXIT_SUCCESS;
}

/*
 * ISP tuning ("<sensor>_attr.hex", 3268 bytes): the vendor files are
 * generated for a newer SDK build; the older library only warns about the
 * version word and still loads them.
 */
int fh_config_load(char *path)
{
    static unsigned char param[4096];
    FILE *file;
    size_t len;

    if (EQUALS(path, _fh_param_path))
        return EXIT_SUCCESS;
    if (!(file = fopen(path, "rb")))
        return EXIT_FAILURE;
    memset(param, 0, sizeof(param));
    len = fread(param, 1, sizeof(param), file);
    fclose(file);
    if (len < 0x2c)
        return EXIT_FAILURE;

    HAL_INFO("fh_hal", "Loading ISP parameters from %s (%zu bytes)\n", path, len);
    if (fh_isp.fnLoadParam(param))
        return EXIT_FAILURE;
    {
        unsigned char sat[32] = {0};
        if (!fh_isp.fnGetSaturation(sat) && sat[5]) {
            memcpy(_fh_sat_colour, sat, sizeof(_fh_sat_colour));
            _fh_sat_ok = 1;
            fh_grayscale_apply();       /* honour a mode requested before the tuning load */
        } else
            HAL_WARNING("fh_hal", "Could not read the colour saturation record; grayscale off will be a no-op\n");
    }
    strncpy(_fh_param_path, path, sizeof(_fh_param_path) - 1);
    {
        int ret = fh_isp.fnAdvInit();
        return ret;
    }
}

void *fh_image_thread(void)
{
    while (keepRunning) {
        if (_fh_isp_on) {
            _fh_isp_busy = 1;
            fh_isp.fnRun();
            _fh_isp_busy = 0;
            usleep(10000);
        } else usleep(100000);
    }
    HAL_INFO("fh_hal", "Shutting down ISP thread...\n");
    return NULL;
}

/* Anti-banding: the ISP AE stores a 2-bit flicker mode at param offset 0x3a
 * bits 6-7 (0 off, 1 = 50 Hz, 2 = 60 Hz), set through AE command 0xa. The
 * newer vendor SDK adds dedicated commands 0x1b-0x1d, absent from these libs. */
int fh_set_antiflicker(char hz)
{
    unsigned int mode = hz >= 60 ? 2 : hz >= 50 ? 1 : 0;
    if (!fh_isp.fnAeSendCmd)
        return EXIT_FAILURE;
    return fh_isp.fnAeSendCmd(0x0a, &mode);
}

int fh_pipeline_create(short width, short height, char mirror, char flip, char framerate, char antiflicker)
{
    int ret;
    char path[128];

    /* Kernel-side buffer sizing must precede FH_SYS_Init() */
    {
        char value[64];
        snprintf(value, sizeof(value), "vi_%u_%u", _fh_snr_dim.width, _fh_snr_dim.height);
        fh_proc_write("/proc/driver/vpu", value);
        /* Scaler channel memory is allocated on demand in fh_channel_create() */
        for (int i = 0; i < FH_VPSS_CHN_NUM; i++) {
            snprintf(value, sizeof(value), "cap_%d_0_0", i);
            fh_proc_write("/proc/driver/vpu", value);
            snprintf(value, sizeof(value), "buf_%d_2", i);
            fh_proc_write("/proc/driver/vpu", value);
        }
        fh_proc_write("/proc/driver/vpu", "support4k_off");
        fh_proc_write("/proc/driver/isp", "support4k_off");
        fh_proc_write("/proc/driver/vpu", "sublimit_off");
        fh_proc_write("/proc/driver/isp", "sublimit_off");
        fh_proc_write("/proc/driver/isp", "wdr_off");
        fh_proc_write("/proc/driver/isp", "cir_off");
        fh_proc_write("/proc/driver/enc", "stm_2621440");
        fh_proc_write("/proc/driver/hevc", "stm_2621440");
        fh_proc_write("/proc/driver/hevc", "usebfrm_0");
        /* JPEG snapshot buffer: the driver's own rule is width*height/2, forcing 128 KiB
         * (the vendor value) overruns at 2560x1440 and crashes the JPEG thread */
        snprintf(value, sizeof(value), "mem_1_%u", ALIGN_UP((unsigned int)width * height / 2 + 8, 4096));
        fh_proc_write("/proc/driver/jpeg", value);
        /* Motion JPEG pool: three 512 KiB slots (larger pools fail to ioremap on 32 MiB systems) */
        {
            unsigned int jpgSize = 512 * 1024;
            snprintf(value, sizeof(value), "mjpg_%u_%u", jpgSize * 3, jpgSize);
            fh_proc_write("/proc/driver/jpeg", value);
        }
    }

    if (ret = fh_sys.fnInit())
        return ret;

    {
        int (*fnGetCapability)(unsigned int, void *) = dlsym(fh_vpss.handle, "FH_VPSS_GetChnCapality");
        unsigned int cap[8];
        _fh_vpss_count = 0;
        while (fnGetCapability && _fh_vpss_count < FH_VPSS_CHN_NUM && !fnGetCapability(_fh_vpss_count, cap))
            _fh_vpss_count++;
        if (!_fh_vpss_count) _fh_vpss_count = 1;
        HAL_INFO("fh_hal", "%d scaler channels available\n", _fh_vpss_count);
    }

    if (ret = fh_vpss.fnSetInputConfig(&_fh_snr_dim))
        return ret;
    if (ret = fh_vpss.fnEnable(0))
        return ret;

    _fh_snr_ops = _fh_snr->fnCreate(&fh_isp);
    _fh_snr_fmt = _fh_snr->fnFormatForFramerate(framerate);

    /* The ISP only starts delivering frames when a scaler channel is already open */
    if (width > _fh_snr_dim.width) width = _fh_snr_dim.width;
    if (height > _fh_snr_dim.height) height = _fh_snr_dim.height;
    if (ret = fh_vpss_setup(0, width, height, framerate))
        return ret;

    if (ret = fh_isp.fnMemInit(_fh_snr_dim.width, _fh_snr_dim.height))
        return ret;
    if (ret = fh_isp.fnRegisterSensor(0, _fh_snr_ops))
        return ret;
    if (ret = fh_isp.fnSensorInit())
        return ret;
    if (ret = fh_isp.fnSetSensorFormat(_fh_snr_fmt))
        return ret;
    if (ret = fh_isp.fnInit())
        return ret;

    snprintf(path, sizeof(path), "/etc/sensors/%s_attr.hex", _fh_snr->name);
    if (fh_config_load(path)) {
        snprintf(path, sizeof(path), "/usr/lib/sensors/params/%s_attr.hex", _fh_snr->name);
        if (fh_config_load(path))
            HAL_WARNING("fh_hal", "No ISP tuning file found for %s, image quality will suffer!\n",
                _fh_snr->name);
    }

    if (mirror || flip)
        fh_isp.fnSetFlipMirrorEx(mirror ? 1 : 0, flip ? 1 : 0, _fh_snr->bayer);

    fh_set_antiflicker(antiflicker);

    /* Image-gain based day/night detection (no external light sensor needed) */
    if (fh_isp.fnSmartIrInit && !fh_isp.fnSmartIrInit()) {
        if (fh_isp.fnSmartIrSetAttr)
            fh_isp.fnSmartIrSetAttr(0);
        _fh_smartir = fh_isp.fnSmartIrStatus != NULL;
        HAL_INFO("fh_hal", "SmartIR day/night detection %s\n", _fh_smartir ? "enabled" : "unavailable");
        if (_fh_smartir && fh_isp.fnSmartIrGetThreshold && fh_isp.fnSmartIrSetThreshold) {
            /* The status function re-reads the gain thresholds from the ISP
             * parameter block (loaded from the sensor tuning file) on every
             * call, which silently replaces the library defaults; the GC4653
             * file's values flipped the PB1 to night in daylight at 1.1x gain.
             * The vendor application always writes them back; do the same. */
            unsigned short th[4] = {0};
            fh_isp.fnSmartIrGetThreshold(th);
            fh_isp.fnSmartIrSetThreshold(th);
            HAL_INFO("fh_hal", "SmartIR thresholds: %u %u %u %u\n", th[0], th[1], th[2], th[3]);
        }
    }
    fh_irled(0);
    fh_whitelamp(0);

    _fh_isp_on = 1;

    return EXIT_SUCCESS;
}

void fh_pipeline_destroy(void)
{
    _fh_isp_on = 0;
    for (int i = 0; i < 50 && _fh_isp_busy; i++)
        usleep(10000);

    fh_isp.fnExit();
    fh_isp.fnUnregisterSensor(0);
    if (_fh_snr_ops && _fh_snr_ops->fnDeinit)
        _fh_snr_ops->fnDeinit();

    for (int i = 0; i < FH_VPSS_CHN_NUM; i++)
        fh_channel_destroy(i);
    fh_vpss.fnDisable(0);
}

static void fh_venc_fill_rc(fh_venc_rc *rc, hal_vidconfig *config, int h265, int framerate)
{
    unsigned int bitrate = (unsigned int)config->bitrate << 10;
    unsigned int maxBitrate = (unsigned int)MAX(config->bitrate, config->maxBitrate) << 10;
    unsigned int fps = (unsigned int)framerate | (1u << 16);

    memset(rc, 0, sizeof(*rc));
    switch (config->mode) {
        case HAL_VIDMODE_QP:
            if (h265) goto cbr;
            rc->mode = FH_VENC_RC_H264_FIXQP;
            rc->param[0] = config->maxQual;
            rc->param[1] = config->maxQual;
            rc->param[2] = fps;
            break;
        case HAL_VIDMODE_VBR:
        case HAL_VIDMODE_ABR:
        case HAL_VIDMODE_AVBR:
            rc->mode = h265 ? FH_VENC_RC_H265_VBR : FH_VENC_RC_H264_VBR;
            rc->param[0] = 35;
            rc->param[1] = maxBitrate;
            rc->param[2] = fps;
            rc->param[3] = 120;
            rc->param[6] = 5;
            rc->param[7] = 1;
            break;
        default:
cbr:
            rc->mode = h265 ? FH_VENC_RC_H265_CBR : FH_VENC_RC_H264_CBR;
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

static int fh_osd_setup(void)
{
    int ret;
    /* libvmm helper: fills { phys, virt, size } */
    int (*fnAlloc)(void *, unsigned int, unsigned int, const char *) =
        dlsym(fh_sys.handleVmm, "buffer_malloc_withname");
    int (*fnSetGraph)(fh_vpss_graph *) = dlsym(fh_vpss.handle, "FH_VPSS_SetGraph");
    fh_vpss_graph graph;
    struct { unsigned int phys; unsigned short *virt; unsigned int size; } buf = { 0 };

    if (_fh_osd_on) return EXIT_SUCCESS;
    if (!fnAlloc || !fnSetGraph)
        HAL_ERROR("fh_osd", "Overlay symbols are unavailable!\n");

    _fh_osd_size = _fh_snr_dim.width * _fh_snr_dim.height * 2;
    if (ret = fnAlloc(&buf, _fh_osd_size, 8, "divinus-osd"))
        HAL_ERROR("fh_osd", "Allocating the overlay plane failed with %#x!\n", ret);
    if (!buf.virt)
        HAL_ERROR("fh_osd", "The overlay plane has no user mapping!\n");
    _fh_osd_phys = buf.phys;
    _fh_osd_virt = buf.virt;
    HAL_INFO("fh_osd", "Overlay plane %ux%u at %#x\n", _fh_snr_dim.width, _fh_snr_dim.height, _fh_osd_phys);
    memset(_fh_osd_virt, 0, _fh_osd_size);

    memset(&graph, 0, sizeof(graph));
    graph.enable = 1;
    graph.physAddr = _fh_osd_phys;
    graph.alpha = _fh_osd_alpha;
    graph.width = _fh_snr_dim.width;
    graph.height = _fh_snr_dim.height;
    graph.stride = _fh_snr_dim.width * 2;
    if (ret = fnSetGraph(&graph))
        HAL_ERROR("fh_osd", "Enabling the overlay plane failed with %#x!\n", ret);
    _fh_osd_on = 1;
    return EXIT_SUCCESS;
}

/*
 * The hardware carries one alpha for the whole graphics plane, and the pixel
 * format is ARGB1555 whose alpha bit only says opaque or transparent, so a
 * per-region opacity cannot be honoured individually. Apply the highest
 * opacity any active region asks for: a region is then never more transparent
 * than configured, and with the usual single OSD region it is exact.
 */
static void fh_osd_apply_alpha(void)
{
    int (*fnSetGraph)(fh_vpss_graph *) = dlsym(fh_vpss.handle, "FH_VPSS_SetGraph");
    fh_vpss_graph graph;
    unsigned char alpha = 0;
    int any = 0;

    for (int i = 0; i < FH_OSD_MAX; i++)
        if (_fh_osd[i].used) { any = 1; if (_fh_osd[i].opal > alpha) alpha = _fh_osd[i].opal; }
    if (!any) alpha = 255;
    if (!_fh_osd_on || !fnSetGraph || alpha == _fh_osd_alpha) { _fh_osd_alpha = alpha; return; }

    _fh_osd_alpha = alpha;
    memset(&graph, 0, sizeof(graph));
    graph.enable = 1;
    graph.physAddr = _fh_osd_phys;
    graph.alpha = _fh_osd_alpha;
    graph.width = _fh_snr_dim.width;
    graph.height = _fh_snr_dim.height;
    graph.stride = _fh_snr_dim.width * 2;
    if (fnSetGraph(&graph))
        HAL_WARNING("fh_osd", "Could not set the overlay alpha to %u\n", _fh_osd_alpha);
}

/* divinus positions regions in main-stream pixels; the plane is at sensor size */
static void fh_osd_scale(hal_rect *rect)
{
    unsigned int sw = _fh_vpss_dim[0].width ? _fh_vpss_dim[0].width : _fh_snr_dim.width;
    unsigned int sh = _fh_vpss_dim[0].height ? _fh_vpss_dim[0].height : _fh_snr_dim.height;
    rect->x = (unsigned int)rect->x * _fh_snr_dim.width / sw;
    rect->y = (unsigned int)rect->y * _fh_snr_dim.height / sh;
}

static void fh_osd_clear(hal_rect rect)
{
    unsigned int w;

    /* rect.x/y are unsigned short and OSD positions are only range-checked
     * against SHRT_MAX, so a region placed past the plane made
     * _fh_snr_dim.width - rect.x wrap and memset most of memory. Reject the
     * rectangle before it is used to derive any width or offset. */
    if (!_fh_osd_virt) return;
    if (rect.x >= _fh_snr_dim.width || rect.y >= _fh_snr_dim.height) return;

    w = rect.width;
    if ((unsigned int)rect.x + w > _fh_snr_dim.width)
        w = _fh_snr_dim.width - rect.x;

    for (unsigned int y = 0; y < rect.height; y++) {
        unsigned int py = (unsigned int)rect.y + y;
        if (py >= _fh_snr_dim.height) break;
        memset(_fh_osd_virt + py * _fh_snr_dim.width + rect.x, 0, w * 2);
    }
}

int fh_region_create(int *handle, hal_rect rect, short opacity)
{
    int ret;
    if (ret = fh_osd_setup())
        return ret;
    if (*handle < 0 || *handle >= FH_OSD_MAX) {
        for (int i = 0; i < FH_OSD_MAX; i++)
            if (!_fh_osd[i].used) { *handle = i; break; }
        if (*handle < 0 || *handle >= FH_OSD_MAX)
            HAL_ERROR("fh_osd", "No free overlay region!\n");
    }
    if (_fh_osd[*handle].used)
        fh_osd_clear(_fh_osd[*handle].rect);
    fh_osd_scale(&rect);
    _fh_osd[*handle].used = 1;
    _fh_osd[*handle].rect = rect;
    _fh_osd[*handle].opal = opacity < 0 ? 0 : (opacity > 255 ? 255 : (unsigned char)opacity);
    fh_osd_apply_alpha();
    return EXIT_SUCCESS;
}

void fh_region_destroy(int *handle)
{
    if (*handle < 0 || *handle >= FH_OSD_MAX || !_fh_osd[*handle].used) return;
    fh_osd_clear(_fh_osd[*handle].rect);
    _fh_osd[*handle].used = 0;
    *handle = -1;
    fh_osd_apply_alpha();
}

/* Bitmaps arrive as BGR555LE; set the alpha bit for opaque pixels (ARGB1555) */
int fh_region_setbitmap(int *handle, hal_bitmap *bitmap)
{

    if (*handle < 0 || *handle >= FH_OSD_MAX || !_fh_osd[*handle].used || !_fh_osd_virt)
        return EXIT_FAILURE;
    fh_osd_rgn *rgn = &_fh_osd[*handle];
    fh_osd_clear(rgn->rect);
    if (rgn->rect.x >= _fh_snr_dim.width || rgn->rect.y >= _fh_snr_dim.height)
        return EXIT_SUCCESS;

    /* The bitmap is rendered in main-stream pixels while this plane covers the
     * whole sensor frame, so it is scaled by the same ratio fh_osd_scale()
     * applies to the position. Scaling only the position left the overlay
     * placed for one resolution and drawn at another whenever the main stream
     * was smaller than the sensor. Nearest neighbour is enough for OSD text. */
    unsigned int sw = _fh_vpss_dim[0].width ? _fh_vpss_dim[0].width : _fh_snr_dim.width;
    unsigned int sh = _fh_vpss_dim[0].height ? _fh_vpss_dim[0].height : _fh_snr_dim.height;
    unsigned int bw = bitmap->dim.width, bh = bitmap->dim.height;
    unsigned int dw = bw * _fh_snr_dim.width / sw;
    unsigned int dh = bh * _fh_snr_dim.height / sh;
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
        if (py >= _fh_snr_dim.height) break;
        unsigned int sy = y * bh / dh;
        unsigned short *dst = _fh_osd_virt + py * _fh_snr_dim.width + rgn->rect.x;
        for (unsigned int x = 0; x < dw && (unsigned int)rgn->rect.x + x < _fh_snr_dim.width; x++) {
            unsigned short p = src[sy * bw + (x * bw / dw)];
            dst[x] = p ? (p | 0x8000) : 0;
        }
    }
    return EXIT_SUCCESS;
}

int fh_video_create(char index, hal_vidconfig *config)
{
    int ret;
    unsigned int type;

    if (index >= FH_VENC_CHN_NUM)
        HAL_ERROR("fh_hal", "Only %d encoder channels are available!\n", FH_VENC_CHN_NUM);

    switch (config->codec) {
        case HAL_VIDCODEC_JPG:  type = FH_VENC_TYPE_JPEG; break;
        case HAL_VIDCODEC_MJPG: type = FH_VENC_TYPE_MJPEG; break;
        case HAL_VIDCODEC_H264: type = FH_VENC_TYPE_H264; break;
        case HAL_VIDCODEC_H265: type = FH_VENC_TYPE_H265; break;
        default: HAL_ERROR("fh_venc", "This codec is not supported by the hardware!\n");
    }

    if (config->width > _fh_snr_dim.width || config->height > _fh_snr_dim.height) {
        config->width = _fh_snr_dim.width;
        config->height = _fh_snr_dim.height;
    }

    /* The scaler output mode must match the encoder before it is configured */
    if (_fh_vpss_src[index] >= 0) {
        if (ret = fh_vpss.fnSetOutputMode(index, type == FH_VENC_TYPE_H265 ? 1 : 0))
            HAL_WARNING("fh_vpss", "Setting output mode on channel %d failed with %#x!\n", index, ret);
    } else {
        /* Shared scaler channel: the encoder must use that channel's size */
        char src = fh_vpss_pick(index);
        config->width = _fh_vpss_dim[src].width;
        config->height = _fh_vpss_dim[src].height;
    }

    if (_fh_venc_made[index].width) {
        /* There is no FH_VENC_DestroyChn: a channel's memory pool stays allocated
         * until FH_SYS_Exit and a second CreateChn fails (-0x3f3). The vendor
         * application creates each channel once with both codecs reserved and
         * switches with SetChnAttr; do the same, but never grow past the pool. */
        if (config->width > _fh_venc_made[index].width || config->height > _fh_venc_made[index].height)
            HAL_ERROR("fh_venc", "Channel %d was created at %ux%u; %ux%u needs a restart\n", index,
                _fh_venc_made[index].width, _fh_venc_made[index].height, config->width, config->height);
    } else {
        /* The vendor application always reserves memory for both H.264 and H.265 */
        fh_venc_cfg cfg = { .types = (type & (FH_VENC_TYPE_H264 | FH_VENC_TYPE_H265)) ?
            (FH_VENC_TYPE_H264 | FH_VENC_TYPE_H265) : type, .width = config->width, .height = config->height };
        if (ret = fh_venc.fnCreateChannel(index, &cfg)) {
            HAL_DANGER("fh_venc", "Creating channel %d (%ux%u) failed with %#x!\n", index, cfg.width, cfg.height, ret);
            return ret;
        }
        _fh_venc_made[index].width = cfg.width;
        _fh_venc_made[index].height = cfg.height;
    }

    if (type == FH_VENC_TYPE_JPEG) {
        fh_venc_jpgattr attr;
        memset(&attr, 0, sizeof(attr));
        attr.type = type;
        attr.quality = MAX(config->maxQual, 1);
        attr.rateIndex = 4;
        if (ret = fh_venc.fnSetChannelConfig(index, &attr))
            return ret;
    } else if (type == FH_VENC_TYPE_MJPEG) {
        /* { type, width, height, rotation(0..3), rateIndex(0..9), ..., rc } */
        fh_venc_attr attr;
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
        if (ret = fh_venc.fnSetChannelConfig(index, &attr)) {
            HAL_DANGER("fh_venc", "Configuring MJPEG channel %d failed with %#x!\n", index, ret);
            return ret;
        }
    } else {
        fh_venc_attr attr;
        memset(&attr, 0, sizeof(attr));
        attr.type = type;
        if (type == FH_VENC_TYPE_H265)
            attr.profile = 1;
        else {
            /* The vendor application only ever sets Main (77); High (100) is
             * rejected by FH_VENC_SetChnAttr with 0x19d and would leave the
             * channel closed, so every H.264 profile maps to Main here */
            if (config->profile != HAL_VIDPROFILE_MAIN)
                HAL_WARNING("fh_venc", "Channel %d: only the H.264 Main profile is supported, using it\n", index);
            attr.profile = 77;
        }
        attr.gop = config->gop ? config->gop : config->framerate * 2;
        attr.width = config->width;
        attr.height = config->height;
        fh_venc_fill_rc(&attr.rc, config, type == FH_VENC_TYPE_H265, config->framerate);
        if (ret = fh_venc.fnSetChannelConfig(index, &attr)) {
            HAL_DANGER("fh_venc", "Configuring channel %d failed with %#x!\n", index, ret);
            return ret;
        }
    }

    _fh_venc_type[index] = type;

    if (type != FH_VENC_TYPE_JPEG && (ret = fh_venc.fnStartReceiving(index))) {
        HAL_DANGER("fh_venc", "Starting channel %d failed with %#x!\n", index, ret);
        return ret;
    }

    fh_state[index].payload = config->codec;

    return EXIT_SUCCESS;
}

int fh_video_destroy(char index)
{
    fh_state[index].enable = 0;
    fh_state[index].payload = HAL_VIDCODEC_UNSPEC;

    fh_venc.fnStopReceiving(index);
    fh_sys.fnUnbindByDst(index);
    _fh_venc_type[index] = 0;
    fh_channel_destroy(index);

    return EXIT_SUCCESS;
}

int fh_video_destroy_all(void)
{
    for (char i = 0; i < FH_VENC_CHN_NUM; i++)
        if (fh_state[i].enable)
            fh_video_destroy(i);

    return EXIT_SUCCESS;
}

void fh_video_request_idr(char index)
{
    fh_venc.fnRequestIdr(index);
}

int fh_video_snapshot_grab(signed char index, hal_jpegdata *jpeg)
{
    int ret = EXIT_FAILURE;
    fh_venc_strm strm;
    unsigned int type = FH_VENC_TYPE_JPEG;

    /* index < 0: serve the most recent MJPEG frame cached by the video thread.
     * A real JPEG encoder shares the main scaler channel at bind time (the VPU
     * only has main + one sub), so snapshots come out at the main resolution;
     * this path is the fallback if that encoder cannot deliver a frame. */
    if (index < 0) {
        for (int w = 0; w < 100; w++) {
            pthread_mutex_lock(&_fh_mjpeg_mtx);
            if (_fh_mjpeg_len) {
                if (_fh_mjpeg_len > jpeg->length) {
                    /* keep the old buffer if this fails; assigning the result
                     * straight to jpeg->data leaked it and then memcpy'd NULL */
                    unsigned char *grown = realloc(jpeg->data, _fh_mjpeg_len);
                    if (!grown) {
                        pthread_mutex_unlock(&_fh_mjpeg_mtx);
                        HAL_ERROR("fh_venc", "Growing the snapshot buffer to %u bytes failed!\n",
                            _fh_mjpeg_len);
                    }
                    jpeg->data = grown;
                    jpeg->length = _fh_mjpeg_len;
                }
                memcpy(jpeg->data, _fh_mjpeg_cache, _fh_mjpeg_len);
                jpeg->jpegSize = _fh_mjpeg_len;
                pthread_mutex_unlock(&_fh_mjpeg_mtx);
                return EXIT_SUCCESS;
            }
            pthread_mutex_unlock(&_fh_mjpeg_mtx);
            usleep(20000);
        }
        HAL_ERROR("fh_venc", "No MJPEG frame available for snapshot!\n");
    } else if (_fh_venc_type[index] != FH_VENC_TYPE_JPEG)
        HAL_ERROR("fh_venc", "Channel %d is not a JPEG encoder!\n", index);

    pthread_mutex_lock(&_fh_strm_mtx);

    {
        char src = _fh_vpss_src[index] < 0 ? fh_vpss_pick(index) : _fh_vpss_src[index];
        if (ret = fh_sys.fnBindVpu2Enc(src, index)) {
            HAL_DANGER("fh_venc", "Binding the encoder channel %d failed with %#x!\n", index, ret);
            goto fallback;
        }
    }

    for (int i = 0; i < 100; i++) {
        memset(&strm, 0, sizeof(strm));
        strm.type = type;
        if (!(ret = fh_venc.fnGetStream(type, &strm)))
            break;
        usleep(20000);
    }
    if (ret) {
        HAL_DANGER("fh_venc", "Getting a JPEG frame timed out (%#x)!\n", ret);
        goto fallback;
    }

    {
        /* JPEG: base = data address, frameType = length. MJPEG: NALU-style packets. */
        if (type == FH_VENC_TYPE_JPEG) {
            unsigned int len = strm.frameType;
            if (len > jpeg->length) {
                unsigned char *grown = realloc(jpeg->data, len);
                if (!grown) {
                    HAL_DANGER("fh_venc", "Growing the snapshot buffer to %u bytes failed!\n", len);
                    fh_venc.fnReleaseStream(strm.channel);
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
                    HAL_DANGER("fh_venc", "Growing the snapshot buffer to %u bytes failed!\n", total);
                    fh_venc.fnReleaseStream(strm.channel);
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
    fh_venc.fnReleaseStream(strm.channel);
    ret = EXIT_SUCCESS;

abort:
    fh_sys.fnUnbindByDst(index);
    pthread_mutex_unlock(&_fh_strm_mtx);

    return ret;

fallback:
    fh_sys.fnUnbindByDst(index);
    pthread_mutex_unlock(&_fh_strm_mtx);
    HAL_WARNING("fh_venc", "Falling back to the MJPEG sub-stream for the snapshot\n");

    return fh_video_snapshot_grab(-1, jpeg);
}

void *fh_video_thread(void)
{
    unsigned int mask = FH_VENC_TYPE_H264 | FH_VENC_TYPE_H265 | FH_VENC_TYPE_MJPEG;
    fh_venc_strm strm;

    while (keepRunning) {
        char active = 0;
        for (int i = 0; i < FH_VENC_CHN_NUM; i++)
            if (fh_state[i].enable && fh_state[i].mainLoop) active = 1;
        if (!active) {
            usleep(100000);
            continue;
        }

        memset(&strm, 0, sizeof(strm));
        strm.type = mask;
        if (fh_venc.fnGetStreamBlocking(mask, &strm)) {
            usleep(10000);
            continue;
        }

        char index = strm.channel;
        /* The stream lives in the SDK's DMA buffer, which is mapped uncached:
         * every pass over it (start-code scans, RTP payload copies) reads at
         * bus speed and a 300 KB keyframe cost ~400 ms of CPU. Copy each frame
         * once into a cached buffer, hand the SDK buffer back at once, and let
         * every consumer work on the copy. */
        {
            static unsigned char *vbuf; static unsigned int vcap;
            unsigned int need = 0, off = 0;
            if (strm.type == FH_VENC_TYPE_MJPEG || _fh_venc_type[index] == FH_VENC_TYPE_MJPEG)
                need = strm.frameType;
            else for (unsigned int i = 0; i < strm.naluCount && i < 28; i++) need += strm.nalu[i].length;
            if (need > vcap) {
                unsigned char *n = realloc(vbuf, need + 65536);
                if (n) { vbuf = n; vcap = need + 65536; }
            }
            if (vbuf && need <= vcap) {
                if (strm.type == FH_VENC_TYPE_MJPEG || _fh_venc_type[index] == FH_VENC_TYPE_MJPEG) {
                    memcpy(vbuf, (void*)strm.base, need);
                    strm.base = (unsigned int)vbuf;
                } else for (unsigned int i = 0; i < strm.naluCount && i < 28; i++) {
                    memcpy(vbuf + off, (void*)strm.nalu[i].addr, strm.nalu[i].length);
                    strm.nalu[i].addr = (unsigned int)(vbuf + off);
                    off += strm.nalu[i].length;
                }
                fh_venc.fnReleaseStream(strm.channel);
                strm.channel |= 0x100;   /* released: skip the release below */
            }
        }
        if (index < FH_VENC_CHN_NUM && fh_state[index].enable && fh_vid_cb) {
            /* The stream record's type field does not match the fh_venc_type the
             * channel was configured with for HEVC (VPS/SPS/PPS were parsed with
             * H.264 rules and never recognised), so go by what was configured */
            unsigned int chnType = _fh_venc_type[index];
            char h265 = chnType == FH_VENC_TYPE_H265;
            hal_vidstream outStrm;
            hal_vidpack outPack[28];
            unsigned long long pts = ((unsigned long long)strm.ptsHigh << 32) | strm.ptsLow;
            memset(outPack, 0, sizeof(outPack));
            outStrm.seq = 0;
            outStrm.pack = outPack;

            if (chnType == FH_VENC_TYPE_MJPEG || strm.type == FH_VENC_TYPE_MJPEG) {
                /* MJPEG/JPEG frames arrive whole at base/frameType, not as NALUs */
                unsigned int mlen = strm.frameType;
                pthread_mutex_lock(&_fh_mjpeg_mtx);
                if (mlen > _fh_mjpeg_cap) {
                    _fh_mjpeg_cache = realloc(_fh_mjpeg_cache, mlen);
                    _fh_mjpeg_cap = mlen;
                }
                if (_fh_mjpeg_cache) {
                    memcpy(_fh_mjpeg_cache, (void*)strm.base, mlen);
                    _fh_mjpeg_len = mlen;
                }
                pthread_mutex_unlock(&_fh_mjpeg_mtx);
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
            (*fh_vid_cb)(index, &outStrm);
        }

        if (!(strm.channel & 0x100))
            fh_venc.fnReleaseStream(strm.channel);
    }
    HAL_INFO("fh_venc", "Shutting down encoding thread...\n");
    return NULL;
}

void fh_system_deinit(void)
{
    fh_sys.fnExit();
}

int fh_system_init(char *snrConfig)
{
    {
        fh_sys_ver version;
        if (!fh_sys.fnGetVersion(&version))
            HAL_INFO("fh_hal", "SDK build %08x (%08x), package id %#x\n",
                version.date, version.commit, version.package);
    }

    _fh_snr = NULL;
    for (int i = 0; fh_snr_drivers[i]; i++) {
        if (fh_snr_drivers[i]->fnProbe()) continue;
        _fh_snr = fh_snr_drivers[i];
        break;
    }
    if (!_fh_snr)
        HAL_ERROR("fh_hal", "No supported sensor found on the I2C bus!\n");

    _fh_snr_dim = _fh_snr->dim;
    strncpy(sensor, _fh_snr->name, sizeof(sensor) - 1);
    HAL_INFO("fh_hal", "Detected sensor %s (%ux%u)\n", _fh_snr->name,
        _fh_snr_dim.width, _fh_snr_dim.height);

    return EXIT_SUCCESS;
}

int fh_night_available(void)
{
    return _fh_smartir;
}

/* 1 = scene is dark (night), 0 = day. Fed the previous decision for hysteresis. */
/*
 * Illuminators (from the vendor application's gpio table and lamp code): each
 * lamp has an enable GPIO and a brightness PWM, set through /proc/driver/pwm as
 * "id,output_mask,duty_ns,period_ns,pulses,delay_ns,phase_ns,stop" (the mask is
 * written raw to GLOBAL_CTRL2, so 1 << id). IR: GPIO7 + PWM3. White light:
 * GPIO11 + PWM7 (the board file calls GPIO11 "PHY reset"; it is not wired as
 * one on the PB1). GPIO11 is pulled up, so the white light is on from reset
 * until something drives it low.
 */
#define FH_PWM_ON(id, mask)  id "," mask ",60,200,0,0,0,0"   /* 30%, the driver default */
#define FH_PWM_OFF(id, mask) id "," mask ",0,200,0,0,0,0"    /* held low */
#define FH_WHITELAMP_GPIO 11

int fh_irled(char enable)
{
    /* Brightness only; the enable is GPIO7, which divinus drives as ir_led_pin */
    fh_proc_write("/proc/driver/pwm", enable ? FH_PWM_ON("3", "8") : FH_PWM_OFF("3", "8"));
    return EXIT_SUCCESS;
}

int fh_whitelamp(char enable)
{
    fh_proc_write("/proc/driver/pwm", enable ? FH_PWM_ON("7", "128") : FH_PWM_OFF("7", "128"));
    gpio_write(FH_WHITELAMP_GPIO, enable);
    return EXIT_SUCCESS;
}

/* Override SmartIR's gain thresholds (th[0] day->night, th[1] night->day); 0 keeps the default */
int fh_night_thresholds(unsigned short night, unsigned short day)
{
    unsigned short th[4] = {0};
    if (!_fh_smartir || !fh_isp.fnSmartIrGetThreshold || !fh_isp.fnSmartIrSetThreshold)
        return EXIT_FAILURE;
    if (fh_isp.fnSmartIrGetThreshold(th))
        return EXIT_FAILURE;
    if (night) th[0] = night;
    if (day) th[1] = day;
    HAL_INFO("fh_hal", "SmartIR thresholds set to %u %u %u %u\n", th[0], th[1], th[2], th[3]);
    return fh_isp.fnSmartIrSetThreshold(th);
}

int fh_night_status(void)
{
    if (!_fh_smartir)
        return 0;
    {
        static int lastRaw = -1, calls = 0;
        int raw = fh_isp.fnSmartIrStatus(_fh_night_prev);
        /* Log every verdict change and a sample every ~10 min (at 40 ms polls) with
         * the exposure/gain/total the library decided on and the live thresholds */
        if (raw != lastRaw || !(++calls % 15000)) {
            unsigned int ae[16] = {0};
            unsigned short th[4] = {0};
            if (fh_isp.fnGetAeInfo) fh_isp.fnGetAeInfo(ae);
            if (fh_isp.fnSmartIrGetThreshold) fh_isp.fnSmartIrGetThreshold(th);
            HAL_INFO("fh_hal", "SmartIR raw status %d (prev %d): exp %u gain %u total %u, thresholds %u %u\n",
                raw, _fh_night_prev, ae[0], ae[1], ae[4], th[0], th[1]);
            lastRaw = raw;
        }
        _fh_night_prev = raw ? 1 : 0;
    }
    return _fh_night_prev;
}

#endif
