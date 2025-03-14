#ifdef __mips__

#include "t31_hal.h"

t31_aud_impl  t31_aud;
t31_fs_impl   t31_fs;
t31_isp_impl  t31_isp;
t31_osd_impl  t31_osd;
t31_sys_impl  t31_sys;
t31_venc_impl t31_venc;

hal_chnstate t31_state[T31_VENC_CHN_NUM] = {0};
int (*t31_aud_cb)(hal_audframe*);
int (*t31_vid_cb)(char, hal_vidstream*);

t31_isp_snr _t31_isp_snr;
t31_common_dim _t31_snr_dim;

char _t31_aud_chn = 0;
char _t31_aud_dev = 0;
char _t31_fs_chn[T31_VENC_CHN_NUM] = {-1, -1, -1, -1};
char _t31_osd_grp = 0;

void t31_hal_deinit(void)
{
    t31_venc_unload(&t31_venc);
    t31_osd_unload(&t31_osd);
    t31_isp_unload(&t31_isp);
    t31_fs_unload(&t31_fs);
    t31_aud_unload(&t31_aud);
    t31_sys_unload(&t31_sys);
}

int t31_hal_init(void)
{
    int ret;

    if (ret = t31_sys_load(&t31_sys))
        return ret;
    if (ret = t31_aud_load(&t31_aud))
        return ret;
    if (ret = t31_fs_load(&t31_fs))
        return ret;
    if (ret = t31_isp_load(&t31_isp))
        return ret;
    if (ret = t31_osd_load(&t31_osd))
        return ret;
    if (ret = t31_venc_load(&t31_venc))
        return ret;

    return EXIT_SUCCESS;
}

void t31_audio_deinit(void)
{
    t31_aud.fnDisableChannel(_t31_aud_dev, _t31_aud_chn);

    t31_aud.fnDisableDevice(_t31_aud_dev);
}

int t31_audio_init(int samplerate)
{
    int ret;

    {
        t31_aud_cnf config;
        config.rate = samplerate;
        config.bit = T31_AUD_BIT_16;
        config.mode = T31_AUD_SND_MONO;
        config.frmNum = 40;
        config.packNumPerFrm = samplerate / 25;
        config.chnNum = 1;
        if (ret = t31_aud.fnSetDeviceConfig(_t31_aud_dev, &config))
            return ret;
    }
    if (ret = t31_aud.fnEnableDevice(_t31_aud_dev))
        return ret;
    
    {
        t31_aud_chn config;
        config.usrFrmDepth = 40;
        if (ret = t31_aud.fnSetChannelConfig(_t31_aud_dev, _t31_aud_chn, &config))
            return ret;
    }
    if (ret = t31_aud.fnEnableChannel(_t31_aud_dev, _t31_aud_chn))
        return ret;

    if (ret = t31_aud.fnSetGain(_t31_aud_dev, _t31_aud_chn, 28))
        return ret;

    if (ret = t31_aud.fnSetVolume(_t31_aud_dev, _t31_aud_chn, 60))
        return ret;
    
    return EXIT_SUCCESS;
}

void *t31_audio_thread(void)
{
    int ret;

    t31_aud_frm frame;
    memset(&frame, 0, sizeof(frame));

    while (keepRunning) {
        if (ret = t31_aud.fnPollFrame(_t31_aud_dev, _t31_aud_chn, 1000))
            continue;

        if (ret = t31_aud.fnGetFrame(_t31_aud_dev, _t31_aud_chn, &frame, 1)) {
            HAL_WARNING("t31_aud", "Getting the frame failed "
                "with %#x!\n", ret);
            continue;
        }

        if (t31_aud_cb) {
            hal_audframe outFrame;
            outFrame.channelCnt = 1;
            outFrame.data[0] = (char*)frame.addr;
            outFrame.length[0] = frame.length;
            outFrame.seq = frame.sequence;
            outFrame.timestamp = frame.timestamp;
            (t31_aud_cb)(&outFrame);
        }

        if (ret = t31_aud.fnFreeFrame(_t31_aud_dev, _t31_aud_chn, &frame)) {
            HAL_WARNING("t31_aud", "Releasing the frame failed"
                " with %#x!\n", ret);
        }
    }
    HAL_INFO("t31_aud", "Shutting down capture thread...\n");
}

int t31_channel_bind(char index)
{
    int ret;

    {
        t31_sys_bind source = { .device = T31_SYS_DEV_FS, .group = _t31_fs_chn[index], .port = 0 };
        t31_sys_bind dest = { .device = T31_SYS_DEV_OSD, .group = _t31_osd_grp, .port = index };
        if (ret = t31_sys.fnBind(&source, &dest))
            return ret;
    }

    {
        t31_sys_bind source = { .device = T31_SYS_DEV_OSD, .group = _t31_osd_grp, .port = index };
        t31_sys_bind dest = { .device = T31_SYS_DEV_ENC, .group = index, .port = 0 };
        if (ret = t31_sys.fnBind(&source, &dest))
            return ret;
    }

    if (ret = t31_fs.fnEnableChannel(_t31_fs_chn[index]))
        return ret;

    return EXIT_SUCCESS;
}

int t31_channel_create(char index, short width, short height, char framerate, char jpeg)
{
    int ret;

    {
        t31_fs_chn channel = {
            .dest = { .width = width, .height = height }, .pixFmt = T31_PIXFMT_NV12,
            .scale = { .enable = (_t31_snr_dim.width != width || _t31_snr_dim.height != height) 
                ? 1 : 0, .width = width, .height = height },
            .fpsNum = framerate, .fpsDen = 1, .bufCount = jpeg ? 1 : 2, .phyOrExtChn = 0,
        };
    
        _t31_fs_chn[index] = index;

        if (ret = t31_fs.fnCreateChannel(_t31_fs_chn[index], &channel))
            return ret;
    }

    return EXIT_SUCCESS;
}

void t31_channel_destroy(char index)
{
    t31_fs.fnDestroyChannel(_t31_fs_chn[index]);

    _t31_fs_chn[index] = -1;
}

int t31_channel_grayscale(char enable)
{
    return t31_isp.fnSetRunningMode(enable & 1);
}

int t31_channel_unbind(char index)
{
    int ret;

    t31_fs.fnDisableChannel(_t31_fs_chn[index]);

    {
        t31_sys_bind source = { .device = T31_SYS_DEV_OSD, .group = _t31_osd_grp, .port = index };
        t31_sys_bind dest = { .device = T31_SYS_DEV_ENC, .group = index, .port = 0 };
        t31_sys.fnUnbind(&source, &dest);
    }

    {
        t31_sys_bind source = { .device = T31_SYS_DEV_FS, .group = _t31_fs_chn[index], .port = 0 };
        t31_sys_bind dest = { .device = T31_SYS_DEV_OSD, .group = _t31_osd_grp, .port = index };
        t31_sys.fnUnbind(&source, &dest);
    }

    return EXIT_SUCCESS;
}

int t31_config_load(char *path)
{
    t31_isp.fnDisableSensor();
    int ret = t31_isp.fnLoadConfig(path);
    t31_isp.fnEnableSensor();

    return ret;
}

int t31_pipeline_create(char mirror, char flip, char antiflicker, char framerate)
{
    int ret = EXIT_FAILURE;

    {
        char* paths[] = {"/proc/jz/sinfo/info", "/proc/jz/sensor/name"};
        char **path = paths;
        char *sensorlocal, sensorname[50];
        FILE *sensorinfo;

        while (*path) {
            if (sensorinfo = fopen(*path, "r")) {
                sensorlocal = fgets(sensorname, 50, sensorinfo);
                fclose(sensorinfo);
                if (sensorlocal) {
                    sensorlocal = strstr(sensorname, ":");
                    if (!sensorlocal++) goto sensor_from_env;
                    sensorlocal[strlen(sensorlocal) - 1] = '\0';
                    goto sensor_found;
                }
            }
            else *path++;
        }
        
        if (!sensorlocal)
            HAL_INFO("t31_hal", "Couldn't determine the sensor name from kernel modules!\n");

sensor_from_env:
        sensorlocal = getenv("SENSOR");

sensor_found:
        if (*sensorlocal) strncpy(sensor, sensorlocal, sizeof(sensor));

        for (char i = 0; i < sizeof(t31_sensors) / sizeof(*t31_sensors); i++) {
            if (strcmp(t31_sensors[i].name, sensorlocal)) continue;
            memcpy(&_t31_isp_snr, &t31_sensors[i], sizeof(t31_isp_snr));
            if (t31_sensors[i].mode == T31_ISP_COMM_I2C)
                memcpy(&_t31_isp_snr.i2c.type, &t31_sensors[i].name, strlen(t31_sensors[i].name));
            else if (t31_sensors[i].mode == T31_ISP_COMM_SPI)
                memcpy(&_t31_isp_snr.spi.alias, &t31_sensors[i].name, strlen(t31_sensors[i].name));
            _t31_snr_dim = t31_dims[i];
            ret = 0;
            break;
        }
        if (ret)
            HAL_ERROR("t31_hal", "Unknown sensor, please update the app!\n");
    }
    if (ret = t31_isp.fnInit())
        return ret;
    if (ret = t31_isp.fnAddSensor(&_t31_isp_snr))
        return ret;
    if (ret = t31_isp.fnEnableSensor())
        return ret;
    if (ret = t31_isp.fnEnableTuning())
        return ret;
    if (ret = t31_isp.fnSetRunningMode(0))
        return ret;
    {
        t31_isp_flick mode = T31_ISP_FLICK_OFF;
        if (antiflicker >= 60)
            mode = T31_ISP_FLICK_60HZ;
        else if (antiflicker >= 50)
            mode = T31_ISP_FLICK_50HZ;
        if (ret = t31_isp.fnSetAntiFlicker(mode))
            return ret;
    }
    if (ret = t31_isp.fnSetFlip((mirror ? 1 : 0) | (flip ? 2 : 0)))
        return ret;
    if (ret = t31_isp.fnSetFramerate(framerate, 1))
        return ret;

    if (ret = t31_osd.fnCreateGroup(_t31_osd_grp))
        return ret;
    if (ret = t31_osd.fnStartGroup(_t31_osd_grp))
        return ret;

    return EXIT_SUCCESS;
}

void t31_pipeline_destroy(void)
{
    t31_osd.fnStopGroup(_t31_osd_grp);
    t31_osd.fnDestroyGroup(_t31_osd_grp);

    t31_isp.fnDisableSensor();
    t31_isp.fnDeleteSensor(&_t31_isp_snr);
    t31_isp.fnExit();
}

int t31_region_create(int *handle, hal_rect rect, short opacity)
{
    int ret;

    t31_osd_rgn region, regionCurr;
    t31_osd_grp attrib, attribCurr;

    region.type = T31_OSD_TYPE_PIC;
    region.pixFmt = T31_PIXFMT_BGR555LE;
    region.rect.p0.x = rect.x;
    region.rect.p0.y = rect.y;
    region.rect.p1.x = rect.x + rect.width - 1;
    region.rect.p1.y = rect.y + rect.height - 1;
    region.data.picture = NULL;

    if (t31_osd.fnGetRegionConfig(*handle, &regionCurr)) {
        HAL_INFO("t31_osd", "Creating region...\n");
        if ((ret = t31_osd.fnCreateRegion(&region)) < 0)
            return ret;
        else *handle = ret;
    } else if (regionCurr.rect.p1.y - regionCurr.rect.p0.y != rect.height || 
        regionCurr.rect.p1.x - regionCurr.rect.p0.x != rect.width) {
        HAL_INFO("t31_osd", "Parameters are different, recreating "
            "region...\n");
        if (ret = t31_osd.fnSetRegionConfig(*handle, &region))
            return ret;
        if (ret = t31_osd.fnUnregisterRegion(*handle, _t31_osd_grp))
            return ret;
    }

    if (t31_osd.fnGetGroupConfig(*handle, _t31_osd_grp, &attribCurr))
        HAL_INFO("t31_osd", "Attaching region...\n");

    memset(&attrib, 0, sizeof(attrib));
    attrib.show = 1;
    attrib.alphaOn = 1;
    attrib.fgAlpha = opacity;
    
    t31_osd.fnRegisterRegion(*handle, _t31_osd_grp, &attrib);

    return EXIT_SUCCESS;
}

void t31_region_destroy(int *handle)
{
    t31_osd.fnUnregisterRegion(*handle, _t31_osd_grp);
    *handle = -1;
}

int t31_region_setbitmap(int *handle, hal_bitmap *bitmap)
{
    t31_osd_rgn region;
    t31_osd.fnGetRegionConfig(*handle, &region);
    region.type = T31_OSD_TYPE_PIC;
    region.rect.p1.x = region.rect.p0.x + bitmap->dim.width - 1;
    region.rect.p1.y = region.rect.p0.y + bitmap->dim.height - 1;
    region.pixFmt = T31_PIXFMT_BGR555LE;
    region.data.picture = bitmap->data;    
    return t31_osd.fnSetRegionConfig(*handle, &region);
}

int t31_video_create(char index, hal_vidconfig *config)
{
    int ret;
    t31_venc_chn channel;
    t31_venc_prof profile;
    t31_venc_ratemode ratemode;

    switch (config->mode) {
        case HAL_VIDMODE_CBR: ratemode = T31_VENC_RATEMODE_CBR; break;
        case HAL_VIDMODE_VBR: ratemode = T31_VENC_RATEMODE_VBR; break;
        case HAL_VIDMODE_QP: ratemode = T31_VENC_RATEMODE_QP; break;
        case HAL_VIDMODE_AVBR: ratemode = T31_VENC_RATEMODE_AVBR; break;
        default: HAL_ERROR("t31_venc", "Video encoder does not support this mode!");
    }
    switch (config->codec) {
        case HAL_VIDCODEC_JPG:
            config->framerate = config->gop = 1;
        case HAL_VIDCODEC_MJPG:
            profile = T31_VENC_PROF_MJPG;
            if (ratemode == T31_VENC_RATEMODE_QP) {
                config->minQual = config->minQual * (48 - 3) / 99 + 3;
                config->maxQual = config->maxQual * (48 - 3) / 99 + 3;
            } else config->minQual = config->maxQual = 42;
            break;
        case HAL_VIDCODEC_H265: profile = T31_VENC_PROF_H265_MAIN; break;
        case HAL_VIDCODEC_H264:
            switch (config->profile) {
                case HAL_VIDPROFILE_BASELINE: profile = T31_VENC_PROF_H264_BASE; break;
                case HAL_VIDPROFILE_MAIN: profile = T31_VENC_PROF_H264_MAIN; break;
                default: profile = T31_VENC_PROF_H264_HIGH; break;
            } break;
        default: HAL_ERROR("t31_venc", "This codec is not supported by the hardware!");
    }

    memset(&channel, 0, sizeof(channel));
    t31_venc.fnSetDefaults(&channel, profile, ratemode, config->width, config->height, 
        config->framerate, 1, config->gop, config->gop / config->framerate, -1, 0);

    switch (channel.rate.mode) {
        case T31_VENC_RATEMODE_CBR:
            channel.rate.cbr = (t31_venc_rate_cbr){ .tgtBitrate = MAX(config->bitrate, config->maxBitrate), 
                .initQual = -1, .minQual = 34, .maxQual = 48, .ipDelta = -1, .pbDelta = -1,
                .options = T31_VENC_RCOPT_SCN_CHG_RES | T31_VENC_RCOPT_SC_PREVENTION }; break;
        case T31_VENC_RATEMODE_VBR:
            channel.rate.vbr = (t31_venc_rate_vbr){ .tgtBitrate = config->bitrate,
                .maxBitrate = config->maxBitrate, .initQual = -1, .minQual = 34, .maxQual = 48,
                .ipDelta = -1, .pbDelta = -1, .options = T31_VENC_RCOPT_SCN_CHG_RES |
                T31_VENC_RCOPT_SC_PREVENTION }; break;
        case T31_VENC_RATEMODE_QP:
            channel.rate.qpModeQual = MAX(config->minQual, config->maxQual); break;
        case T31_VENC_RATEMODE_AVBR:
            channel.rate.avbr = (t31_venc_rate_xvbr){ .tgtBitrate = config->bitrate, .maxBitrate = 
                config->maxBitrate, .initQual = -1, .minQual = config->minQual, .maxQual = config->maxQual,
                .ipDelta = -1, .pbDelta = -1, .options = T31_VENC_RCOPT_SCN_CHG_RES |
                T31_VENC_RCOPT_SC_PREVENTION, .maxPsnr = 42 }; break;
    }

    if (ret = t31_venc.fnCreateGroup(index))
        return ret;

    if (ret = t31_venc.fnCreateChannel(index, &channel))
        return ret;

    if (ret = t31_venc.fnRegisterChannel(index, index))
        return ret;

    if (config->codec != HAL_VIDCODEC_JPG && 
        (ret = t31_venc.fnStartReceiving(index)))
        return ret;
    
    t31_state[index].payload = config->codec;

    return EXIT_SUCCESS;
}

int t31_video_destroy(char index)
{
    int ret;

    t31_state[index].enable = 0;
    t31_state[index].payload = HAL_VIDCODEC_UNSPEC;

    t31_channel_destroy(index);

    t31_venc.fnStopReceiving(index);

    if (ret = t31_venc.fnUnregisterChannel(index))
        return ret;

    if (ret = t31_venc.fnDestroyChannel(index))
        return ret;

    if (ret = t31_venc.fnDestroyGroup(index))
        return ret;

    return EXIT_SUCCESS;
}

int t31_video_destroy_all(void)
{
    int ret;

    for (char i = 0; i < T31_VENC_CHN_NUM; i++)
        if (t31_state[i].enable)
            if (ret = t31_video_destroy(i))
                return ret;

    return EXIT_SUCCESS;
}

void t31_video_request_idr(char index)
{
    t31_venc.fnRequestIdr(index);
}

int t31_video_snapshot_grab(char index, hal_jpegdata *jpeg)
{
    int ret;
    char mjpeg = 0;

    if (index == -1) {
        HAL_INFO("t31_venc", "Snapshot falling back to the MJPEG channel,"
            " its resolution will be used in place\n");
        for (char i = 0; i < T31_VENC_CHN_NUM; i++) {
            if (!t31_state[i].enable) continue; 
            if (t31_state[i].payload != HAL_VIDCODEC_MJPG) continue;
            index = i;
            mjpeg = 1;
        }
        if (index == -1) return EXIT_FAILURE;
    }

    if (!mjpeg) {
        if (ret = t31_channel_bind(index)) {
            HAL_DANGER("t31_venc", "Binding the encoder channel "
                "%d failed with %#x!\n", index, ret);
            goto abort;
        }

        if (ret = t31_venc.fnStartReceiving(index)) {
            HAL_DANGER("t31_venc", "Requesting one frame on"
                "channel %d failed with %#x!\n", index, ret);
            goto abort;
        }
    }

    ret = t31_venc.fnPollStream(index, 2000);
    if (ret < 0) {
        HAL_DANGER("t31_venc", "Polling the encoder channel "
            "%d failed!\n", index);
        goto abort;
    }
    
    t31_venc_stat stat;
    if (ret = t31_venc.fnQuery(index, &stat)) {
        HAL_DANGER("t31_venc", "Querying the encoder channel "
            "%d failed with %#x!\n", index, ret);
        goto abort;
    }

    if (!stat.curPacks) {
        HAL_DANGER("t31_venc", "Current frame is empty, skipping it!\n");
        goto abort;
    }

    t31_venc_strm strm;
    memset(&strm, 0, sizeof(strm));
    strm.packet = (t31_venc_pack*)malloc(sizeof(t31_venc_pack) * stat.curPacks);
    if (!strm.packet) {
        HAL_DANGER("t31_venc", "Memory allocation on channel %d failed!\n", index);
        goto abort;
    }
    strm.count = stat.curPacks;

    if (ret = t31_venc.fnGetStream(index, &strm, 0)) {
        HAL_DANGER("t31_venc", "Getting the stream on "
            "channel %d failed with %#x!\n", index, ret);
        free(strm.packet);
        strm.packet = NULL;
        goto abort;
    }

    {
        jpeg->jpegSize = 0;
        for (unsigned int i = 0; i < strm.count; i++) {
            t31_venc_pack *pack = &strm.packet[i];
            unsigned int packLen = pack->length - pack->offset;
            unsigned char *packData = (unsigned char*)(strm.addr + pack->offset);

            unsigned int newLen = jpeg->jpegSize + packLen;
            if (newLen > jpeg->length) {
                jpeg->data = realloc(jpeg->data, newLen);
                jpeg->length = newLen;
            }
            memcpy(jpeg->data + jpeg->jpegSize, packData, packLen);
            jpeg->jpegSize += packLen;
        }
    }

abort:
    t31_venc.fnFreeStream(index, &strm);

    if (!mjpeg) {
        t31_venc.fnStopReceiving(index);

        t31_channel_unbind(index);
    }

    return ret;
}

void *t31_video_thread(void)
{
    int ret, maxFd = 0;

    for (int i = 0; i < T31_VENC_CHN_NUM; i++) {
        if (!t31_state[i].enable) continue;
        if (!t31_state[i].mainLoop) continue;

        ret = t31_venc.fnGetDescriptor(i);
        if (ret < 0) {
            HAL_DANGER("t31_venc", "Getting the encoder descriptor failed with %#x!\n", ret);
            return NULL;
        }
        t31_state[i].fileDesc = ret;

        if (maxFd <= t31_state[i].fileDesc)
            maxFd = t31_state[i].fileDesc;
    }

    t31_venc_stat stat;
    struct timeval timeout;
    fd_set readFds;

    while (keepRunning) {
        FD_ZERO(&readFds);
        for(int i = 0; i < T31_VENC_CHN_NUM; i++) {
            if (!t31_state[i].enable) continue;
            if (!t31_state[i].mainLoop) continue;
            FD_SET(t31_state[i].fileDesc, &readFds);
        }

        timeout.tv_sec = 2;
        timeout.tv_usec = 0;
        ret = select(maxFd + 1, &readFds, NULL, NULL, &timeout);
        if (ret < 0) {
            HAL_DANGER("t31_venc", "Select operation failed!\n");
            break;
        } else if (ret == 0) {
            HAL_WARNING("t31_venc", "Main stream loop timed out!\n");
            continue;
        } else {
            for (int i = 0; i < T31_VENC_CHN_NUM; i++) {
                if (!t31_state[i].enable) continue;
                if (!t31_state[i].mainLoop) continue;
                if (FD_ISSET(t31_state[i].fileDesc, &readFds)) {
                    t31_venc_strm stream;

                    if (ret = t31_venc.fnGetStream(i, &stream, 0)) {
                        HAL_DANGER("t31_venc", "Getting the stream on "
                            "channel %d failed with %#x!\n", i, ret);
                        break;
                    }

                    if (t31_vid_cb) {
                        hal_vidstream outStrm;
                        hal_vidpack outPack[stream.count];
                        memset(outPack, 0, sizeof(outPack));
                        outStrm.count = stream.count;
                        outStrm.seq = stream.sequence;
                        for (int j = 0; j < stream.count; j++) {
                            t31_venc_pack *pack = &stream.packet[j];
                            if (!pack->length) continue;
                            unsigned int remain = stream.length - pack->offset;
                            if (remain < pack->length) {
                                outPack[j].data = (unsigned char*)(stream.addr);
                                outPack[j].length = pack->length - remain;
                            } else {
                                outPack[j].data = (unsigned char*)(stream.addr + pack->offset);
                                outPack[j].length = pack->length;
                            }
                            outPack[j].naluCnt = 1;
                            outPack[j].nalu[0].length = outPack[j].length;
                            outPack[j].nalu[0].offset = 0;
                            switch (t31_state[i].payload) {
                                case HAL_VIDCODEC_H264:
                                    outPack[j].nalu[0].type = pack->naluType.h264Nalu;
                                    break;
                                case HAL_VIDCODEC_H265:
                                    outPack[j].nalu[0].type = pack->naluType.h265Nalu;
                                    break;
                            }
                            outPack[j].offset = 0;
                            outPack[j].timestamp = pack->timestamp;
                        }
                        outStrm.pack = outPack;
                        (*t31_vid_cb)(i, &outStrm);
                    }

                    if (ret = t31_venc.fnFreeStream(i, &stream)) {
                        HAL_DANGER("t31_venc", "Releasing the stream on "
                            "channel %d failed with %#x!\n", i, ret);
                    }
                }
            }
        }
    }

    HAL_INFO("t31_venc", "Shutting down encoding thread...\n");
}

void t31_system_deinit(void)
{
    t31_sys.fnExit();
}

int t31_system_init(void)
{
    int ret;

    {
        t31_sys_ver version;
        if (ret = t31_sys.fnGetVersion(&version))
            return ret;
        printf("App built with headers v%s\n", T31_SYS_API);
        puts(version.version);
    }

    if (ret = t31_sys.fnInit())
        return ret;

    return EXIT_SUCCESS;
}

#endif