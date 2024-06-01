#include "tx_hal.h"

tx_aud_impl  tx_aud;
tx_fs_impl   tx_fs;
tx_isp_impl  tx_isp;
tx_osd_impl  tx_osd;
tx_sys_impl  tx_sys;
tx_venc_impl tx_venc;

hal_chnstate tx_state[TX_VENC_CHN_NUM] = {0};
int (*tx_venc_cb)(char, hal_vidstream*);

tx_isp_snr _tx_isp_snr;
tx_common_dim _tx_snr_dim;

char _tx_aud_chn = 0;
char _tx_aud_dev = 0;
char _tx_osd_grp = 0;

void tx_hal_deinit(void)
{
    tx_venc_unload(&tx_venc);
    tx_osd_unload(&tx_osd);
    tx_isp_unload(&tx_isp);
    tx_fs_unload(&tx_fs);
    tx_aud_unload(&tx_aud);
    tx_sys_unload(&tx_sys);
}

int tx_hal_init(void)
{
    int ret;

    if (ret = tx_sys_load(&tx_sys))
    return ret;
    if (ret = tx_aud_load(&tx_aud))
    return ret;
    if (ret = tx_fs_load(&tx_fs))
    return ret;
    if (ret = tx_isp_load(&tx_isp))
    return ret;
    if (ret = tx_osd_load(&tx_osd))
    return ret;
    if (ret = tx_venc_load(&tx_venc))
    return ret;

    return EXIT_SUCCESS;
}

void tx_audio_deinit(void)
{
    tx_aud.fnDisableChannel(_tx_aud_dev, _tx_aud_chn);

    tx_aud.fnDisableDevice(_tx_aud_dev);
}

int tx_audio_init(void)
{
    int ret;

    {
        tx_aud_cnf config;
        config.rate = 48000;
        config.bit = TX_AUD_BIT_16;
        config.mode = TX_AUD_SND_MONO;
        config.frmNum = 0;
        config.packNumPerFrm = 0;
        config.chnNum = 0;
        if (ret = tx_aud.fnSetDeviceConfig(_tx_aud_dev, &config))
            return ret;
    }
    if (ret = tx_aud.fnEnableDevice(_tx_aud_dev))
        return ret;
    
    if (ret = tx_aud.fnEnableChannel(_tx_aud_dev, _tx_aud_chn))
        return ret;
    {
        int dbLevel = 0xF6;
        if (ret = tx_aud.fnSetVolume(_tx_aud_dev, _tx_aud_chn, &dbLevel))
                return ret;
    }
    
    return EXIT_SUCCESS;
}

int tx_channel_bind(char index)
{
    int ret;

    {
        tx_sys_bind source = { .device = TX_SYS_DEV_FS, .group = index, .port = 0 };
        tx_sys_bind dest = { .device = TX_SYS_DEV_OSD, .group = _tx_osd_grp, .port = 0 };
        if (ret = tx_sys.fnBind(&source, &dest))
            return ret;
    }

    {
        tx_sys_bind source = { .device = TX_SYS_DEV_OSD, .group = _tx_osd_grp, .port = 0 };
        tx_sys_bind dest = { .device = TX_SYS_DEV_ENC, .group = index, .port = 0 };
        if (ret = tx_sys.fnBind(&source, &dest))
            return ret;
    }

    if (ret = tx_fs.fnEnableChannel(index))
        return ret;

    return EXIT_SUCCESS;
}

int tx_channel_create(char index, short width, short height, char framerate)
{
    int ret;

    {
        tx_fs_chn channel = {
            .dest = { .width = width, .height = height }, .pixFmt = TX_PIXFMT_NV12,
            .crop = { .enable = 1, .left = 0, .top = 0,  .width = _tx_snr_dim.width, 
                .height = _tx_snr_dim.height },
            .scale = { .enable = (_tx_snr_dim.width != width || _tx_snr_dim.height != height) 
                ? 1 : 0, .width = width, .height = height },
            .fpsDen = framerate, .fpsNum = 1, .bufCount = 2, .phyOrExtChn = 0,  
        };

        if (ret = tx_fs.fnCreateChannel(index, &channel))
            return ret;
    }

    if (ret = tx_venc.fnCreateGroup(index))
        return ret;

    return EXIT_SUCCESS;
}

void tx_channel_destroy(char index)
{
    tx_venc.fnDestroyGroup(index);

    tx_fs.fnDestroyChannel(index);
}

int tx_channel_unbind(char index)
{
    int ret;

    tx_fs.fnDisableChannel(index);

    {
        tx_sys_bind source = { .device = TX_SYS_DEV_OSD, .group = _tx_osd_grp, .port = 0 };
        tx_sys_bind dest = { .device = TX_SYS_DEV_ENC, .group = index, .port = 0 };
        tx_sys.fnUnbind(&source, &dest);
    }

    {
        tx_sys_bind source = { .device = TX_SYS_DEV_FS, .group = index, .port = 0 };
        tx_sys_bind dest = { .device = TX_SYS_DEV_OSD, .group = _tx_osd_grp, .port = 0 };
        tx_sys.fnUnbind(&source, &dest);
    }

    return EXIT_SUCCESS;
}

int tx_region_create(int *handle, hal_rect rect)
{
    int ret;

    tx_osd_rgn region, regionCurr;
    tx_osd_grp attrib, attribCurr;

    region.type = TX_OSD_TYPE_PIC;
    region.pixFmt = TX_PIXFMT_RGB555LE;
    region.rect.p0.x = rect.x;
    region.rect.p0.y = rect.y;
    region.rect.p1.x = rect.x + rect.width - 1;
    region.rect.p1.y = rect.y + rect.height - 1;
    region.data.picture = NULL;

    if (tx_osd.fnGetRegionConfig(*handle, &regionCurr)) {
        fprintf(stderr, "[tx_osd] Creating region...\n", _tx_osd_grp);
        if ((ret = tx_osd.fnCreateRegion(&region)) < 0)
            return ret;
        else *handle = ret;
    } else if (regionCurr.rect.p1.y - regionCurr.rect.p0.y != rect.height || 
        regionCurr.rect.p1.x - regionCurr.rect.p0.x != rect.width) {
        fprintf(stderr, "[tx_osd] Parameters are different, recreating "
            "region...\n", _tx_osd_grp);
        if (ret = tx_osd.fnSetRegionConfig(*handle, &region))
            return ret;
    }

    if (tx_osd.fnGetGroupConfig(*handle, _tx_osd_grp, &attribCurr))
        fprintf(stderr, "[tx_osd] Attaching region...\n", _tx_osd_grp);

    memset(&attrib, 0, sizeof(attrib));
    attrib.show = 1;
    attrib.alphaOn = 1;
    attrib.fgAlpha = 128;
    
    tx_osd.fnRegisterRegion(*handle, _tx_osd_grp, &attrib);

    return ret;
}

void tx_region_destroy(int *handle)
{
    tx_osd.fnUnregisterRegion(*handle, _tx_osd_grp);
    *handle = -1;
}

int tx_region_setbitmap(int *handle, hal_bitmap *bitmap)
{
    tx_osd_rgn region;
    tx_osd.fnGetRegionConfig(*handle, &region);
    region.type = TX_OSD_TYPE_PIC;
    region.rect.p1.x = region.rect.p0.x + bitmap->dim.width - 1;
    region.rect.p1.y = region.rect.p0.y + bitmap->dim.height - 1;
    region.pixFmt = TX_PIXFMT_RGB555LE;
    region.data.picture = bitmap->data;    
    return tx_osd.fnSetRegionConfig(*handle, &region);
}

int tx_video_create(char index, hal_vidconfig *config)
{
    int ret;
    tx_venc_chn channel;
    memset(&channel, 0, sizeof(channel));
    channel.gop.mode = TX_VENC_GOPMODE_NORMAL;
    channel.gop.length = config->gop / config->framerate;
    channel.rate.fpsDen = config->framerate;
    channel.rate.fpsNum = 1;
    switch (config->codec) {
        case HAL_VIDCODEC_JPG:
            channel.attrib.profile = TX_VENC_PROF_MJPG;
            break;
        case HAL_VIDCODEC_MJPG:
            channel.attrib.profile = TX_VENC_PROF_MJPG;
            break;
        case HAL_VIDCODEC_H265:
            channel.attrib.profile = TX_VENC_PROF_H265_MAIN;
            break;
        case HAL_VIDCODEC_H264:
            switch (config->profile) {
                case HAL_VIDPROFILE_BASELINE: channel.attrib.profile = TX_VENC_PROF_H264_BASE; break;
                case HAL_VIDPROFILE_MAIN: channel.attrib.profile = TX_VENC_PROF_H264_MAIN; break;
                default: channel.attrib.profile = TX_VENC_PROF_H264_HIGH; break;
            }
            break;
        default: TX_ERROR("This codec is not supported by the hardware!");

    }
    switch (config->mode) {
        case HAL_VIDMODE_CBR:
            channel.rate.mode = TX_VENC_RATEMODE_CBR;
            channel.rate.cbr = (tx_venc_rate_cbr){ .tgtBitrate = MAX(config->bitrate,
                config->maxBitrate) }; break;
        case HAL_VIDMODE_VBR:
            channel.rate.mode = TX_VENC_RATEMODE_VBR;
            channel.rate.vbr = (tx_venc_rate_vbr){ .tgtBitrate = config->bitrate, 
                .maxBitrate = config->maxBitrate }; break;
        case HAL_VIDMODE_QP:
            channel.rate.mode = TX_VENC_RATEMODE_QP;
            channel.rate.qpModeQual = config->maxQual; break;
        case HAL_VIDMODE_AVBR:
            channel.rate.mode = TX_VENC_RATEMODE_AVBR;
            channel.rate.avbr = (tx_venc_rate_xvbr){ .tgtBitrate  = config->bitrate, 
                .maxBitrate = config->maxBitrate }; break;
        default:
            TX_ERROR("Video encoder does not support this mode!");
    }
    channel.attrib.width = config->width;
    channel.attrib.height = config->height;
    channel.attrib.picFmt = (config->codec == HAL_VIDCODEC_JPG || config->codec == HAL_VIDCODEC_MJPG) ?
        TX_VENC_PICFMT_422_8BPP : TX_VENC_PICFMT_420_8BPP;

    if (ret = tx_venc.fnCreateChannel(index, &channel))
        return ret;

    if (ret = tx_venc.fnRegisterChannel(index, index))
        return ret;

    {
        int count = -1;
        if (config->codec != HAL_VIDCODEC_JPG && 
            (ret = tx_venc.fnStartReceiving(index)))
            return ret;
    }
    
    tx_state[index].payload = config->codec;

    return EXIT_SUCCESS;
}

int tx_video_destroy(char index)
{
    int ret;

    tx_state[index].payload = HAL_VIDCODEC_UNSPEC;

    tx_venc.fnStopReceiving(index);

    if (ret = tx_venc.fnUnregisterChannel(index))
        return ret;

    if (ret = tx_venc.fnDestroyChannel(index))
        return ret;

    return EXIT_SUCCESS;
}

int tx_video_destroy_all(void)
{
    int ret;

    for (char i = 0; i < TX_VENC_CHN_NUM; i++)
        if (tx_state[i].enable)
            if (ret = tx_video_destroy(i))
                return ret;

    return EXIT_SUCCESS;
}

void *tx_video_thread(void)
{
    int ret;
    int maxFd = 0;

    for (int i = 0; i < TX_VENC_CHN_NUM; i++) {
        if (!tx_state[i].enable) continue;
        if (!tx_state[i].mainLoop) continue;

        ret = tx_venc.fnGetDescriptor(i);
        if (ret < 0) {
            fprintf(stderr, "[tx_venc] Getting the encoder descriptor failed with %#x!\n", ret);
            return NULL;
        }
        tx_state[i].fileDesc = ret;

        if (maxFd <= tx_state[i].fileDesc)
            maxFd = tx_state[i].fileDesc;
    }

    tx_venc_stat stat;
    tx_venc_strm stream;
    struct timeval timeout;
    fd_set readFds;

    while (keepRunning) {
        FD_ZERO(&readFds);
        for(int i = 0; i < TX_VENC_CHN_NUM; i++) {
            if (!tx_state[i].enable) continue;
            if (!tx_state[i].mainLoop) continue;
            FD_SET(tx_state[i].fileDesc, &readFds);
        }

        timeout.tv_sec = 2;
        timeout.tv_usec = 0;
        ret = select(maxFd + 1, &readFds, NULL, NULL, &timeout);
        if (ret < 0) {
            fprintf(stderr, "[tx_venc] Select operation failed!\n");
            break;
        } else if (ret == 0) {
            fprintf(stderr, "[tx_venc] Main stream loop timed out!\n");
            continue;
        } else {
            for (int i = 0; i < TX_VENC_CHN_NUM; i++) {
                if (!tx_state[i].enable) continue;
                if (!tx_state[i].mainLoop) continue;
                if (FD_ISSET(tx_state[i].fileDesc, &readFds)) {
                    memset(&stream, 0, sizeof(stream));
                    
                    if (ret = tx_venc.fnQuery(i, &stat)) {
                        fprintf(stderr, "[tx_venc] Querying the encoder channel "
                            "%d failed with %#x!\n", i, ret);
                        break;
                    }

                    if (!stat.curPacks) {
                        fprintf(stderr, "[tx_venc] Current frame is empty, skipping it!\n");
                        continue;
                    }

                    stream.packet = (tx_venc_pack*)malloc(
                        sizeof(tx_venc_pack) * stat.curPacks);
                    if (!stream.packet) {
                        fprintf(stderr, "[tx_venc] Memory allocation on channel %d failed!\n", i);
                        break;
                    }
                    stream.count = stat.curPacks;

                    if (ret = tx_venc.fnGetStream(i, &stream, stat.curPacks)) {
                        fprintf(stderr, "[tx_venc] Getting the stream on "
                            "channel %d failed with %#x!\n", i, ret);
                        break;
                    }

                    if (tx_venc_cb) {
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
                        (*tx_venc_cb)(i, &outStrm);
                    }

                    if (ret = tx_venc.fnFreeStream(i, &stream)) {
                        fprintf(stderr, "[tx_venc] Releasing the stream on "
                            "channel %d failed with %#x!\n", i, ret);
                    }
                    free(stream.packet);
                    stream.packet = NULL;
                }
            }
        }
    }
    fprintf(stderr, "[tx_venc] Shutting down encoding thread...\n");
}

void tx_system_deinit(void)
{
    tx_osd.fnDestroyGroup(_tx_osd_grp);

    tx_sys.fnExit();

    tx_isp.fnDisableSensor();
    tx_isp.fnDeleteSensor(&_tx_isp_snr);
    tx_isp.fnExit();
}

int tx_system_init(void)
{
    int ret;

    {
        tx_sys_ver version;
        if (ret = tx_sys.fnGetVersion(&version))
            return ret;
        printf("App built with headers v%s\n", TX_SYS_API);
        puts(version.version);
    }

    {
        const char *sensor = getenv("SENSOR");
        for (char i = 0; i < sizeof(tx_sensors) / sizeof(*tx_sensors); i++) {
            if (strcmp(tx_sensors[i].name, sensor)) continue;
            memcpy(&_tx_isp_snr, &tx_sensors[i], sizeof(tx_isp_snr));
            if (tx_sensors[i].mode == TX_ISP_COMM_I2C)
                memcpy(&_tx_isp_snr.i2c.type, &tx_sensors[i].name, strlen(tx_sensors[i].name));
            else if (tx_sensors[i].mode == TX_ISP_COMM_SPI)
                memcpy(&_tx_isp_snr.spi.alias, &tx_sensors[i].name, strlen(tx_sensors[i].name));
            _tx_snr_dim = tx_dims[i];
            ret = 0;
            break;
        }
        if (ret)
            return EXIT_FAILURE;
    }

    if (ret = tx_isp.fnInit())
        return ret;

    if (ret = tx_isp.fnAddSensor(&_tx_isp_snr))
        return ret;

    if (ret = tx_isp.fnEnableSensor())
        return ret;

    if (ret = tx_sys.fnInit())
        return ret;

    if (ret = tx_osd.fnCreateGroup(_tx_osd_grp))
        return ret;

    return EXIT_SUCCESS;
}
