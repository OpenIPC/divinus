#if defined(__ARM_PCS_VFP)

#include "i3_hal.h"

/*
 * SigmaStar Infinity3 (MSC313E / MSC316DC / MSC316DM) HAL.
 *
 * The call sequence and structure layouts below were verified on hardware
 * (Imou LOOC 2, OV2735) against the vendor libmi.so, not taken from headers:
 *   - libmi.so and libmi_isp.so reference each other's symbols, so they must be
 *     in the process before us (LD_PRELOAD both); dlopen() of one alone fails.
 *   - MI_VENC_GetStreamTimeOut() hands back one Annex-B buffer per frame:
 *     word 0 = data pointer, word 1 = length, words 2-3 = timestamp, word 4 = type.
 *   - Rate control modes are 0 none, 1 VBR, 2 CBR, 3 fixed QP: VBR and CBR
 *     are the other way round from later chips (_MI_VENC_SetRcParamEx, called
 *     from MI_VENC_CreateChn, hands 1 to _MI_VENC_SetVbr and 2 to
 *     _MI_VENC_SetCbr). With 0 the H.264 encoder ignores the bitrate and sits
 *     at QP 48; the JPEG encoder is left at 0, its quality sets the size.
 */

typedef struct { unsigned int width, height; } i3_dim;
typedef struct { int enable; unsigned int x, y, width, height; } i3_crop;
typedef struct { int source; float srcFps; i3_crop crop; int reserved[1]; } i3_vi_dev;
typedef struct { i3_dim dest; int pixFmt; int mode; void *reserved1; int reserved2[1]; } i3_vi_chn;

typedef struct {
    int codec;
    union { struct { unsigned int profile; int reserved[1]; } h26x; unsigned int pad[2]; };
    i3_dim capt; int pixFmt; unsigned int dstFps; int mode; int reserved[1];
} i3_venc_attr;
typedef struct { unsigned int gop, statTime, bitrate, avgLvl, maxQual, minQual; int ipQpDelta; int reserved[4]; } i3_rate_cbr;
typedef struct { unsigned int gop, statTime, maxBitrate, maxIQual, minIQual, maxPQual, minPQual; int ipQpDelta; int chgPos; } i3_rate_vbr;
typedef struct {
    int mode;
    union { i3_rate_cbr cbr; i3_rate_vbr vbr; unsigned int pad[12]; };
    unsigned int rowQpDelta; void *extend; unsigned int rcVersion;
} i3_venc_rate;
typedef struct { i3_venc_attr attrib; i3_venc_rate rate; } i3_venc_chn;
typedef struct { int module; unsigned int device, channel; } i3_sys_bind;

#define I3_VI_SRC_CAMERA   12
#define I3_PIXFMT_YUV420SP 5
#define I3_MODE_TUNNEL     0
#define I3_SYS_MOD_VI      4
#define I3_SYS_MOD_VENC    5
#define I3_STRM_WORDS      32

static struct {
    void *handle;
    int (*sysInit)(void);
    int (*sysExit)(void);
    int (*sysBind)(i3_sys_bind*, i3_sys_bind*);
    int (*sysUnbind)(i3_sys_bind*, i3_sys_bind*);
    int (*viSetDev)(i3_vi_dev*);
    int (*viSetChn)(int, i3_vi_chn*);
    int (*viEnable)(int);
    int (*viDisable)(int);
    int (*vencCreate)(int, i3_venc_chn*);
    int (*vencDestroy)(int);
    int (*vencStart)(int);
    int (*vencStop)(int);
    int (*vencGet)(int, void*, unsigned int);
    int (*vencRelease)(int, void*);
    int (*vencIdr)(int, char);
} i3;

hal_chnstate i3_state[I3_VENC_CHN_NUM] = {0};
int (*i3_aud_cb)(hal_audframe*);
int (*i3_vid_cb)(char, hal_vidstream*);

static char i3_vi_on[I3_VENC_CHN_NUM], i3_venc_on[I3_VENC_CHN_NUM], i3_bound[I3_VENC_CHN_NUM];
static i3_dim i3_chn_dim[I3_VENC_CHN_NUM];
// VI channel feeding each encoder. Normally the same index; snapshots use the
// camera component's dedicated still-image port (VI channel 3), the only one
// besides the two video ports that an encoder will bind to.
static char i3_vi_of[I3_VENC_CHN_NUM] = { 0, 1, 2 };
#define I3_VI_CHN_IMAGE 3

#define I3_LOAD(field, name) \
    if (!(i3.field = hal_symbol_load("i3_hal", i3.handle, name))) return EXIT_FAILURE

/*
 * The vendor libraries read tuning values ("mi.vi.sub.width" and so on) through
 * an Android-style property store that the stock firmware fills with a
 * property service. There is no such service here, so the store is written
 * directly: a 32 KiB area in /dev/__properties__ in the classic bionic layout
 * (count, serial, magic, version, table of contents at 32, 128-byte records
 * from 1024: name[32], serial = value length << 24, value[92]).
 * Call i3_property_set() before i3_hal_init().
 */
#define I3_PROP_SIZE 32768
#define I3_PROP_MAX 64
static struct { char name[32]; char value[92]; } i3_props[I3_PROP_MAX];
static int i3_prop_num;

void i3_property_set(const char *name, const char *value)
{
    int i;
    for (i = 0; i < i3_prop_num; i++)
        if (!strcmp(i3_props[i].name, name)) break;
    if (i == I3_PROP_MAX || strlen(name) >= sizeof(i3_props[0].name) ||
        strlen(value) >= sizeof(i3_props[0].value)) return;
    strcpy(i3_props[i].name, name);
    strcpy(i3_props[i].value, value);
    if (i == i3_prop_num) i3_prop_num++;
}

static void i3_property_commit(void)
{
    if (!i3_prop_num) return;
    unsigned char *area = calloc(1, I3_PROP_SIZE);
    if (!area) return;
    unsigned int *head = (unsigned int*)area;
    head[0] = i3_prop_num;
    head[2] = 0x504f5250;
    head[3] = 0x45434f76;
    for (int i = 0; i < i3_prop_num; i++) {
        unsigned int off = 1024 + i * 128;
        head[8 + i] = (unsigned int)strlen(i3_props[i].name) << 24 | off;
        strcpy((char*)area + off, i3_props[i].name);
        *(unsigned int*)(area + off + 32) = (unsigned int)strlen(i3_props[i].value) << 24;
        strcpy((char*)area + off + 36, i3_props[i].value);
    }
    // The reader insists on a root-owned file that others cannot write
    FILE *file = fopen("/dev/__properties__", "wb");
    if (file) {
        fwrite(area, 1, I3_PROP_SIZE, file);
        fclose(file);
        chmod("/dev/__properties__", 0644);
    }
    free(area);

    // OpenIPC's own libat.so keeps the settings in the process instead (read
    // from /etc/sigmastar.conf), so hand them to it as well where it is used.
    void *at = dlopen("libat.so", RTLD_LAZY | RTLD_GLOBAL);
    if (at) {
        int (*set)(const char*, const char*) = dlsym(at, "property_set");
        if (set && !dlsym(at, "__system_property_area__"))
            for (int i = 0; i < i3_prop_num; i++)
                set(i3_props[i].name, i3_props[i].value);
    }
}

void i3_hal_deinit(void)
{
    if (i3.handle) dlclose(i3.handle);
    memset(&i3, 0, sizeof(i3));
}

int i3_hal_init(void)
{
    i3_property_commit();

    // libmi.so and libmi_isp.so use each other's symbols. glibc resolves that
    // lazily when both are preloaded; musl needs libmi_isp.so opened first with
    // RTLD_GLOBAL, so its pending symbols are bound when libmi.so arrives.
    if (!dlopen("libmi_isp.so", RTLD_LAZY | RTLD_GLOBAL))
        HAL_WARNING("i3_hal", "libmi_isp.so: %s\n", dlerror());
    if (!(i3.handle = dlopen("libmi.so", RTLD_LAZY | RTLD_GLOBAL)))
        HAL_ERROR("i3_hal", "Failed to load libmi.so (it needs libmi_isp.so "
            "preloaded alongside it)!\nError: %s\n", dlerror());

    I3_LOAD(sysInit, "MI_SYS_Init");
    I3_LOAD(sysExit, "MI_SYS_Exit");
    I3_LOAD(sysBind, "MI_SYS_Bind");
    I3_LOAD(sysUnbind, "MI_SYS_UnBind");
    I3_LOAD(viSetDev, "MI_VI_SetDevAttr");
    I3_LOAD(viSetChn, "MI_VI_SetChnAttr");
    I3_LOAD(viEnable, "MI_VI_EnableChn");
    I3_LOAD(viDisable, "MI_VI_DisableChn");
    I3_LOAD(vencCreate, "MI_VENC_CreateChn");
    I3_LOAD(vencDestroy, "MI_VENC_DestroyChn");
    I3_LOAD(vencStart, "MI_VENC_StartRecvPic");
    I3_LOAD(vencStop, "MI_VENC_StopRecvPic");
    I3_LOAD(vencGet, "MI_VENC_GetStreamTimeOut");
    I3_LOAD(vencRelease, "MI_VENC_ReleaseStream");
    I3_LOAD(vencIdr, "MI_VENC_RequestIDR");

    return EXIT_SUCCESS;
}

/*
 * Audio input goes through the kernel's ALSA device (card 0, "infinity_snd_machine")
 * using the tinyalsa library the vendor package ships. MI_AI exists in libmi.so but
 * its channel start fails without vendor-side setup we have no description of,
 * while ALSA capture works out of the box.
 */
typedef struct {
    unsigned int channels, rate, period_size, period_count;
    int format;
    unsigned int start_threshold, stop_threshold, silence_threshold;
    int avail_min;
    unsigned int pad[8];
} i3_pcm_config;

#define I3_PCM_IN 0x10000000
#define I3_AUD_SAMPLES 640

static struct {
    void *handle, *pcm;
    unsigned int rate;
    void *(*pcmOpen)(unsigned int, unsigned int, unsigned int, i3_pcm_config*);
    int (*pcmReady)(void*);
    int (*pcmRead)(void*, void*, unsigned int);
    int (*pcmClose)(void*);
    void *(*mixerOpen)(unsigned int);
    void (*mixerClose)(void*);
    void *(*mixerByName)(void*, const char*);
    int (*mixerSet)(void*, unsigned int, int);
    int (*mixerSetEnum)(void*, const char*);
} i3a;

static pthread_t i3_spk_pid;
static char i3_spk_run;

void i3_audio_deinit(void)
{
    if (i3_spk_run) { i3_spk_run = 0; pthread_join(i3_spk_pid, NULL); }
    if (i3a.pcm) { i3a.pcmClose(i3a.pcm); i3a.pcm = NULL; }
    if (i3a.handle) { dlclose(i3a.handle); i3a.handle = NULL; }
}

int i3_audio_init(int samplerate, int gain)
{
    if (!(i3a.handle = dlopen("libtinyalsa.so", RTLD_LAZY)))
        HAL_ERROR("i3_aud", "Failed to load libtinyalsa.so!\nError: %s\n", dlerror());

    if (!(i3a.pcmOpen = hal_symbol_load("i3_aud", i3a.handle, "pcm_open")) ||
        !(i3a.pcmReady = hal_symbol_load("i3_aud", i3a.handle, "pcm_is_ready")) ||
        !(i3a.pcmRead = hal_symbol_load("i3_aud", i3a.handle, "pcm_read")) ||
        !(i3a.pcmClose = hal_symbol_load("i3_aud", i3a.handle, "pcm_close")))
        return EXIT_FAILURE;
    i3a.mixerOpen = dlsym(i3a.handle, "mixer_open");
    i3a.mixerClose = dlsym(i3a.handle, "mixer_close");
    i3a.mixerByName = dlsym(i3a.handle, "mixer_get_ctl_by_name");
    i3a.mixerSet = dlsym(i3a.handle, "mixer_ctl_set_value");
    i3a.mixerSetEnum = dlsym(i3a.handle, "mixer_ctl_set_enum_by_string");

    // Select the microphone and map the generic gain (dB-ish, 0..) onto the
    // codec's capture volume range (0..94, 64 is the driver default).
    void *mixer;
    if (i3a.mixerOpen && i3a.mixerByName && (mixer = i3a.mixerOpen(0))) {
        void *ctl;
        if (i3a.mixerSetEnum && (ctl = i3a.mixerByName(mixer, "ADC Mux")))
            i3a.mixerSetEnum(ctl, "Mic-in");
        if (i3a.mixerSet && gain > 0 && (ctl = i3a.mixerByName(mixer, "Main Capture Volume")))
            i3a.mixerSet(ctl, 0, MIN(MAX(gain, 0), 94));
        if (i3a.mixerClose) i3a.mixerClose(mixer);
    }

    i3_pcm_config config;
    memset(&config, 0, sizeof(config));
    config.channels = 1;
    config.rate = samplerate;
    i3a.rate = samplerate;
    config.period_size = I3_AUD_SAMPLES;
    config.period_count = 4;
    config.format = 0; // S16_LE
    i3a.pcm = i3a.pcmOpen(0, 0, I3_PCM_IN, &config);
    if (!i3a.pcm || !i3a.pcmReady(i3a.pcm)) {
        if (i3a.pcm) { i3a.pcmClose(i3a.pcm); i3a.pcm = NULL; }
        HAL_ERROR("i3_aud", "Opening the capture device at %d Hz failed!\n", samplerate);
    }

    return EXIT_SUCCESS;
}

/*
 * Speaker (talk-back). PCM handed to i3_speaker_write() is queued and played by
 * a thread through the same ALSA card. The amplifier on this board (Imou LOOC 2)
 * is not the kernel's amp-gpio: it sits on GPIO 18 (PAD_FUART_CTS, RIU 0x103c2c,
 * bit 4 out / bit 5 oen) and wants the vendor's enable sequence - idle low,
 * seven fast low/high pulses, then high (pdc.ko gPaCfg). Sysfs GPIO is far too
 * slow for that, so the pad register is driven directly.
 */
#include <fcntl.h>
#include <pthread.h>
#include <sys/mman.h>

#define I3_SPK_RING (16000 * 2)
static short i3_spk_ring[I3_SPK_RING];
static unsigned int i3_spk_head, i3_spk_fill;
static pthread_mutex_t i3_spk_mtx = PTHREAD_MUTEX_INITIALIZER;
static void i3_spk_spin(int n) { volatile int i; for (i = 0; i < n; i++); }

static void i3_speaker_amp(char on)
{
    int fd = open("/dev/mem", O_RDWR | O_SYNC);
    if (fd < 0) return;
    void *map = mmap(NULL, 0x1000, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0x1F207000);
    close(fd);
    if (map == MAP_FAILED) return;
    volatile unsigned short *reg = (volatile unsigned short*)((char*)map + 0x858);
    unsigned short lo = *reg & ~0x30, hi = lo | 0x10;
    *reg = lo;
    if (on) {
        usleep(2000);
        for (int i = 0; i < 7; i++) { *reg = lo; i3_spk_spin(75); *reg = hi; i3_spk_spin(75); }
        *reg = hi;
    }
    munmap(map, 0x1000);
}

static void *i3_speaker_thread(void *arg)
{
    static short period[I3_AUD_SAMPLES];
    void *pcm = NULL;
    int (*pcmWrite)(void*, const void*, unsigned int) = dlsym(i3a.handle, "pcm_write");
    unsigned int idle = 0;

    while (keepRunning && i3_spk_run && pcmWrite) {
        pthread_mutex_lock(&i3_spk_mtx);
        unsigned int n = MIN(i3_spk_fill, I3_AUD_SAMPLES);
        for (unsigned int i = 0; i < n; i++)
            period[i] = i3_spk_ring[(i3_spk_head + i) % I3_SPK_RING];
        i3_spk_head = (i3_spk_head + n) % I3_SPK_RING;
        i3_spk_fill -= n;
        pthread_mutex_unlock(&i3_spk_mtx);

        if (!n) {
            // Nothing to say: play silence for a moment (network gaps), then
            // switch the amplifier off and release the device.
            if (pcm && ++idle > 20) {
                i3_speaker_amp(0);
                i3a.pcmClose(pcm);
                pcm = NULL;
            }
            if (!pcm) { usleep(20000); continue; }
        } else idle = 0;
        memset(period + n, 0, (I3_AUD_SAMPLES - n) * sizeof(short));

        if (!pcm) {
            i3_pcm_config config;
            memset(&config, 0, sizeof(config));
            config.channels = 1;
            config.rate = 16000;
            config.period_size = I3_AUD_SAMPLES;
            config.period_count = 4;
            pcm = i3a.pcmOpen(0, 0, 0, &config);
            if (!pcm || !i3a.pcmReady(pcm)) {
                if (pcm) i3a.pcmClose(pcm);
                pcm = NULL;
                usleep(200000);
                continue;
            }
            // DAC first, then the amplifier, so it does not pop
            static short silence[I3_AUD_SAMPLES];
            pcmWrite(pcm, silence, sizeof(silence));
            i3_speaker_amp(1);
        }
        if (pcmWrite(pcm, period, sizeof(period))) usleep(10000);
    }

    i3_speaker_amp(0);
    if (pcm) i3a.pcmClose(pcm);
    return NULL;
}

// Codec playback volume, 0..94 (the driver default is 64); -1 only reads it
int i3_speaker_volume(int volume)
{
    static int current = -1;
    void *mixer, *ctl;
    if (volume >= 0 && i3a.mixerOpen && i3a.mixerByName && i3a.mixerSet && (mixer = i3a.mixerOpen(0))) {
        if ((ctl = i3a.mixerByName(mixer, "Main Playback Volume")) &&
            !i3a.mixerSet(ctl, 0, MIN(volume, 94)))
            current = MIN(volume, 94);
        if (i3a.mixerClose) i3a.mixerClose(mixer);
    }
    return current;
}

// 16 kHz mono signed 16-bit samples for the speaker
int i3_speaker_write(const short *pcm, unsigned int samples)
{
    if (!i3a.handle || !i3a.pcmOpen) return EXIT_FAILURE;
    if (!i3_spk_run) {
        i3_spk_run = 1;
        if (pthread_create(&i3_spk_pid, NULL, i3_speaker_thread, NULL)) {
            i3_spk_run = 0;
            return EXIT_FAILURE;
        }
    }
    pthread_mutex_lock(&i3_spk_mtx);
    for (unsigned int i = 0; i < samples; i++) {
        if (i3_spk_fill == I3_SPK_RING) { // full: drop the oldest
            i3_spk_head = (i3_spk_head + 1) % I3_SPK_RING;
            i3_spk_fill--;
        }
        i3_spk_ring[(i3_spk_head + i3_spk_fill++) % I3_SPK_RING] = pcm[i];
    }
    pthread_mutex_unlock(&i3_spk_mtx);
    return EXIT_SUCCESS;
}

void *i3_audio_thread(void)
{
    static short buffer[I3_AUD_SAMPLES];
    unsigned int seq = 0, rate = i3a.rate ? i3a.rate : 16000;
    unsigned long long samples = 0;

    while (keepRunning && audioOn && i3a.pcm) {
        if (i3a.pcmRead(i3a.pcm, buffer, sizeof(buffer))) {
            usleep(10000);
            continue;
        }

        if (i3_aud_cb) {
            hal_audframe outFrame;
            memset(&outFrame, 0, sizeof(outFrame));
            outFrame.channelCnt = 1;
            outFrame.data[0] = (unsigned char*)buffer;
            outFrame.length[0] = sizeof(buffer);
            outFrame.seq = seq++;
            // ALSA gives no capture timestamps; derive them from the sample count.
            outFrame.timestamp = (unsigned int)(samples * 1000ULL / rate);
            samples += I3_AUD_SAMPLES;
            (i3_aud_cb)(&outFrame);
        }
    }
    HAL_INFO("i3_aud", "Shutting down capture thread...\n");
    return NULL;
}

int i3_channel_create(char index, short width, short height, char jpeg)
{
    int ret;
    if (index < 0 || index >= I3_VENC_CHN_NUM) return EXIT_FAILURE;

    i3_vi_chn channel;
    memset(&channel, 0, sizeof(channel));
    channel.dest.width = width;
    channel.dest.height = height;
    channel.pixFmt = I3_PIXFMT_YUV420SP;
    channel.mode = I3_MODE_TUNNEL;
    if (ret = i3.viSetChn(index, &channel))
        return ret;

    i3_chn_dim[index].width = width;
    i3_chn_dim[index].height = height;
    return EXIT_SUCCESS;
}

int i3_channel_bind(char index, char framerate)
{
    int ret;
    i3_sys_bind source = { I3_SYS_MOD_VI, 0, i3_vi_of[index] };
    i3_sys_bind dest = { I3_SYS_MOD_VENC, 0, index };

    if (!i3_vi_on[index]) {
        if (ret = i3.viEnable(i3_vi_of[index]))
            return ret;
        i3_vi_on[index] = 1;
    }

    if (!i3_bound[index]) {
        if (ret = i3.sysBind(&source, &dest))
            return ret;
        i3_bound[index] = 1;
    }

    return i3.vencStart(index);
}

// Grayscale for night mode is done by the ISP, so every channel (streams and
// snapshots) turns gray. MI_ISP_SetColorToGray(chn, attr) takes the attribute
// by value; its layout is the ColorToGray page of I3_isp_api.xml: Enable,
// WBCtrlEnable, OpType (0 auto, 1 manual), Auto.Enable[16], Manual.Enable.
// MI_VENC_SetColor2Grey() is only the fallback: it leaves the bottom of a
// 1080p frame in colour and the MJPEG encoder does not support it at all.
typedef struct {
    unsigned int enable, wbCtrlEnable, opType;
    unsigned int autoEnable[16];
    unsigned int manualEnable;
} i3_isp_colortogray;

int i3_channel_grayscale(char enable)
{
    int (*getGray)(int, i3_isp_colortogray*) =
        dlsym(RTLD_DEFAULT, "MI_ISP_GetColorToGray");
    int (*setGray)(int, i3_isp_colortogray) =
        dlsym(RTLD_DEFAULT, "MI_ISP_SetColorToGray");

    if (getGray && setGray) {
        i3_isp_colortogray attr;
        memset(&attr, 0, sizeof(attr));
        getGray(0, &attr);
        attr.enable = 1;
        attr.opType = 1;
        attr.manualEnable = enable ? 1 : 0;
        if (!setGray(0, attr))
            return EXIT_SUCCESS;
    }

    int (*setGrey)(int, int*) = dlsym(i3.handle, "MI_VENC_SetColor2Grey");
    if (!setGrey) return EXIT_FAILURE;

    int ret = EXIT_SUCCESS, grey = enable ? 1 : 0;
    for (char i = 0; i < I3_VENC_CHN_NUM; i++)
        if (i3_venc_on[i] && (i3_state[i].payload == HAL_VIDCODEC_H264 ||
            i3_state[i].payload == HAL_VIDCODEC_H265) && setGrey(i, &grey))
            ret = EXIT_FAILURE;
    return ret;
}

/*
 * On-screen display through the vendor MI_OSD "widget" API (signatures recovered
 * from libmi.so). Bitmap widgets need the hardware graphics plane, which the
 * library keeps off (mi.osd.gop.use, also 0 in the stock firmware; forcing it
 * to 1 or 3 through the property store kills the pipeline at start), so this
 * uses the software-blended YUV widgets the stock firmware uses:
 *   MI_OSD_CreateYuvWidget(&handle, viChn, data, size, &position, width, height)
 *   MI_OSD_UpdateYuvWidget(handle, data, size, &position, width, height)
 *   MI_OSD_UpdateCanvas(viChn), MI_OSD_DestroyWidget(handle)
 * The picture is NV12 (Y plane, then interleaved UV at half height) and a Y of
 * zero marks a transparent pixel. Widgets are attached to a VI channel, so
 * they show up on the stream encoded from it.
 */
#define I3_OSD_NUM 10
typedef struct { unsigned int x, y; } i3_osd_pos;
// One widget per region and encoder channel: the main stream, a sub-stream and
// the snapshot channel come from different VI channels at different sizes.
static void *i3_osd_widget[I3_OSD_NUM][I3_VENC_CHN_NUM];
static hal_rect i3_osd_rect[I3_OSD_NUM];
static char i3_osd_plate[I3_OSD_NUM];

void i3_region_init(void) { memset(i3_osd_widget, 0, sizeof(i3_osd_widget)); }

void i3_region_destroy(char handle)
{
    int (*destroy)(void*) = dlsym(i3.handle, "MI_OSD_DestroyWidget");
    int (*canvas)(int) = dlsym(i3.handle, "MI_OSD_UpdateCanvas");
    if (handle < 0 || handle >= I3_OSD_NUM || !destroy) return;
    for (char c = 0; c < I3_VENC_CHN_NUM; c++) {
        if (!i3_osd_widget[handle][c]) continue;
        destroy(i3_osd_widget[handle][c]);
        i3_osd_widget[handle][c] = NULL;
        if (canvas) canvas(i3_vi_of[c]);
    }
}

void i3_region_deinit(void)
{
    for (char i = 0; i < I3_OSD_NUM; i++) i3_region_destroy(i);
}

int i3_region_create(char handle, hal_rect rect, short opacity)
{
    if (handle < 0 || handle >= I3_OSD_NUM) return EXIT_FAILURE;
    i3_osd_rect[handle] = rect;
    i3_osd_plate[handle] = opacity < 255; // translucent: text on a plate
    return EXIT_SUCCESS;
}

int i3_region_setbitmap(int handle, hal_bitmap *bitmap)
{
    int (*create)(void**, int, void*, unsigned int, i3_osd_pos*, unsigned int, unsigned int) =
        dlsym(i3.handle, "MI_OSD_CreateYuvWidget");
    int (*update)(void*, void*, unsigned int, i3_osd_pos*, unsigned int, unsigned int) =
        dlsym(i3.handle, "MI_OSD_UpdateYuvWidget");
    int (*canvas)(int) = dlsym(i3.handle, "MI_OSD_UpdateCanvas");
    if (handle < 0 || handle >= I3_OSD_NUM || !create || !update || !bitmap->data)
        return EXIT_FAILURE;
    if (!i3_chn_dim[0].width || !i3_chn_dim[0].height) return EXIT_FAILURE;

    const unsigned short *src = bitmap->data;
    unsigned int sw = bitmap->dim.width, sh = bitmap->dim.height;
    int ret = EXIT_SUCCESS;

    for (char c = 0; c < I3_VENC_CHN_NUM; c++) {
        if (!i3_venc_on[c] || !i3_chn_dim[c].width) continue;
        // Positions and sizes are given for the main stream; other channels
        // get the same picture scaled to their frame (16.16 fixed point).
        unsigned int scale = ((unsigned int)i3_chn_dim[c].width << 16) / i3_chn_dim[0].width;
        unsigned int tw = MAX(2, (sw * scale) >> 16), th = MAX(2, (sh * scale) >> 16);
        unsigned int pad = i3_osd_plate[handle] ? MAX(2, (8 * scale) >> 16) : 0;

        // ARGB1555 -> NV12 with Y = 0 as the transparent key; even dimensions.
        // The widgets have no alpha, so the plate behind the text (opacity
        // below 255) is drawn as a sparse grid of black pixels: one in four
        // reads as roughly a quarter opacity once scaled and encoded.
        unsigned int w = (tw + 2 * pad + 1) & ~1, h = (th + 2 * pad + 1) & ~1, size = w * h * 3 / 2;
        unsigned char *yuv = calloc(1, size);
        if (!yuv) return EXIT_FAILURE;
        memset(yuv + w * h, 128, w * h / 2);
        if (pad)
            for (unsigned int y = 0; y < h; y += 2)
                for (unsigned int x = 0; x < w; x += 2)
                    yuv[y * w + x] = 16;
        for (unsigned int y = 0; y < th; y++) {
            unsigned int y0 = ((unsigned long long)y << 16) / scale, y1 = ((unsigned long long)(y + 1) << 16) / scale;
            if (y1 > sh) y1 = sh;
            if (y1 <= y0) y1 = y0 + 1;
            for (unsigned int x = 0; x < tw; x++) {
                unsigned int x0 = ((unsigned long long)x << 16) / scale, x1 = ((unsigned long long)(x + 1) << 16) / scale;
                if (x1 > sw) x1 = sw;
                if (x1 <= x0) x1 = x0 + 1;
                // Of the source pixels under this one keep the brightest opaque
                // one, so thin strokes survive scaling down
                int best = -1, br = 0, bg = 0, bb = 0;
                for (unsigned int yy = y0; yy < y1 && yy < sh; yy++)
                    for (unsigned int xx = x0; xx < x1 && xx < sw; xx++) {
                        unsigned short p = src[yy * sw + xx];
                        if (!(p & 0x8000)) continue;
                        int r = (p >> 10 & 31) * 255 / 31, g = (p >> 5 & 31) * 255 / 31, b = (p & 31) * 255 / 31;
                        int luma = (77 * r + 150 * g + 29 * b) >> 8;
                        if (luma > best) { best = luma; br = r; bg = g; bb = b; }
                    }
                if (best < 0) continue;
                unsigned int ox = x + pad, oy = y + pad;
                yuv[oy * w + ox] = best < 16 ? 16 : best;
                unsigned char *uv = yuv + w * h + (oy / 2) * w + (ox & ~1);
                uv[0] = ((-43 * br - 85 * bg + 128 * bb) >> 8) + 128;
                uv[1] = ((128 * br - 107 * bg - 21 * bb) >> 8) + 128;
            }
        }

        // The plate grows around the text instead of pushing it away from its place
        int px0 = (int)((i3_osd_rect[handle].x * scale) >> 16) - (int)pad;
        int py0 = (int)((i3_osd_rect[handle].y * scale) >> 16) - (int)pad;
        int maxx = (int)i3_chn_dim[c].width - (int)w, maxy = (int)i3_chn_dim[c].height - (int)h;
        if (px0 > maxx) px0 = maxx;
        if (py0 > maxy) py0 = maxy;
        i3_osd_pos pos = { (px0 < 0 ? 0 : px0) & ~1, (py0 < 0 ? 0 : py0) & ~1 };
        int cret;
        if (i3_osd_widget[handle][c])
            cret = update(i3_osd_widget[handle][c], yuv, size, &pos, w, h);
        else if ((cret = create(&i3_osd_widget[handle][c], i3_vi_of[c], yuv, size, &pos, w, h)))
            i3_osd_widget[handle][c] = NULL;
        if (!cret && canvas) canvas(i3_vi_of[c]);
        if (cret) ret = cret;
        free(yuv);
    }
    return ret;
}

// Auto-exposure state from the vendor ISP library. MI_ISP_QueryExposureInfo(chn, out)
// fills a 556-byte record: stable, reach-boundary, FNx10, sensor gain, ISP gain,
// shutter (us), LumY, AvgY, 128-bin histogram, LVx10, BV, scene target.
int i3_exposure_info(unsigned int *sensorGain, unsigned int *ispGain,
    unsigned int *shutterUs, unsigned int *avgY, int *bv)
{
    static int (*query)(int, void*);
    static unsigned int info[160];
    if (!query && !(query = dlsym(RTLD_DEFAULT, "MI_ISP_QueryExposureInfo")))
        return EXIT_FAILURE;
    memset(info, 0, sizeof(info));
    if (query(0, info)) return EXIT_FAILURE;
    if (sensorGain) *sensorGain = info[3];
    if (ispGain) *ispGain = info[4];
    if (shutterUs) *shutterUs = info[5];
    if (avgY) *avgY = info[7];
    if (bv) *bv = (int)info[137];
    return EXIT_SUCCESS;
}

int i3_channel_unbind(char index)
{
    i3_sys_bind source = { I3_SYS_MOD_VI, 0, i3_vi_of[index] };
    i3_sys_bind dest = { I3_SYS_MOD_VENC, 0, index };

    if (i3_venc_on[index]) i3.vencStop(index);
    if (i3_bound[index]) { i3.sysUnbind(&source, &dest); i3_bound[index] = 0; }
    if (i3_vi_on[index]) { i3.viDisable(i3_vi_of[index]); i3_vi_on[index] = 0; }

    return EXIT_SUCCESS;
}

int i3_config_load(char *path) { return EXIT_SUCCESS; }

int i3_pipeline_create(char sensor, short width, short height, char mirror, char flip, char framerate)
{
    i3_vi_dev device;
    memset(&device, 0, sizeof(device));
    device.source = I3_VI_SRC_CAMERA;
    device.srcFps = (float)framerate;

    return i3.viSetDev(&device);
}

void i3_pipeline_destroy(void)
{
    for (char i = 0; i < I3_VENC_CHN_NUM; i++)
        if (i3_vi_on[i]) { i3.viDisable(i3_vi_of[i]); i3_vi_on[i] = 0; }
}

int i3_video_create(char index, hal_vidconfig *config)
{
    int ret;
    i3_venc_chn channel;
    memset(&channel, 0, sizeof(channel));

    switch (config->codec) {
        case HAL_VIDCODEC_H264: channel.attrib.codec = 0; break;
        case HAL_VIDCODEC_H265: channel.attrib.codec = 1; break;
        // The single-shot JPEG encoder refuses to bind to a video port, so
        // snapshots are taken from an MJPEG channel as well.
        case HAL_VIDCODEC_MJPG:
        case HAL_VIDCODEC_JPG:  channel.attrib.codec = 2; break;
        default: HAL_ERROR("i3_venc", "This codec is not supported by the hardware!\n");
    }
    if (config->codec == HAL_VIDCODEC_JPG && i3_vi_of[index] != I3_VI_CHN_IMAGE) {
        i3_vi_chn still;
        memset(&still, 0, sizeof(still));
        still.dest.width = config->width;
        still.dest.height = config->height;
        still.pixFmt = I3_PIXFMT_YUV420SP;
        still.mode = I3_MODE_TUNNEL;
        if (ret = i3.viSetChn(I3_VI_CHN_IMAGE, &still))
            return ret;
        i3_vi_of[index] = I3_VI_CHN_IMAGE;
    }
    if (channel.attrib.codec == 2) {
        // JPEG quality (rate control is not applied to this encoder, quality
        // sets the size): the configured qfactor, else generous for stills
        // and moderate for the MJPEG stream. Fixed for the channel's life.
        channel.attrib.pad[0] = config->maxQual ? MIN(config->maxQual, 100) :
            config->codec == HAL_VIDCODEC_JPG ? 85 : 50;
        config->mode = HAL_VIDMODE_CBR;
        if (!config->bitrate) config->bitrate = 4096;
        if (!config->framerate) config->framerate = 5;
        if (!config->gop) config->gop = config->framerate;
    } else
        channel.attrib.h26x.profile = MIN(config->profile, 2);
    channel.attrib.capt.width = config->width;
    channel.attrib.capt.height = config->height;
    channel.attrib.pixFmt = I3_PIXFMT_YUV420SP;
    channel.attrib.dstFps = config->framerate;
    channel.attrib.mode = I3_MODE_TUNNEL;

    if (config->mode == HAL_VIDMODE_CBR) {
        channel.rate.mode = channel.attrib.codec == 2 ? 0 : 2;
        channel.rate.cbr.gop = config->gop;
        channel.rate.cbr.statTime = 1;
        channel.rate.cbr.bitrate = (unsigned int)config->bitrate << 10;
        channel.rate.cbr.avgLvl = 1;
        channel.rate.cbr.maxQual = config->maxQual ? config->maxQual : 45;
        channel.rate.cbr.minQual = config->minQual ? config->minQual : 20;
    } else {
        channel.rate.mode = 1;
        channel.rate.vbr.gop = config->gop;
        channel.rate.vbr.statTime = 1;
        channel.rate.vbr.maxBitrate = (unsigned int)MAX(config->bitrate, config->maxBitrate) << 10;
        channel.rate.vbr.maxIQual = config->maxQual ? config->maxQual : 45;
        channel.rate.vbr.minIQual = config->minQual ? config->minQual : 20;
        channel.rate.vbr.maxPQual = config->maxQual ? config->maxQual : 45;
        channel.rate.vbr.minPQual = config->minQual ? config->minQual : 20;
        channel.rate.vbr.chgPos = 80;
    }

    if (ret = i3.vencCreate(index, &channel))
        return ret;

    i3_venc_on[index] = 1;
    i3_state[index].payload = config->codec;

    return EXIT_SUCCESS;
}

int i3_video_destroy(char index)
{
    i3_state[index].enable = 0;
    i3_state[index].payload = HAL_VIDCODEC_UNSPEC;

    i3_channel_unbind(index);
    if (i3_venc_on[index]) { i3.vencDestroy(index); i3_venc_on[index] = 0; }
    i3_vi_of[index] = index;

    return EXIT_SUCCESS;
}

int i3_video_destroy_all(void)
{
    for (char i = 0; i < I3_VENC_CHN_NUM; i++)
        if (i3_state[i].enable || i3_venc_on[i])
            i3_video_destroy(i);

    return EXIT_SUCCESS;
}

void i3_video_request_idr(char index)
{
    if (i3_venc_on[index]) i3.vencIdr(index, 1);
}

int i3_video_snapshot_grab(char index, char quality, hal_jpegdata *jpeg)
{
    int ret;
    unsigned int stream[I3_STRM_WORDS];

    if (ret = i3_channel_bind(index, 1)) {
        HAL_DANGER("i3_venc", "Binding the encoder channel "
            "%d failed with %#x!\n", index, ret);
        goto abort;
    }

    ret = EXIT_FAILURE;
    for (int attempt = 0; attempt < 15; attempt++) {
        memset(stream, 0, sizeof(stream));
        if (i3.vencGet(index, stream, 200)) continue;

        unsigned char *data = (unsigned char*)(unsigned long)stream[0];
        unsigned int length = stream[1];
        // Skip anything that is not a complete JPEG (starts FF D8).
        if (data && length > 4 && data[0] == 0xFF && data[1] == 0xD8) {
            if (length > jpeg->length) {
                jpeg->data = realloc(jpeg->data, length);
                jpeg->length = length;
            }
            memcpy(jpeg->data, data, length);
            jpeg->jpegSize = length;
            ret = EXIT_SUCCESS;
        }
        i3.vencRelease(index, stream);
        if (!ret) break;
    }
    if (ret) HAL_DANGER("i3_venc", "Capture stream timed out!\n");

abort:
    i3_channel_unbind(index);
    return ret;
}

void *i3_video_thread(void)
{
    int ret;
    unsigned int stream[I3_STRM_WORDS];

    while (keepRunning) {
        char active = 0;
        for (char i = 0; i < I3_VENC_CHN_NUM; i++) {
            if (!i3_state[i].enable || !i3_state[i].mainLoop) continue;
            if (!i3_venc_on[i] || !i3_bound[i]) continue;
            active = 1;

            memset(stream, 0, sizeof(stream));
            // A channel that is being restarted fails at once instead of
            // waiting out the timeout; don't spin on it.
            if (i3.vencGet(i, stream, 200)) { usleep(10000); continue; }

            unsigned char *data = (unsigned char*)(unsigned long)stream[0];
            unsigned int length = stream[1];
            if (data && length && i3_vid_cb) {
                hal_vidstream outStrm;
                hal_vidpack outPack[1];
                memset(outPack, 0, sizeof(outPack));
                outPack[0].data = data;
                outPack[0].length = length;
                outPack[0].offset = 0;
                outPack[0].timestamp =
                    ((unsigned long long)stream[3] << 32) | stream[2];

                unsigned int n = 0, sc, scanOff = 0;
                char h26x = i3_state[i].payload == HAL_VIDCODEC_H264 ||
                    i3_state[i].payload == HAL_VIDCODEC_H265;
                while (h26x && n < 7 &&
                       (sc = nal_find_startcode(data, scanOff, length)) < length) {
                    unsigned int scLen = (data[sc + 2] == 1) ? 3 : 4;
                    outPack[0].nalu[n].type = i3_state[i].payload == HAL_VIDCODEC_H264
                        ? (data[sc + scLen] & 0x1F)
                        : ((data[sc + scLen] & 0x7E) >> 1);
                    outPack[0].nalu[n].offset = sc;
                    n++;
                    scanOff = sc + scLen;
                }
                outPack[0].naluCnt = n;
                outPack[0].nalu[n].offset = length;
                for (n = 0; n < outPack[0].naluCnt; n++)
                    outPack[0].nalu[n].length =
                        outPack[0].nalu[n + 1].offset - outPack[0].nalu[n].offset;

                outStrm.count = 1;
                outStrm.seq = 0;
                outStrm.pack = outPack;
                (*i3_vid_cb)(i, &outStrm);
            }

            if (ret = i3.vencRelease(i, stream))
                HAL_WARNING("i3_venc", "Releasing the stream on "
                    "channel %d failed with %#x!\n", i, ret);
        }
        if (!active) usleep(20000);
    }
    HAL_INFO("i3_venc", "Shutting down encoding thread...\n");
    return NULL;
}

void i3_system_deinit(void)
{
    i3.sysExit();
}

// The vendor libraries take their log level from an Android-style property
// service that OpenIPC does not run, and default to tracing every buffer.
// Both export the level as plain variables, so quieten them directly.
static void i3_quiet_logs(void)
{
    unsigned int *level;
    if ((level = dlsym(i3.handle, "g_uDbgLogLevel"))) *level = 0;

    void *omx = dlopen("/lib/libOMX_BELA.so", RTLD_LAZY | RTLD_NOLOAD);
    if (!omx) omx = dlopen("libOMX_BELA.so", RTLD_LAZY | RTLD_NOLOAD);
    if (omx) {
        if ((level = dlsym(omx, "OmxDbgLogLevel"))) *level = 0;
        if ((level = dlsym(omx, "ModuleDbgLogLevel"))) *level = 0;
        dlclose(omx);
    }
}

int i3_system_init(void)
{
    int ret = i3.sysInit();
    i3_quiet_logs();
    return ret;
}

#define I3_SYS_REG_ID        0x1F2025A4
#define I3_SYS_CHIP_MASK     0xF000
#define I3_SYS_CHIP_MSC313E  0x6000
#define I3_SYS_CHIP_MSC316DC 0x7000
#define I3_SYS_CHIP_MSC318   0x8000

int i3_system_identify(void)
{
    unsigned int val = 0;

    if (access("/proc/mstar/kernel", 0) ||
        !hal_registry(I3_SYS_REG_ID, &val, OP_READ))
        return 0;

    plat = HAL_PLATFORM_I3;
    switch (val & I3_SYS_CHIP_MASK) {
        case I3_SYS_CHIP_MSC313E: strcpy(chip, "MSC313E"); break;
        case I3_SYS_CHIP_MSC316DC: strcpy(chip, "MSC316DC"); break;
        case I3_SYS_CHIP_MSC318: strcpy(chip, "MSC318"); break;
    }
    strcpy(family, "infinity3");
    chnCount = I3_VENC_CHN_NUM;
    chnState = (hal_chnstate*)i3_state;
    aud_thread = i3_audio_thread;
    vid_thread = i3_video_thread;
    return 1;
}

#endif
