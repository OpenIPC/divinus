#include "t31_hal.h"

t31_aud_impl  t31_aud;
t31_fs_impl   t31_fs;
t31_isp_impl  t31_isp;
t31_osd_impl  t31_osd;
t31_sys_impl  t31_sys;
t31_venc_impl t31_venc;

hal_chnstate t31_state[T31_VENC_CHN_NUM] = {0};
int (*t31_venc_cb)(char, hal_vidstream*);

t31_isp_snr _t31_isp_snr;
t31_common_dim _t31_snr_dim;

char _t31_aud_chn = 0;
char _t31_aud_dev = 0;
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

int t31_audio_init(void)
{
    int ret;

    {
        t31_aud_cnf config;
        config.rate = 48000;
        config.bit = T31_AUD_BIT_16;
        config.mode = T31_AUD_SND_MONO;
        config.frmNum = 0;
        config.packNumPerFrm = 0;
        config.chnNum = 0;
        if (ret = t31_aud.fnSetDeviceConfig(_t31_aud_dev, &config))
            return ret;
    }
    if (ret = t31_aud.fnEnableDevice(_t31_aud_dev))
        return ret;
    
    if (ret = t31_aud.fnEnableChannel(_t31_aud_dev, _t31_aud_chn))
        return ret;
    {
        int dbLevel = 0xF6;
        if (ret = t31_aud.fnSetVolume(_t31_aud_dev, _t31_aud_chn, &dbLevel))
                return ret;
    }
    
    return EXIT_SUCCESS;
}

int t31_channel_bind(char index)
{
    int ret;

    {
        t31_sys_bind source = { .device = T31_SYS_DEV_FS, .group = index, .port = 0 };
        t31_sys_bind dest = { .device = T31_SYS_DEV_OSD, .group = _t31_osd_grp, .port = 0 };
        if (ret = t31_sys.fnBind(&source, &dest))
            return ret;
    }

    {
        t31_sys_bind source = { .device = T31_SYS_DEV_OSD, .group = _t31_osd_grp, .port = 0 };
        t31_sys_bind dest = { .device = T31_SYS_DEV_ENC, .group = index, .port = 0 };
        if (ret = t31_sys.fnBind(&source, &dest))
            return ret;
    }

    if (ret = t31_fs.fnEnableChannel(index))
        return ret;

    return EXIT_SUCCESS;
}

int t31_channel_create(char index, short width, short height, char framerate)
{
    int ret;

    {
        t31_fs_chn channel = {
            .dest = { .width = width, .height = height }, .pixFmt = T31_PIXFMT_NV12,
            .crop = { .enable = 0, .left = 0, .top = 0,  .width = _t31_snr_dim.width, 
                .height = _t31_snr_dim.height },
            .scale = { .enable = (_t31_snr_dim.width != width || _t31_snr_dim.height != height) 
                ? 1 : 0, .width = width, .height = height },
            .fpsDen = framerate, .fpsNum = 1, .bufCount = 1, .phyOrExtChn = 0,  
        };

        if (ret = t31_fs.fnCreateChannel(index, &channel))
            return ret;
    }

    if (ret = t31_venc.fnCreateGroup(index))
        return ret;

    return EXIT_SUCCESS;
}

void t31_channel_destroy(char index)
{
    t31_venc.fnDestroyGroup(index);

    t31_fs.fnDestroyChannel(index);
}

int t31_channel_unbind(char index)
{
    int ret;

    t31_fs.fnDisableChannel(index);

    {
        t31_sys_bind source = { .device = T31_SYS_DEV_OSD, .group = _t31_osd_grp, .port = 0 };
        t31_sys_bind dest = { .device = T31_SYS_DEV_ENC, .group = index, .port = 0 };
        t31_sys.fnUnbind(&source, &dest);
    }

    {
        t31_sys_bind source = { .device = T31_SYS_DEV_FS, .group = index, .port = 0 };
        t31_sys_bind dest = { .device = T31_SYS_DEV_OSD, .group = _t31_osd_grp, .port = 0 };
        t31_sys.fnUnbind(&source, &dest);
    }

    return EXIT_SUCCESS;
}

int t31_region_create(int *handle, hal_rect rect)
{
    int ret;

    t31_osd_rgn region, regionCurr;
    t31_osd_grp attrib, attribCurr;

    region.type = T31_OSD_TYPE_PIC;
    region.pixFmt = T31_PIXFMT_RGB555LE;
    region.rect.p0.x = rect.x;
    region.rect.p0.y = rect.y;
    region.rect.p1.x = rect.x + rect.width - 1;
    region.rect.p1.y = rect.y + rect.height - 1;
    region.data.picture = NULL;

    if (t31_osd.fnGetRegionConfig(*handle, &regionCurr)) {
        fprintf(stderr, "[t31_osd] Creating region...\n", _t31_osd_grp);
        if ((ret = t31_osd.fnCreateRegion(&region)) < 0)
            return ret;
        else *handle = ret;
    } else if (regionCurr.rect.p1.y - regionCurr.rect.p0.y != rect.height || 
        regionCurr.rect.p1.x - regionCurr.rect.p0.x != rect.width) {
        fprintf(stderr, "[t31_osd] Parameters are different, recreating "
            "region...\n", _t31_osd_grp);
        if (ret = t31_osd.fnSetRegionConfig(*handle, &region))
            return ret;
    }

    if (t31_osd.fnGetGroupConfig(*handle, _t31_osd_grp, &attribCurr))
        fprintf(stderr, "[t31_osd] Attaching region...\n", _t31_osd_grp);

    memset(&attrib, 0, sizeof(attrib));
    attrib.show = 1;
    attrib.alphaOn = 1;
    attrib.fgAlpha = 128;
    
    t31_osd.fnRegisterRegion(*handle, _t31_osd_grp, &attrib);

    return ret;
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
    region.pixFmt = T31_PIXFMT_RGB555LE;
    region.data.picture = bitmap->data;    
    return t31_osd.fnSetRegionConfig(*handle, &region);
}

int t31_video_create(char index, hal_vidconfig *config)
{
    int ret;
    t31_venc_chn channel;
    memset(&channel, 0, sizeof(channel));
    channel.gop.mode = T31_VENC_GOPMODE_NORMAL;
    channel.gop.length = config->gop / config->framerate;
    channel.rate.fpsDen = config->framerate;
    channel.rate.fpsNum = 1;
    switch (config->codec) {
        case HAL_VIDCODEC_JPG:
            channel.attrib.profile = T31_VENC_PROF_MJPG;
            break;
        case HAL_VIDCODEC_MJPG:
            channel.attrib.profile = T31_VENC_PROF_MJPG;
            break;
        case HAL_VIDCODEC_H265:
            channel.attrib.profile = T31_VENC_PROF_H265_MAIN;
            break;
        case HAL_VIDCODEC_H264:
            switch (config->profile) {
                case HAL_VIDPROFILE_BASELINE: channel.attrib.profile = T31_VENC_PROF_H264_BASE; break;
                case HAL_VIDPROFILE_MAIN: channel.attrib.profile = T31_VENC_PROF_H264_MAIN; break;
                default: channel.attrib.profile = T31_VENC_PROF_H264_HIGH; break;
            }
            break;
        default: T31_ERROR("This codec is not supported by the hardware!");

    }
    switch (config->mode) {
        case HAL_VIDMODE_CBR:
            channel.rate.mode = T31_VENC_RATEMODE_CBR;
            channel.rate.cbr = (t31_venc_rate_cbr){ .tgtBitrate = MAX(config->bitrate,
                config->maxBitrate), .initQual = -1, .minQual = 34, .maxQual = 51,
                .ipDelta = -1, .pbDelta = -1, .options = T31_VENC_RCOPT_SCN_CHG_RES | 
                T31_VENC_RCOPT_SC_PREVENTION, .maxPicSize = MAX(config->bitrate,
                config->maxBitrate) / 4 * 3 }; break;
        case HAL_VIDMODE_VBR:
            channel.rate.mode = T31_VENC_RATEMODE_VBR;
            channel.rate.vbr = (t31_venc_rate_vbr){ .tgtBitrate = config->bitrate, 
                .maxBitrate = config->maxBitrate, .initQual = -1, .minQual = 34, .maxQual = 51,
                .ipDelta = -1, .pbDelta = -1, .options = T31_VENC_RCOPT_SCN_CHG_RES | 
                T31_VENC_RCOPT_SC_PREVENTION , .maxPicSize = config->maxBitrate }; break;
        case HAL_VIDMODE_QP:
            channel.rate.mode = T31_VENC_RATEMODE_QP;
            channel.rate.qpModeQual = config->maxQual; break;
        case HAL_VIDMODE_AVBR:
            channel.rate.mode = T31_VENC_RATEMODE_AVBR;
            channel.rate.avbr = (t31_venc_rate_xvbr){ .tgtBitrate  = config->bitrate, 
                .maxBitrate = config->maxBitrate,  .initQual = -1, .minQual = 34, .maxQual = 51,
                .ipDelta = -1, .pbDelta = -1, .options = T31_VENC_RCOPT_SCN_CHG_RES | 
                T31_VENC_RCOPT_SC_PREVENTION, .maxPicSize = config->maxBitrate,
                .maxPsnr = 42 }; break;
        default:
            T31_ERROR("Video encoder does not support this mode!");
    }
    channel.attrib.width = config->width;
    channel.attrib.height = config->height;
    channel.attrib.picFmt = (config->codec == HAL_VIDCODEC_JPG || config->codec == HAL_VIDCODEC_MJPG) ?
        T31_VENC_PICFMT_422_8BPP : T31_VENC_PICFMT_420_8BPP;

    if (ret = t31_venc.fnCreateChannel(index, &channel))
        return ret;

    if (ret = t31_venc.fnRegisterChannel(index, index))
        return ret;

    {
        int count = -1;
        if (config->codec != HAL_VIDCODEC_JPG && 
            (ret = t31_venc.fnStartReceiving(index)))
            return ret;
    }
    
    t31_state[index].payload = config->codec;

    return EXIT_SUCCESS;
}

int t31_video_destroy(char index)
{
    int ret;

    t31_state[index].payload = HAL_VIDCODEC_UNSPEC;

    t31_venc.fnStopReceiving(index);

    if (ret = t31_venc.fnUnregisterChannel(index))
        return ret;

    if (ret = t31_venc.fnDestroyChannel(index))
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

void *t31_video_thread(void)
{
    int ret;
    int maxFd = 0;

    for (int i = 0; i < T31_VENC_CHN_NUM; i++) {
        if (!t31_state[i].enable) continue;
        if (!t31_state[i].mainLoop) continue;

        ret = t31_venc.fnGetDescriptor(i);
        if (ret < 0) {
            fprintf(stderr, "[t31_venc] Getting the encoder descriptor failed with %#x!\n", ret);
            return NULL;
        }
        t31_state[i].fileDesc = ret;

        if (maxFd <= t31_state[i].fileDesc)
            maxFd = t31_state[i].fileDesc;
    }

    t31_venc_stat stat;
    t31_venc_strm stream;
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
            fprintf(stderr, "[t31_venc] Select operation failed!\n");
            break;
        } else if (ret == 0) {
            fprintf(stderr, "[t31_venc] Main stream loop timed out!\n");
            continue;
        } else {
            for (int i = 0; i < T31_VENC_CHN_NUM; i++) {
                if (!t31_state[i].enable) continue;
                if (!t31_state[i].mainLoop) continue;
                if (FD_ISSET(t31_state[i].fileDesc, &readFds)) {
                    memset(&stream, 0, sizeof(stream));
                    
                    if (ret = t31_venc.fnQuery(i, &stat)) {
                        fprintf(stderr, "[t31_venc] Querying the encoder channel "
                            "%d failed with %#x!\n", i, ret);
                        break;
                    }

                    if (!stat.curPacks) {
                        fprintf(stderr, "[t31_venc] Current frame is empty, skipping it!\n");
                        continue;
                    }

                    stream.packet = (t31_venc_pack*)malloc(
                        sizeof(t31_venc_pack) * stat.curPacks);
                    if (!stream.packet) {
                        fprintf(stderr, "[t31_venc] Memory allocation on channel %d failed!\n", i);
                        break;
                    }
                    stream.count = stat.curPacks;

                    if (ret = t31_venc.fnGetStream(i, &stream, stat.curPacks)) {
                        fprintf(stderr, "[t31_venc] Getting the stream on "
                            "channel %d failed with %#x!\n", i, ret);
                        break;
                    }

                    if (t31_venc_cb) {
                        hal_vidstream outStrm;
                        hal_vidpack outPack[stat.curPacks];
                        outStrm.count = stream.count;
                        outStrm.seq = stream.sequence;
                        for (int j = 0; j < stat.curPacks; j++) {
                            outPack[j].data = (unsigned char*)stream.addr + stream.packet[j].offset;
                            outPack[j].length = stream.packet[j].length;
                            outPack[j].offset = stream.packet[j].offset;
                        }
                        outStrm.pack = outPack;
                        (*t31_venc_cb)(i, &outStrm);
                    }

                    if (ret = t31_venc.fnFreeStream(i, &stream)) {
                        fprintf(stderr, "[t31_venc] Releasing the stream on "
                            "channel %d failed with %#x!\n", i, ret);
                    }
                    free(stream.packet);
                    stream.packet = NULL;
                }
            }
        }
    }
    fprintf(stderr, "[t31_venc] Shutting down encoding thread...\n");
}

void t31_system_deinit(void)
{
    t31_osd.fnDestroyGroup(_t31_osd_grp);

    t31_sys.fnExit();

    t31_isp.fnDisableSensor();
    t31_isp.fnDeleteSensor(&_t31_isp_snr);
    t31_isp.fnExit();
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

    {
        const char *sensor = getenv("SENSOR");
        for (char i = 0; i < sizeof(t31_sensors) / sizeof(*t31_sensors); i++) {
            if (strcmp(t31_sensors[i].name, sensor)) continue;
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
            return EXIT_FAILURE;
    }

    if (ret = t31_isp.fnInit())
        return ret;

    if (ret = t31_isp.fnAddSensor(&_t31_isp_snr))
        return ret;

    if (ret = t31_isp.fnEnableSensor())
        return ret;

    if (ret = t31_sys.fnInit())
        return ret;

    if (ret = t31_osd.fnCreateGroup(_t31_osd_grp))
        return ret;

    return EXIT_SUCCESS;
}
