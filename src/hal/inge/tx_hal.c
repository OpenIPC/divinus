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

char _tx_aud_chn = 0;
char _tx_aud_dev = 0;
char _tx_fs_chn = 0;
char _tx_osd_grp = 0;
char _tx_venc_grp = 0;

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

int tx_pipeline_create(short width, short height, char framerate, char regionOn)
{
    int ret;

    {
        tx_fs_chn channel = {
            .dest = { .width = width, .height = height },
            .fpsDen = framerate, .fpsNum = 1,
            .pixFmt = TX_PIXFMT_NV12
        };

        if (ret = tx_fs.fnCreateChannel(_tx_fs_chn, &channel))
            return ret;
    }

    if (ret = tx_venc.fnCreateGroup(_tx_venc_grp))
        return ret;

    if (regionOn) {
        {
            tx_sys_bind source = { .device = TX_SYS_DEV_FS, .group = 0, .port = 0 };
            tx_sys_bind dest = { .device = TX_SYS_DEV_OSD, .group = 0, .port = 0 };
            if (ret = tx_sys.fnBind(&source, &dest))
                return ret;
        }

        {
            tx_sys_bind source = { .device = TX_SYS_DEV_OSD, .group = 0, .port = 0 };
            tx_sys_bind dest = { .device = TX_SYS_DEV_ENC, .group = 0, .port = 0 };
            if (ret = tx_sys.fnBind(&source, &dest))
                return ret;
        }

    } else {
        tx_sys_bind source = { .device = TX_SYS_DEV_FS, .group = 0, .port = 0 };
        tx_sys_bind dest = { .device = TX_SYS_DEV_ENC, .group = 0, .port = 0 };   
        if (ret = tx_sys.fnBind(&source, &dest))
            return ret;    
    }

    if (ret = tx_fs.fnEnableChannel(_tx_fs_chn))
        return ret;

    return EXIT_SUCCESS;
}

void tx_pipeline_destroy(char regionOn)
{
    tx_fs.fnDisableChannel(_tx_fs_chn);

    if (regionOn) {
        {
            tx_sys_bind source = { .device = TX_SYS_DEV_OSD, .group = 0, .port = 0 };
            tx_sys_bind dest = { .device = TX_SYS_DEV_ENC, .group = 0, .port = 0 };
            tx_sys.fnUnbind(&source, &dest);
        }

        {
            tx_sys_bind source = { .device = TX_SYS_DEV_FS, .group = 0, .port = 0 };
            tx_sys_bind dest = { .device = TX_SYS_DEV_OSD, .group = 0, .port = 0 };
            tx_sys.fnUnbind(&source, &dest);
        }
    } else {
        tx_sys_bind source = { .device = TX_SYS_DEV_FS, .group = 0, .port = 0 };
        tx_sys_bind dest = { .device = TX_SYS_DEV_ENC, .group = 0, .port = 0 };   
        tx_sys.fnUnbind(&source, &dest);    
    }

    tx_venc.fnDestroyGroup(_tx_venc_grp);

    tx_fs.fnDestroyChannel(_tx_fs_chn);
}

int tx_region_create(int *handle, char group, hal_rect rect)
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
        fprintf(stderr, "[tx_osd] Creating region %d...\n", group);
        if ((ret = tx_osd.fnCreateRegion(&region)) < 0)
            return ret;
        else *handle = ret;
    } else if (regionCurr.rect.p1.y - regionCurr.rect.p0.y != rect.height || 
        regionCurr.rect.p1.x - regionCurr.rect.p0.x != rect.width) {
        fprintf(stderr, "[tx_osd] Parameters are different, recreating "
            "region %d...\n", group);
        if (ret = tx_osd.fnSetRegionConfig(*handle, &region))
            return ret;
    }

    if (tx_osd.fnGetGroupConfig(*handle, group, &attribCurr))
        fprintf(stderr, "[tx_osd] Attaching region %d...\n", group);

    memset(&attrib, 0, sizeof(attrib));
    attrib.show = 1;
    attrib.alphaOn = 1;
    attrib.fgAlpha = 255;

    tx_osd.fnCreateGroup(group);
    tx_osd.fnRegisterRegion(*handle, group, &attrib);
    tx_osd.fnStartGroup(group);

    return ret;
}

void tx_region_destroy(int *handle, char group)
{
    tx_osd.fnStopGroup(group);
    tx_osd.fnUnregisterRegion(*handle, group);
    tx_osd.fnDestroyGroup(group);
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
            channel.rate.cbr = (tx_venc_rate_cbr){ .tgtBitrate = config->bitrate }; break;
        case HAL_VIDMODE_VBR:
            channel.rate.mode = TX_VENC_RATEMODE_VBR;
            channel.rate.vbr = (tx_venc_rate_vbr){ .maxBitrate = MAX(config->bitrate, 
            config->maxBitrate) }; break;
        case HAL_VIDMODE_QP:
            channel.rate.mode = TX_VENC_RATEMODE_QP;
            channel.rate.qpModeQual = config->maxQual; break;
        case HAL_VIDMODE_AVBR:
            channel.rate.mode = TX_VENC_RATEMODE_AVBR;
            channel.rate.avbr = (tx_venc_rate_xvbr){ .maxBitrate = config->bitrate }; break;
        default:
            TX_ERROR("Video encoder does not support this mode!");
    }
    channel.attrib.width = config->width;
    channel.attrib.height = config->height;
    channel.attrib.picFmt = (config->codec == HAL_VIDCODEC_JPG || config->codec == HAL_VIDCODEC_MJPG) ?
        TX_VENC_PICFMT_422_8BPP : TX_VENC_PICFMT_420_8BPP;

    if (ret = tx_venc.fnCreateChannel(index, &channel))
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

    if (ret = tx_venc.fnStopReceiving(index))
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

}

void tx_system_deinit(void)
{
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

    return EXIT_SUCCESS;
}
