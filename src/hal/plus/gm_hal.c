#ifdef __arm__

#include "gm_hal.h"

gm_lib_impl gm_lib;

hal_chnstate gm_state[GM_VENC_CHN_NUM] = {0};
int (*gm_aud_cb)(hal_audframe*);
int (*gm_venc_cb)(char, hal_vidstream*);

int _gm_cap_dev;
int _gm_cap_grp;
int _gm_venc_bind[GM_VENC_CHN_NUM];
int _gm_venc_dev[GM_VENC_CHN_NUM];

void gm_hal_deinit(void)
{
    gm_lib_unload(&gm_lib);
}

int gm_hal_init(void)
{
    int ret;

    if (ret = gm_lib_load(&gm_lib))
        return ret;

    return EXIT_SUCCESS;
}

int gm_channel_bind(char index)
{
    int ret;

    if (ret = gm_lib.fnBind(_gm_cap_grp, _gm_cap_dev, _gm_venc_dev[index]))
        return ret;
    _gm_venc_bind[index] = ret;

    if (ret = gm_lib.fnRefreshGroup(_gm_cap_grp))
        return ret;

    return EXIT_SUCCESS;
}

int gm_channel_unbind(char index)
{
    int ret;

    if (ret = gm_lib.fnUnbind(_gm_venc_bind[index]))
        return ret;

    if (ret = gm_lib.fnRefreshGroup(_gm_cap_grp))
        return ret;

    return EXIT_SUCCESS;
}

int gm_pipeline_create(char mirror, char flip)
{
    int ret;

    if (ret = gm_lib.fnCreateGroup())
        return ret;
    _gm_cap_grp = ret;

    if (ret = gm_lib.fnCreateDevice(GM_LIB_DEV_CAPTURE))
        return ret;
    _gm_cap_dev = ret;

    {
        GM_DECLARE(gm_lib, config, gm_cap_cnf);
        config.channel = 0;
        config.output = GM_CAP_OUT_SCALER2;
        config.motionDataOn = 0;

        if (ret = gm_lib.fnSetDeviceConfig(_gm_cap_dev, &config))
            return ret;
    }
}

void gm_pipeline_destroy(void)
{
    gm_lib.fnDestroyDevice(_gm_cap_dev);

    gm_lib.fnDestroyGroup(_gm_cap_grp);
}



int gm_video_create(char index, hal_vidconfig *config)
{
    int ret;

    if (ret = gm_lib.fnCreateDevice(GM_LIB_DEV_VIDENC))
        return ret;
    _gm_venc_dev[index] = ret;

    gm_venc_ratemode ratemode;

    switch (config->mode) {
        case HAL_VIDMODE_CBR: ratemode = GM_VENC_RATEMODE_CBR; break;
        case HAL_VIDMODE_VBR: ratemode = GM_VENC_RATEMODE_VBR; break;
        default: GM_ERROR("Video encoder does not support this mode!");
    }

    switch (config->codec) {
        case HAL_VIDCODEC_JPG:
        case HAL_VIDCODEC_MJPG:
            GM_DECLARE(gm_lib, mjpgchn, gm_venc_mjpg_cnf);
            mjpgchn.dest.width = config->width;
            mjpgchn.dest.height = config->height;
            mjpgchn.fpsNum = config->framerate;
            mjpgchn.fpsDen = 1;
            mjpgchn.quality = MAX(config->minQual, config->maxQual);
            mjpgchn.mode = ratemode;
            mjpgchn.bitrate = config->bitrate;
            mjpgchn.maxBitrate = MAX(config->bitrate, config->maxBitrate);
            if (ret = gm_lib.fnSetDeviceConfig(_gm_venc_dev[index], &mjpgchn))
                return ret;
            break;
        case HAL_VIDCODEC_H264:
            GM_DECLARE(gm_lib, h264chn, gm_venc_h264_cnf);
            h264chn.dest.width = config->width;
            h264chn.dest.height = config->height;
            h264chn.fpsNum = config->framerate;
            h264chn.fpsDen = 1;
            h264chn.rate.mode = ratemode;
            h264chn.rate.gop = config->gop;
            h264chn.rate.minQual = config->minQual;
            h264chn.rate.maxQual = config->maxQual;
            h264chn.rate.bitrate = config->bitrate;
            h264chn.rate.maxBitrate = MAX(config->bitrate, config->maxBitrate);
            h264chn.motionDataOn = 0;
            switch (config->profile) {
                case HAL_VIDPROFILE_BASELINE:
                    h264chn.profile = GM_VENC_H264PROF_BASELINE;
                    h264chn.bFrameNum = 0;
                    break;
                case HAL_VIDPROFILE_MAIN:
                    h264chn.profile = GM_VENC_H264PROF_MAIN;
                    h264chn.bFrameNum = 0;
                    break;
                case HAL_VIDPROFILE_HIGH:
                    h264chn.profile = GM_VENC_H264PROF_HIGH;
                    h264chn.bFrameNum = 2;
                    break;    
            }
            h264chn.level = 41;
            if (ret = gm_lib.fnSetDeviceConfig(_gm_venc_dev[index], &mjpgchn))
                return ret;
            break;
        default: GM_ERROR("This codec is not supported by the hardware!");
    }

    gm_state[index].payload = config->codec;

    return EXIT_SUCCESS;
}

int gm_video_destroy(char index)
{
    int ret;

    gm_state[index].payload = HAL_VIDCODEC_UNSPEC;

    if (ret = gm_lib.fnDestroyDevice(index))
        return ret;

    return EXIT_SUCCESS;
}

int gm_video_destroy_all(void)
{
    int ret;

    for (char i = 0; i < GM_VENC_CHN_NUM; i++)
        if (gm_state[i].enable)
            if (ret = gm_video_destroy(i))
                return ret;

    return EXIT_SUCCESS;
}

int gm_video_snapshot_grab(short width, short height, char quality, hal_jpegdata *jpeg)
{
    int ret;
    unsigned int length = 2 * 1024 * 1024;
    char *buffer = malloc(length);

    GM_DECLARE(gm_lib, snap, gm_venc_snap);
    snap.bind = (void*)_gm_venc_bind[0];
    snap.quality = quality;
    snap.buffer = buffer;
    snap.length = length;
    snap.dest.width = MIN(width, GM_VENC_SNAP_WIDTH_MAX);
    snap.dest.height = MIN(height, GM_VENC_SNAP_HEIGHT_MAX);

    if ((ret = gm_lib.fnSnapshot(&snap, 500)) <= 0)
        goto abort;

    jpeg->data = buffer;
    jpeg->jpegSize = jpeg->length = length;
    return EXIT_SUCCESS;

abort:
    free(buffer);
    GM_ERROR("Taking a snapshot failed with %#x!\n", ret);
}

void *gm_video_thread(void)
{
    
}

void gm_system_deinit(void)
{
    gm_lib.fnExit();
}

int gm_system_init(void)
{
    int ret;

    puts("App built with headers v" GM_LIB_API);
    printf("GrainMedia - library 0x%#x\n", GM_LIB_VER);

    if (ret = gm_lib.fnInit(GM_LIB_VER))
        return ret;

    return EXIT_SUCCESS;
}

#endif