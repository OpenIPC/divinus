#ifdef __arm__

#include "gm_hal.h"

gm_lib_impl gm_lib;

hal_chnstate gm_state[GM_VENC_CHN_NUM] = {0};
gm_common_pollfd _gm_aud_fds[GM_AUD_CHN_NUM], _gm_venc_fds[GM_VENC_CHN_NUM];
int (*gm_aud_cb)(hal_audframe*);
int (*gm_vid_cb)(char, hal_vidstream*);

void* _gm_aenc_dev;
void* _gm_ain_dev;
void* _gm_aud_grp;
void* _gm_cap_dev;
void* _gm_cap_grp;
void* _gm_venc_dev[GM_VENC_CHN_NUM];
int   _gm_venc_sz[GM_VENC_CHN_NUM] = {0};

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

void gm_audio_deinit(void)
{
    gm_lib.fnUnbind(_gm_aud_fds[0].bind);
    _gm_aud_fds[0].bind = NULL;
    _gm_aud_fds[0].evType = 0;

    gm_lib.fnRefreshGroup(_gm_aud_grp);

    gm_lib.fnDestroyDevice(_gm_aenc_dev);

    gm_lib.fnDestroyDevice(_gm_ain_dev);
}

int gm_audio_init(int samplerate)
{
    int ret;

    _gm_aud_grp = gm_lib.fnCreateGroup();

    _gm_ain_dev = gm_lib.fnCreateDevice(GM_LIB_DEV_AUDIN);
    {
        GM_DECLARE(gm_lib, config, gm_ain_cnf, "gm_audio_grab_attr_t");
        config.channel = 0;
        config.rate = samplerate;
        config.frmNum = 16;
        config.chnNum = 1;
        gm_lib.fnSetDeviceConfig(_gm_ain_dev, &config);
    }

    _gm_aenc_dev = gm_lib.fnCreateDevice(GM_LIB_DEV_AUDENC);
    {
        GM_DECLARE(gm_lib, config, gm_aenc_cnf, "gm_audio_enc_attr_t");
        config.codec = GM_AENC_TYPE_PCM;
        config.bitrate = 32000;
        config.packNumPerFrm = 2048;
        gm_lib.fnSetDeviceConfig(_gm_aenc_dev, &config);
    }

    _gm_aud_fds[0].bind = gm_lib.fnBind(_gm_aud_grp, _gm_ain_dev, _gm_aenc_dev);
    _gm_aud_fds[0].evType = GM_POLL_READ;

    if ((ret = gm_lib.fnRefreshGroup(_gm_aud_grp)) < 0)
        return ret;

    return EXIT_SUCCESS;
}

void *gm_audio_thread(void)
{
    int ret;
    gm_common_strm stream[GM_AUD_CHN_NUM];
    memset(stream, 0, sizeof(stream));

    int bufSize = 0;
    for (char i = 0; i < GM_AUD_CHN_NUM; i++)
        bufSize += 12800;
    unsigned long long sequence = 0;

    char *bsData = malloc(bufSize);
    if (!bsData) goto abort;

    while (keepRunning) {
        ret = gm_lib.fnPollStream(_gm_aud_fds, GM_AUD_CHN_NUM, 500);
        if (ret == GM_ERR_TIMEOUT) {
            HAL_WARNING("gm_aud", "Main stream loop timed out!\n");
            continue;
        }

        for (char i = 0; i < GM_AUD_CHN_NUM; i++) {
            if (_gm_aud_fds[i].event.type != GM_POLL_READ)
                continue;
            if (_gm_aud_fds[i].event.bsLength > bufSize) {
                HAL_WARNING("gm_aud", "Bitstream buffer needs %d bytes "
                    "more, dropping the upcoming data!\n",
                    _gm_aud_fds[i].event.bsLength - bufSize);
                continue;
            }

            stream[i].bind = _gm_aud_fds[i].bind;
            stream[i].pack.bsData = bsData;
            stream[i].pack.bsLength = bufSize;
            stream[i].pack.mdData = 0;
            stream[i].pack.mdLength = 0;
        }

        if ((ret = gm_lib.fnReceiveStream(stream, GM_AUD_CHN_NUM)) < 0)
            HAL_WARNING("gm_aud", "Receiving the streams failed "
                "with %#x!\n", ret);
        else for (char i = 0; i < GM_AUD_CHN_NUM; i++) {
            if (!stream[i].bind) continue;
            if (stream[i].ret < 0)
                HAL_WARNING("gm_aud", "Failed to the receive bitstream on "
                    "channel %d with %#x!\n", i, stream[i].ret);
            else if (!stream[i].ret && gm_vid_cb) {
                gm_common_pack *pack = &stream[i].pack;
                if (gm_aud_cb) {
                    hal_audframe outFrame;
                    outFrame.channelCnt = 1;
                    outFrame.data[0] = pack->bsData;
                    outFrame.length[0] = pack->bsSize;
                    outFrame.seq = sequence++;
                    outFrame.timestamp = pack->timestamp;
                    (gm_aud_cb)(&outFrame);
                }
            }
        }        
    }
abort:
    HAL_INFO("gm_venc", "Shutting down encoding thread...\n");
    free(bsData);
}

int gm_channel_bind(char index)
{
    int ret;

    _gm_venc_fds[index].bind = 
        gm_lib.fnBind(_gm_cap_grp, _gm_cap_dev, _gm_venc_dev[index]);
    _gm_venc_fds[index].evType = GM_POLL_READ;

    if ((ret = gm_lib.fnRefreshGroup(_gm_cap_grp)) < 0)
        return ret;

    return EXIT_SUCCESS;
}

int gm_channel_unbind(char index)
{
    int ret;

    gm_lib.fnUnbind(_gm_venc_fds[index].bind);
    _gm_venc_fds[index].bind = NULL;
    _gm_venc_fds[index].evType = 0;

    if ((ret = gm_lib.fnRefreshGroup(_gm_cap_grp)) < 0)
        return ret;

    return EXIT_SUCCESS;
}

int gm_pipeline_create(char mirror, char flip)
{
    _gm_cap_grp = gm_lib.fnCreateGroup();

    _gm_cap_dev = gm_lib.fnCreateDevice(GM_LIB_DEV_CAPTURE);
    {
        GM_DECLARE(gm_lib, config, gm_cap_cnf, "gm_cap_attr_t");
        config.channel = 0;
        config.output = GM_CAP_OUT_SCALER1;

        gm_lib.fnSetDeviceConfig(_gm_cap_dev, &config);
    }

    return EXIT_SUCCESS;
}

void gm_pipeline_destroy(void)
{
    gm_lib.fnDestroyDevice(_gm_cap_dev);

    gm_lib.fnDestroyGroup(_gm_cap_grp);
}

int gm_region_create(char handle, hal_rect rect, short opacity)
{
    if (opacity == 0) opacity = GM_OSD_OPAL_0;
    else if (opacity < 32) opacity = GM_OSD_OPAL_12_5;
    else if (opacity < 64) opacity = GM_OSD_OPAL_25;
    else if (opacity < 96) opacity = GM_OSD_OPAL_37_5;
    else if (opacity < 128) opacity = GM_OSD_OPAL_50;
    else if (opacity < 160) opacity = GM_OSD_OPAL_62_5;
    else if (opacity < 192) opacity = GM_OSD_OPAL_75;
    else opacity = GM_OSD_OPAL_100;

    gm_osd_cnf config = {
        .channel = handle + 4,
        .enabled = 1,
        .x = rect.x,
        .y = rect.y,
        .opacity = opacity,
        .zoom = GM_OSD_ZOOM_1X,
        .align = GM_ALIGN_TOP_LEFT,
        .osgChan = handle + 4
    };
    gm_lib.fnSetRegionConfig(_gm_cap_dev, &config);

    return EXIT_SUCCESS;
}

void gm_region_destroy(char handle)
{
    gm_osd_cnf config = { .channel = handle, .enabled = 0 };
    gm_lib.fnSetRegionConfig(_gm_cap_dev, &config);
}

int gm_region_setbitmap(char handle, hal_bitmap *bitmap)
{   
    gm_osd_imgs bitmaps = {
        .image = {
            {
                .exists = 1,
                .buffer = bitmap->data,
                .length = bitmap->dim.width * bitmap->dim.height * 2,
                .width = bitmap->dim.width,
                .height = bitmap->dim.height,
                .osgChan = handle + 4
            }, {0}, {0}, {0}
        }, .reserved = {0}
    };
    gm_lib.fnSetRegionBitmaps(&bitmaps);

    return EXIT_SUCCESS;
}

int gm_video_create(char index, hal_vidconfig *config)
{
    _gm_venc_dev[index] = gm_lib.fnCreateDevice(GM_LIB_DEV_VIDENC);

    gm_venc_ratemode ratemode;

    switch (config->mode) {
        case HAL_VIDMODE_CBR: ratemode = GM_VENC_RATEMODE_CBR; break;
        case HAL_VIDMODE_VBR: ratemode = GM_VENC_RATEMODE_VBR; break;
        default: HAL_ERROR("gm_venc", "Video encoder does not support this mode!");
    }

    switch (config->codec) {
        case HAL_VIDCODEC_JPG:
        case HAL_VIDCODEC_MJPG:
            GM_DECLARE(gm_lib, mjpgchn, gm_venc_mjpg_cnf, "gm_mjpege_attr_t");
            mjpgchn.dest.width = config->width;
            mjpgchn.dest.height = config->height;
            mjpgchn.framerate = config->framerate;
            mjpgchn.quality = MAX(config->minQual, config->maxQual);
            mjpgchn.mode = ratemode;
            mjpgchn.bitrate = config->bitrate;
            mjpgchn.maxBitrate = MAX(config->bitrate, config->maxBitrate);
            gm_lib.fnSetDeviceConfig(_gm_venc_dev[index], &mjpgchn);
            break;
        case HAL_VIDCODEC_H264:
            GM_DECLARE(gm_lib, h264chn, gm_venc_h264_cnf, "gm_h264e_attr_t");
            h264chn.dest.width = config->width;
            h264chn.dest.height = config->height;
            h264chn.framerate = config->framerate;
            h264chn.rate.mode = ratemode;
            h264chn.rate.gop = config->gop;
            if (config->mode != HAL_VIDMODE_CBR) {
                h264chn.rate.minQual = config->minQual;
                h264chn.rate.maxQual = config->maxQual;
            }
            h264chn.rate.bitrate = config->bitrate;
            h264chn.rate.maxBitrate = MAX(config->bitrate, config->maxBitrate);
            switch (config->profile) {
                case HAL_VIDPROFILE_BASELINE:
                    h264chn.profile = GM_VENC_H264PROF_BASELINE;
                    break;
                case HAL_VIDPROFILE_MAIN:
                    h264chn.profile = GM_VENC_H264PROF_MAIN;
                    break;
                case HAL_VIDPROFILE_HIGH:
                    h264chn.profile = GM_VENC_H264PROF_HIGH;
                    break;    
            }
            h264chn.level = 41;
            gm_lib.fnSetDeviceConfig(_gm_venc_dev[index], &h264chn);
            break;
        default: HAL_ERROR("gm_venc", "This codec is not supported by the hardware!");
    }

    gm_state[index].payload = config->codec;

    _gm_venc_sz[index] = config->width * config->height * 3 / 2;

    return EXIT_SUCCESS;
}

int gm_video_destroy(char index)
{
    _gm_venc_sz[index] = 0;

    gm_state[index].enable = 0;
    gm_state[index].payload = HAL_VIDCODEC_UNSPEC;

    gm_lib.fnUnbind(_gm_venc_fds[index].bind);
    gm_lib.fnRefreshGroup(_gm_cap_grp);

    gm_lib.fnDestroyDevice(_gm_venc_dev[index]);

    return EXIT_SUCCESS;
}

int gm_video_destroy_all(void)
{
    for (char i = 0; i < GM_VENC_CHN_NUM; i++)
        if (gm_state[i].enable)
            gm_video_destroy(i);

    return EXIT_SUCCESS;
}

void gm_video_request_idr(char index)
{
    gm_lib.fnRequestIdr(_gm_venc_fds[index].bind);
}

int gm_video_snapshot_grab(short width, short height, char quality, hal_jpegdata *jpeg)
{
    int ret;
    char *buffer = malloc(GM_MAX_SNAP);

    gm_venc_snap snap;
    snap.bind = _gm_venc_fds[0].bind;
    snap.quality = quality;
    snap.buffer = buffer;
    snap.length = GM_MAX_SNAP;
    snap.dest.width = MIN(width, GM_VENC_SNAP_WIDTH_MAX);
    snap.dest.height = MIN(height, GM_VENC_SNAP_HEIGHT_MAX);

    if ((ret = gm_lib.fnSnapshot(&snap, 1000)) <= 0)
        goto abort;

    jpeg->data = buffer;
    jpeg->jpegSize = jpeg->length = ret;
    return EXIT_SUCCESS;

abort:
    free(buffer);
    HAL_ERROR("gm_venc", "Taking a snapshot failed with %#x!\n", ret);
}

void *gm_video_thread(void)
{
    int ret;
    gm_common_strm stream[GM_VENC_CHN_NUM];
    memset(stream, 0, sizeof(stream));

    int bufSize = 0;
    for (char i = 0; i < GM_VENC_CHN_NUM; i++)
        bufSize += _gm_venc_sz[i];

    char *bsData = malloc(bufSize);
    if (!bsData) goto abort;

    while (keepRunning) {
        ret = gm_lib.fnPollStream(_gm_venc_fds, GM_VENC_CHN_NUM, 500);
        if (ret == GM_ERR_TIMEOUT) {
            HAL_WARNING("gm_venc", "Main stream loop timed out!\n");
            continue;
        }

        for (char i = 0; i < GM_VENC_CHN_NUM; i++) {
            if (_gm_venc_fds[i].event.type != GM_POLL_READ)
                continue;
            if (_gm_venc_fds[i].event.bsLength > bufSize) {
                HAL_WARNING("gm_venc", "Bitstream buffer needs %d bytes "
                    "more, dropping the upcoming data!\n",
                    _gm_venc_fds[i].event.bsLength - bufSize);
                continue;
            }

            stream[i].bind = _gm_venc_fds[i].bind;
            stream[i].pack.bsData = bsData;
            stream[i].pack.bsLength = bufSize;
            stream[i].pack.mdData = 0;
            stream[i].pack.mdLength = 0;
        }

        if ((ret = gm_lib.fnReceiveStream(stream, GM_VENC_CHN_NUM)) < 0)
            HAL_WARNING("gm_venc", "Receiving the streams failed "
                "with %#x!\n", ret);
        else for (char i = 0; i < GM_VENC_CHN_NUM; i++) {
            if (!stream[i].bind) continue;
            if (stream[i].ret < 0)
                HAL_WARNING("gm_venc", "Failed to the receive bitstream on "
                    "channel %d with %#x!\n", i, stream[i].ret);
            else if (!stream[i].ret && gm_vid_cb) {
                gm_common_pack *pack = &stream[i].pack;
                hal_vidstream outStrm;
                hal_vidpack outPack[1];

                outStrm.count = 1;
                outStrm.seq = 0;
                outPack[0].data = pack->bsData;
                outPack[0].length = pack->bsSize;
                outPack[0].offset = 0;
                outPack[0].timestamp = pack->timestamp;

                signed char n = 0;
                for (unsigned int p = 0; p < pack->bsSize - 4; p++) {
                    if (pack->bsData[p] || pack->bsData[p + 1] ||
                        pack->bsData[p + 2] || pack->bsData[p + 3] != 1) continue;
                    outPack[0].nalu[n].type = pack->bsData[p + 4] & 0x1F;
                    outPack[0].nalu[n++].offset = p;
                    if (n == (pack->isKeyFrame ? 3 : 1)) break;
                }
                outPack[0].naluCnt = n;
                outPack[0].nalu[n].offset = pack->bsSize;
                for (n = 0; n < outPack[0].naluCnt; n++)
                    outPack[0].nalu[n].length = 
                        outPack[0].nalu[n + 1].offset -
                        outPack[0].nalu[n].offset;

                outStrm.pack = outPack;
                (*gm_vid_cb)(i, &outStrm);
            }
        }        
    }
abort:
    HAL_INFO("gm_venc", "Shutting down encoding thread...\n");
    free(bsData);
}

void gm_system_deinit(void)
{
    gm_lib.fnExit();
}

int gm_system_init(void)
{
    int ret;

    puts("App built with headers v" GM_LIB_API);
    printf("GrainMedia - library %#x\n", GM_LIB_VER);

    if (ret = gm_lib.fnInit(GM_LIB_VER))
        return ret;

    memset(_gm_venc_fds, 0, sizeof(_gm_venc_fds));

    return EXIT_SUCCESS;
}

#endif