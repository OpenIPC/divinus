#if defined(__arm__) && !defined(__ARM_PCS_VFP)

#include "ak_hal.h"
#include "ak_osd.h"

ak_aud_impl  ak_aud;
ak_sys_impl  ak_sys;

hal_chnstate ak_state[AK_VENC_CHN_NUM] = {0};
int (*ak_aud_cb)(hal_audframe*);
int (*ak_vid_cb)(char, hal_vidstream*);

static unsigned char *ak_mjpeg_buf;
static size_t ak_mjpeg_len;
static pthread_mutex_t ak_mjpeg_mtx = PTHREAD_MUTEX_INITIALIZER;

static struct {
    int width, height, maxWidth, maxHeight;
} _ak_vi_res;

void *_ak_aud_dev;
void *_ak_vi_dev;
void *_ak_venc_dev[AK_VENC_CHN_NUM];
void *_ak_venc_strm[AK_VENC_CHN_NUM];

void ak_hal_deinit(void)
{
    ak_aud_unload(&ak_aud);
    ak_sys_unload(&ak_sys);
}

int ak_hal_init(void)
{
    int ret;

    if (ret = ak_sys_load(&ak_sys))
        return ret;
    if (ret = ak_aud_load(&ak_aud))
        return ret;

    return EXIT_SUCCESS;
}

void ak_audio_deinit(void)
{
    ak_aud.fnDisableDevice(_ak_aud_dev);

    _ak_aud_dev = NULL;
}

int ak_audio_init(int samplerate)
{
    int ret;
    ak_aud_cnf config = {
        .rate = samplerate,
        .bit = 16,
        .chnNum = 1
    };

    if (!(_ak_aud_dev = ak_aud.fnEnableDevice(&config)))
        HAL_ERROR("ak_aud", "Opening the audio input failed with %#x!\n%s\n",
            ret = ak_sys.fnGetErrorNum(), ak_sys.fnGetErrorStr(ret));

    return EXIT_SUCCESS;
}

void *ak_audio_thread(void)
{
    int ret;
    unsigned long sequence = 0;

    while (keepRunning && audioOn) {
        ak_aud_frm frame;
        memset(&frame, 0, sizeof(frame));

        if (ret = ak_aud.fnGetFrame(_ak_aud_dev, &frame, 1000)) {
            HAL_WARNING("ak_aud", "Getting the audio frame failed with %#x!\n",
                ak_sys.fnGetErrorNum());
            continue;
        }
        if (!frame.length) {
            ak_aud.fnFreeFrame(_ak_aud_dev, &frame);
            continue;
        }

        if (ak_aud_cb) {
            hal_audframe outFrame;
            outFrame.channelCnt = 1;
            outFrame.data[0] = frame.data;
            outFrame.length[0] = frame.length;
            outFrame.seq = sequence++;
            outFrame.timestamp = frame.timestamp;
            (*ak_aud_cb)(&outFrame);
        }

        if (ak_aud.fnFreeFrame(_ak_aud_dev, &frame))
            HAL_DANGER("ak_aud", "Releasing the audio frame failed!\n");
    }
    HAL_INFO("ak_aud", "Shutting down audio capture thread...\n");
}

int ak_channel_bind(char index)
{
    if (ak_v4l2_enc_streamon(index) < 0) {
        HAL_ERROR("ak_venc", "Starting the encoder failed!\n");
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}

int ak_channel_grayscale(char enable)
{
    return ak_v4l2_enc_grayscale(0, enable);
}

int ak_channel_unbind(char index)
{
    ak_v4l2_enc_streamoff(index);
    return EXIT_SUCCESS;
}

int ak_pipeline_create(short width, short height, char mirror, char flip)
{
    (void)mirror;
    (void)flip;

    if (width <= 0 || height <= 0) {
        width = 1280;
        height = 720;
    }

    if (ak_v4l2_cap_open(0, width, height) < 0)
        HAL_ERROR("ak_vi", "Opening the main capture failed!\n");

    if (ak_v4l2_cap_open(1, app_config.mjpeg_width,
            app_config.mjpeg_height) < 0)
        HAL_WARNING("ak_vi", "Opening the sub capture failed; main only\n");

    _ak_vi_res.width = width;
    _ak_vi_res.height = height;
    _ak_vi_res.maxWidth = width;
    _ak_vi_res.maxHeight = height;

    return EXIT_SUCCESS;
}

void ak_pipeline_streamon(void)
{
    if (ak_v4l2_cap_streamon(0) < 0)
        HAL_DANGER("ak_vi", "Starting the main capture failed!\n");
}

void ak_pipeline_streamon_sub(void)
{
    if (ak_v4l2_cap_streamon(1) < 0)
        HAL_WARNING("ak_vi", "Starting the sub capture failed; main only\n");
}

void ak_pipeline_destroy(void)
{
    ak_v4l2_cap_close(0);
    ak_v4l2_cap_close(1);
}

int ak_video_create(char index, hal_vidconfig *config)
{
    if (ak_v4l2_enc_open(index, config->width, config->height,
            config->codec, config->framerate, config->bitrate,
            config->gop) < 0) {
        HAL_ERROR("ak_venc", "Creating channel %d failed!\n", index);
        return EXIT_FAILURE;
    }

    ak_state[index].payload = config->codec;

    return EXIT_SUCCESS;
}

int ak_video_destroy(char index)
{
    ak_v4l2_enc_close(index);
    return EXIT_SUCCESS;
}

int ak_video_destroy_all(void)
{
    int ret;

    for (char i = 0; i < AK_VENC_CHN_NUM; i++)
        if (ak_state[i].enable)
            if (ret = ak_video_destroy(i))
                return ret;

    return EXIT_SUCCESS;
}

void ak_video_request_idr(char index)
{
    ak_v4l2_enc_request_idr(index);
}

int ak_video_snapshot_grab(char index, hal_jpegdata *jpeg)
{
    void *data = NULL;
    size_t len = 0;
    (void)index;

    jpeg->data = NULL;
    jpeg->length = jpeg->jpegSize = 0;

    pthread_mutex_lock(&ak_mjpeg_mtx);
    if (ak_mjpeg_len) {
        data = malloc(ak_mjpeg_len);
        if (data) {
            memcpy(data, ak_mjpeg_buf, ak_mjpeg_len);
            len = ak_mjpeg_len;
        }
    }
    pthread_mutex_unlock(&ak_mjpeg_mtx);

    if (!data)
        return EXIT_FAILURE;

    jpeg->data = data;
    jpeg->length = jpeg->jpegSize = len;

    return EXIT_SUCCESS;
}

static unsigned int ak_nal_start(const unsigned char *d, unsigned int off, unsigned int len)
{
    for (unsigned int i = off; i + 2 < len; i++)
        if (!d[i] && !d[i + 1] &&
            (d[i + 2] == 1 || (i + 3 < len && d[i + 2] == 0 && d[i + 3] == 1)))
            return i;
    return len;
}

int ak_region_create(char handle, hal_rect rect, short opacity)
{
    if (handle < 0 || handle >= AK_OSD_MAX)
        return EXIT_FAILURE;
    pthread_mutex_lock(&ak_region_mtx);
    ak_region[handle].rect = rect;
    ak_region[handle].opacity = opacity;
    pthread_mutex_unlock(&ak_region_mtx);
    return EXIT_SUCCESS;
}

int ak_region_setbitmap(char handle, hal_bitmap *bitmap)
{
    if (handle < 0 || handle >= AK_OSD_MAX || !bitmap || !bitmap->data)
        return EXIT_FAILURE;
    if (!bitmap->dim.width || !bitmap->dim.height)
        return EXIT_FAILURE;
    const int bw = bitmap->dim.width, bh = bitmap->dim.height;
    const int stride = (bw + 1) & ~1;
    unsigned char *bits = calloc((size_t)stride * bh, 2);
    if (!bits)
        return EXIT_FAILURE;
    for (int y = 0; y < bh; y++)
        memcpy(bits + (size_t)y * stride * 2,
            (unsigned char *)bitmap->data + (size_t)y * bw * 2, bw * 2);
    pthread_mutex_lock(&ak_region_mtx);
    free(ak_region[handle].bits);
    ak_region[handle].bits = bits;
    ak_region[handle].dim = bitmap->dim;
    pthread_mutex_unlock(&ak_region_mtx);
    return EXIT_SUCCESS;
}

void ak_region_destroy(char handle)
{
    if (handle < 0 || handle >= AK_OSD_MAX)
        return;
    pthread_mutex_lock(&ak_region_mtx);
    free(ak_region[handle].bits);
    ak_region[handle].bits = NULL;
    ak_region[handle].dim.width = ak_region[handle].dim.height = 0;
    pthread_mutex_unlock(&ak_region_mtx);
}

static void ak_video_emit(char index, void *out, size_t outlen, uint64_t tstamp)
{
    if (!ak_vid_cb || !outlen) return;

    if (ak_state[index].payload == HAL_VIDCODEC_MJPG) {
        pthread_mutex_lock(&ak_mjpeg_mtx);
        unsigned char *nb = realloc(ak_mjpeg_buf, outlen);
        if (nb) {
            ak_mjpeg_buf = nb;
            memcpy(ak_mjpeg_buf, out, outlen);
            ak_mjpeg_len = outlen;
        }
        pthread_mutex_unlock(&ak_mjpeg_mtx);
    }

    hal_vidstream outStrm;
    hal_vidpack outPack[1];
    memset(outPack, 0, sizeof(outPack));
    outStrm.count = 1;
    outStrm.seq = 0;
    outPack[0].data = out;
    outPack[0].length = outlen;

    unsigned int n = 0, sc, scan = 0;
    while (n < 7 && (sc = ak_nal_start(out, scan, outlen)) < outlen) {
        unsigned int scLen = (((unsigned char *)out)[sc + 2] == 1) ? 3 : 4;
        outPack[0].nalu[n].offset = sc;
        outPack[0].nalu[n].type = ((unsigned char *)out)[sc + scLen] & 0x1F;
        n++;
        scan = sc + scLen;
    }
    if (n) {
        outPack[0].naluCnt = n;
        outPack[0].nalu[n].offset = outlen;
        for (unsigned int k = 0; k < n; k++)
            outPack[0].nalu[k].length =
                outPack[0].nalu[k + 1].offset - outPack[0].nalu[k].offset;
    } else {
        outPack[0].naluCnt = 1;
        outPack[0].nalu[0].length = outlen;
        outPack[0].nalu[0].offset = 0;
        outPack[0].nalu[0].type = 1;
    }
    outPack[0].offset = 0;
    outPack[0].timestamp = tstamp;
    outStrm.pack = outPack;
    (*ak_vid_cb)(index, &outStrm);
}

void *ak_video_thread(void)
{
    while (keepRunning) {
        void *data = NULL, *out = NULL;
        size_t len = 0, outlen = 0;
        uint64_t tstamp = 0;
        int frameIdx = -1;

        unsigned int gen = ak_v4l2_cap_gen(0);

        if (ak_v4l2_cap_dequeue(0, &data, &len, &tstamp, &frameIdx, 1000) < 0)
            continue;

        /* The pipeline may have been torn down while the dequeue was
           waiting; the frame pointer would then already be unmapped. */
        if (gen != ak_v4l2_cap_gen(0))
            continue;

        pthread_mutex_lock(&ak_v4l2_mtx);
        if (gen == ak_v4l2_cap_gen(0)) {
            ak_region_apply((unsigned char *)data, _ak_vi_res.width, _ak_vi_res.height);

            for (char i = 0; i < AK_VENC_CHN_NUM; i++) {
                if (!ak_state[i].enable || !ak_state[i].mainLoop) continue;
                if (ak_state[i].payload == HAL_VIDCODEC_MJPG ||
                    ak_state[i].payload == HAL_VIDCODEC_JPG) continue;
                if (ak_v4l2_enc_push(i, data, len, &out, &outlen, 1000) < 0) continue;
                ak_video_emit(i, out, outlen, tstamp);
                ak_v4l2_enc_requeue(i);
            }
            ak_v4l2_cap_requeue(0, frameIdx);
        }
        pthread_mutex_unlock(&ak_v4l2_mtx);

        gen = ak_v4l2_cap_gen(1);

        if (ak_v4l2_cap_dequeue(1, &data, &len, &tstamp, &frameIdx, 1000) < 0)
            continue;

        if (gen != ak_v4l2_cap_gen(1))
            continue;

        pthread_mutex_lock(&ak_v4l2_mtx);
        if (gen == ak_v4l2_cap_gen(1)) {
            for (char i = 0; i < AK_VENC_CHN_NUM; i++) {
                if (!ak_state[i].enable || !ak_state[i].mainLoop) continue;
                if (ak_state[i].payload != HAL_VIDCODEC_MJPG &&
                    ak_state[i].payload != HAL_VIDCODEC_JPG) continue;
                if (ak_v4l2_enc_push(i, data, len, &out, &outlen, 1000) < 0) continue;
                ak_video_emit(i, out, outlen, tstamp);
                ak_v4l2_enc_requeue(i);
            }
            ak_v4l2_cap_requeue(1, frameIdx);
        }
        pthread_mutex_unlock(&ak_v4l2_mtx);
    }

    HAL_INFO("ak_venc", "Shutting down encoding thread...\n");
    return NULL;
}

void ak_system_deinit(void)
{

}

int ak_system_init(char *snrConfig)
{
    int val = AK_SYS_BOOT_MAGIC;
    (void)snrConfig;

    // Disable the watchdog timer if it isn't already
    hal_registry(AK_SYS_BOOT_REG, &val, OP_WRITE);

    return EXIT_SUCCESS;
}

static int ak_identify_board(void)
{
    char compat[128] = {0};
    FILE *file = fopen("/proc/device-tree/compatible", "r");

    if (!file) return 0;

    size_t len = fread(compat, 1, sizeof(compat) - 1, file);
    fclose(file);

    for (size_t i = 0; i < len; i++)
        if (!compat[i]) compat[i] = ' ';

    return strstr(compat, "anyka,ak3918") != NULL;
}

int ak_system_identify(void)
{
    unsigned int val = 0;

    if (!ak_identify_board() ||
        !hal_registry(AK_SYS_REG_CHIP, &val, OP_READ))
        return 0;

    plat = HAL_PLATFORM_AK;
    strcpy(chip, "AK3918E");
    switch (val) {
        case AK_SYS_CHIP_3918EV100: strcat(chip, "V100"); break;
        case AK_SYS_CHIP_3918EV200: strcat(chip, "V200"); break;
        case AK_SYS_CHIP_3918EV300: strcat(chip, "V300"); break;
        case AK_SYS_CHIP_3918EV330: strcat(chip, "V330"); break;
    }
    strcpy(family, "anyka");
    chnCount = AK_VENC_CHN_NUM;
    chnState = (hal_chnstate*)ak_state;
    aud_thread = ak_audio_thread;
    vid_thread = ak_video_thread;

    // Needed for libplat_thread and uClibc
    dlopen("libpthread.so.0", RTLD_LAZY | RTLD_GLOBAL);
    return 1;
}

#endif
