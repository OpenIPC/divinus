#include "v4_hal.h"

v4_isp_alg      v4_ae_lib = { .libName = "ae_lib" };
v4_aud_impl     v4_aud;
v4_isp_alg      v4_awb_lib = { .libName = "awb_lib" };
v4_config_impl  v4_config;
v4_isp_impl     v4_isp;
v4_isp_drv_impl v4_isp_drv;
v4_rgn_impl     v4_rgn;
v4_sys_impl     v4_sys;
v4_vb_impl      v4_vb;
v4_venc_impl    v4_venc;
v4_vi_impl      v4_vi;
v4_vpss_impl    v4_vpss;

hal_chnstate v4_state[V4_VENC_CHN_NUM] = {0};
int (*v4_venc_cb)(char, hal_vidstream*);

char _v4_aud_chn = 0;
char _v4_aud_dev = 0;
char _v4_isp_chn = 0;
char _v4_isp_dev = 0;
char _v4_venc_dev = 0;
char _v4_vi_chn = 0;
char _v4_vi_dev = 0;
char _v4_vpss_grp = 0;

void v4_hal_deinit(void)
{
    v4_vpss_unload(&v4_vpss);
    v4_vi_unload(&v4_vi);
    v4_venc_unload(&v4_venc);
    v4_vb_unload(&v4_vb);
    v4_rgn_unload(&v4_rgn);
    v4_isp_unload(&v4_isp);
    v4_aud_unload(&v4_aud);
    v4_sys_unload(&v4_sys);
}

int v4_hal_init(void)
{
    int ret;

    if (ret = v4_sys_load(&v4_sys))
        return ret;
    if (ret = v4_aud_load(&v4_aud))
        return ret;
    if (ret = v4_isp_load(&v4_isp))
        return ret;
    if (ret = v4_rgn_load(&v4_rgn))
        return ret;
    if (ret = v4_vb_load(&v4_vb))
        return ret;
    if (ret = v4_venc_load(&v4_venc))
        return ret;
    if (ret = v4_vi_load(&v4_vi))
        return ret;
    if (ret = v4_vpss_load(&v4_vpss))
        return ret;

    return EXIT_SUCCESS;
}

void v4_audio_deinit(void)
{
    v4_aud.fnDisableChannel(_v4_aud_dev, _v4_aud_chn);

    v4_aud.fnDisableDevice(_v4_aud_dev);
}

int v4_audio_init(void)
{
    int ret;

    {
        v4_aud_cnf config;
        config.rate = 48000;
        config.bit = V4_AUD_BIT_16;
        config.intf = V4_AUD_INTF_I2S_SLAVE;
        config.stereoOn = 0;
        config.expandOn = 0;
        config.frmNum = 0;
        config.packNumPerFrm = 0;
        config.chnNum = 0;
        config.syncRxClkOn = 0;
        if (ret = v4_aud.fnSetDeviceConfig(_v4_aud_dev, &config))
            return ret;
    }
    if (ret = v4_aud.fnEnableDevice(_v4_aud_dev))
        return ret;
    
    if (ret = v4_aud.fnEnableChannel(_v4_aud_dev, _v4_aud_chn))
        return ret;
    //if (ret = v4_aud.fnSetVolume(_v4_aud_dev, _v4_aud_chn, 0xF6))
    //    return ret;

    return EXIT_SUCCESS;
}

int v4_channel_bind(char index)
{
    int ret;
    int _v4_vpss_grp = index / V4_VPSS_CHN_NUM;
    int vpss_chn = index - _v4_vpss_grp * V4_VPSS_CHN_NUM;

    if (ret = v4_vpss.fnEnableChannel(_v4_vpss_grp, vpss_chn))
        return ret;

    {
        v4_sys_bind source = { .module = V4_SYS_MOD_VPSS, 
            .device = _v4_vpss_grp, .channel = vpss_chn };
        v4_sys_bind dest = { .module = V4_SYS_MOD_VENC,
            .device = _v4_venc_dev, .channel = index };
        if (ret = v4_sys.fnBind(&source, &dest))
            return ret;
    }

    return EXIT_SUCCESS;
}

int v4_channel_create(char index, short width, short height, char framerate)
{
    int ret;
    int _v4_vpss_grp = index / V4_VPSS_CHN_NUM;
    int vpss_chn = index - _v4_vpss_grp * V4_VPSS_CHN_NUM;

    {
        v4_vpss_chn channel;
        memset(&channel, 0, sizeof(channel));
        channel.srcFps = framerate;
        channel.dstFps = framerate;
        if (ret = v4_vpss.fnSetChannelConfig(_v4_vpss_grp, vpss_chn, &channel))
            return ret;
    }

    return EXIT_SUCCESS;
}

int v4_channel_unbind(char index)
{
    int ret;
    int _v4_vpss_grp = index / V4_VPSS_CHN_NUM;
    int vpss_chn = index - _v4_vpss_grp * V4_VPSS_CHN_NUM;

    if (ret = v4_vpss.fnDisableChannel(_v4_vpss_grp, vpss_chn))
        return ret;

    {
        v4_sys_bind source = { .module = V4_SYS_MOD_VPSS, 
            .device = _v4_vpss_grp, .channel = vpss_chn };
        v4_sys_bind dest = { .module = V4_SYS_MOD_VENC,
            .device = _v4_venc_dev, .channel = index };
        if (ret = v4_sys.fnUnbind(&source, &dest))
            return ret;
    }

    return EXIT_SUCCESS;
}

void *v4_image_thread(void)
{
    int ret;

    if (ret = v4_isp.fnRun(_v4_isp_dev))
        fprintf(stderr, "[v4_isp] Operation failed with %#x!\n", ret);
    fprintf(stderr, "[v4_isp] Shutting down ISP thread...\n");
}

int v4_pipeline_create(char mirror, char flip)
{
    int ret;

    if (ret = v4_vi.fnSetDeviceConfig(_v4_isp_dev, &v4_config.videv))
        return ret;
    if (ret = v4_vi.fnEnableDevice(_v4_isp_dev))
        return ret;

    if (ret = v4_vi.fnSetChannelConfig(_v4_isp_chn, &v4_config.vichn))
        return ret;
    if (ret = v4_vi.fnEnableChannel(_v4_isp_chn))
        return ret;

    {
        v4_vpss_grp group;
        group.imgEnhOn = 0;
        group.dciOn = 0;
        group.noiseRedOn = 1;
        group.histOn = 0;
        group.deintMode = 1;
        if (ret = v4_vpss.fnCreateGroup(_v4_vpss_grp, &group))
            return ret;
    }
    if (ret = v4_vpss.fnStartGroup(_v4_vpss_grp))
        return ret;

    {
        v4_sys_bind source = { .module = V4_SYS_MOD_VIU, 
            .device = _v4_vi_dev, .channel = _v4_vi_chn };
        v4_sys_bind dest = { .module = V4_SYS_MOD_VPSS, 
            .device = _v4_vpss_grp, .channel = 0 };
        if (ret = v4_sys.fnBind(&source, &dest))
            return ret;
    }


    return EXIT_SUCCESS;
}

void v4_pipeline_destroy(void)
{
    for (char grp = 0; grp < V4_VPSS_GRP_NUM; grp++)
    {
        for (char chn = 0; chn < V4_VPSS_CHN_NUM; chn++)
            v4_vpss.fnDisableChannel(grp, chn);

        {
            v4_sys_bind source = { .module = V4_SYS_MOD_VIU, 
                .device = _v4_vi_dev, .channel = _v4_vi_chn };
            v4_sys_bind dest = { .module = V4_SYS_MOD_VPSS,
                .device = grp, .channel = 0 };
            v4_sys.fnUnbind(&source, &dest);
        }

        v4_vpss.fnStopGroup(grp);
        v4_vpss.fnDestroyGroup(grp);
    }

    v4_vi.fnDisableChannel(_v4_vi_chn);
    v4_vi.fnDisableDevice(_v4_vi_dev);

    v4_isp.fnExit(_v4_isp_dev);
}

int v4_region_create(char handle, hal_rect rect)
{
    int ret;

    v4_sys_bind channel = { .module = V4_SYS_MOD_VENC,
        .device = _v4_venc_dev, .channel = 0 };
    v4_rgn_cnf region, regionCurr;
    v4_rgn_chn attrib, attribCurr;

    region.type = V4_RGN_TYPE_OVERLAY;
    region.overlay.pixFmt = V4_PIXFMT_ABGR1555;
    region.overlay.size.width = rect.width;
    region.overlay.size.height = rect.height;
    if (ret = v4_rgn.fnGetRegionConfig(handle, &regionCurr)) {
        fprintf(stderr, "[v4_rgn] Creating region %d...\n", handle);
        if (ret = v4_rgn.fnCreateRegion(handle, &region))
            return ret;
    } else if (regionCurr.overlay.size.height != region.overlay.size.height || 
        regionCurr.overlay.size.width != region.overlay.size.width) {
        fprintf(stderr, "[v4_rgn] Parameters are different, recreating "
            "region %d...\n", handle);
        v4_rgn.fnDetachChannel(handle, &channel);
        v4_rgn.fnDestroyRegion(handle);
        if (ret = v4_rgn.fnCreateRegion(handle, &region))
            return ret;
    }

    if (v4_rgn.fnGetChannelConfig(handle, &channel, &attribCurr))
        fprintf(stderr, "[v4_rgn] Attaching region %d...\n", handle);
    else if (attribCurr.overlay.point.x != rect.x || attribCurr.overlay.point.x != rect.y) {
        fprintf(stderr, "[v4_rgn] Position has changed, reattaching "
            "region %d...\n", handle);
        v4_rgn.fnDetachChannel(handle, &channel);
    }

    memset(&attrib, 0, sizeof(attrib));
    attrib.show = 1;
    attrib.type = V4_RGN_TYPE_OVERLAY;
    attrib.overlay.bgAlpha = 0;
    attrib.overlay.fgAlpha = 128;
    attrib.overlay.point.x = rect.x;
    attrib.overlay.point.y = rect.y;
    attrib.overlay.layer = 7;

    v4_rgn.fnAttachChannel(handle, &channel, &attrib);

    return ret;
}

void v4_region_destroy(char handle)
{
    v4_sys_bind channel = { .module = V4_SYS_MOD_VENC,
        .device = _v4_venc_dev, .channel = 0 };
    
    v4_rgn.fnDetachChannel(handle, &channel);
    v4_rgn.fnDestroyRegion(handle);
}

int v4_region_setbitmap(int handle, hal_bitmap *bitmap)
{
    v4_rgn_bmp nativeBmp = { .data = bitmap->data, .pixFmt = V4_PIXFMT_ARGB1555,
        .size.height = bitmap->dim.height, .size.width = bitmap->dim.width };

    return v4_rgn.fnSetBitmap(handle, &nativeBmp);
}

int v4_sensor_config(void) {
    v4_snr_dev config;
    config.device = 0;
    config.input = v4_config.input_mode;
    config.dataRate2X = 0;
    config.rect = v4_config.vichn.capt;

    if (config.input == V4_SNR_INPUT_MIPI)
        memcpy(&config.mipi, &v4_config.mipi, sizeof(v4_snr_mipi));
    else if (config.input == V4_SNR_INPUT_LVDS)
        memcpy(&config.lvds, &v4_config.lvds, sizeof(v4_snr_lvds));

    int fd = open(V4_SNR_ENDPOINT, O_RDWR);
    if (fd < 0)
        V4_ERROR("Opening imaging device has failed!\n");

    if (ioctl(fd, _IOW(V4_SNR_IOC_MAGIC, V4_SNR_CMD_RST_INTF, unsigned int), &config.device))
        V4_ERROR("Resetting imaging device has failed!\n");
    
    if (ioctl(fd, _IOW(V4_SNR_IOC_MAGIC, V4_SNR_CMD_RST_SENS, unsigned int), &config.device))
        V4_ERROR("Resetting imaging sensor has failed!\n");

    if (ioctl(fd, _IOW(V4_SNR_IOC_MAGIC, V4_SNR_CMD_CONF_DEV, v4_snr_dev), &config) && close(fd))
        V4_ERROR("Configuring imaging device has failed!\n");

    usleep(10000);

    if (ioctl(fd, _IOW(V4_SNR_IOC_MAGIC, V4_SNR_CMD_UNRST_INTF, unsigned int), &config.device))
        V4_ERROR("Unresetting imaging device has failed!\n");

    if (ioctl(fd, _IOW(V4_SNR_IOC_MAGIC, V4_SNR_CMD_UNRST_SENS, unsigned int), &config.device))
        V4_ERROR("Unresetting imaging sensor has failed!\n");

    close(fd);

    return EXIT_SUCCESS;
}

int v4_sensor_cb_empty(void) { return EXIT_SUCCESS; }

void v4_sensor_deinit(void)
{
    dlclose(v4_isp_drv.handle);
    v4_isp_drv.handle = NULL;
}

int v4_sensor_init(char *name, char *obj)
{
    char path[128];
    char* dirs[] = {"%s", "./%s", "/usr/lib/sensors/%s"};
    char **dir = dirs;

    while (*dir++) {
        sprintf(path, *dir, name);
        if (v4_isp_drv.handle = dlopen(path, RTLD_LAZY | RTLD_GLOBAL))
            break;
    } if (!v4_isp_drv.handle)
        V4_ERROR("Failed to load the sensor driver");
    
    if (!(v4_isp_drv.obj = (v4_isp_obj*)dlsym(v4_isp_drv.handle, obj)))
        V4_ERROR("Failed to connect the sensor object");

    return EXIT_SUCCESS;
}

int v4_video_create(char index, hal_vidconfig *config)
{
    int ret;
    v4_venc_chn channel;
    v4_venc_attr_h26x *attrib;

    if (config->codec == HAL_VIDCODEC_JPG) {
        channel.attrib.codec = V4_VENC_CODEC_JPEGE;
        channel.attrib.jpg.maxWidth = ALIGN_BACK(config->width, 16);
        channel.attrib.jpg.maxHeight = ALIGN_BACK(config->height, 16);
        channel.attrib.jpg.bufSize = 
            ALIGN_BACK(config->height, 16) * ALIGN_BACK(config->width, 16);
        channel.attrib.jpg.byFrame = 1;
        channel.attrib.jpg.width = config->width;
        channel.attrib.jpg.height = config->height;
        channel.attrib.jpg.dcfThumbs = 0;
    } else if (config->codec == HAL_VIDCODEC_MJPG) {
        channel.attrib.codec = V4_VENC_CODEC_MJPG;
        channel.attrib.mjpg.maxWidth = ALIGN_BACK(config->width, 16);
        channel.attrib.mjpg.maxHeight = ALIGN_BACK(config->height, 16);
        channel.attrib.mjpg.bufSize = 
            ALIGN_BACK(config->height, 16) * ALIGN_BACK(config->width, 16);
        channel.attrib.mjpg.byFrame = 1;
        channel.attrib.mjpg.width = config->width;
        channel.attrib.mjpg.height = config->height;
        switch (config->mode) {
            case HAL_VIDMODE_CBR:
                channel.rate.mode = V4_VENC_RATEMODE_MJPGCBR;
                channel.rate.mjpgCbr = (v4_venc_rate_mjpgcbr){ .statTime = 1, .srcFps = config->framerate,
                    .dstFps = config->framerate, .bitrate = config->bitrate, .avgLvl = 0 }; break;
            case HAL_VIDMODE_VBR:
                channel.rate.mode = V4_VENC_RATEMODE_MJPGVBR;
                channel.rate.mjpgVbr = (v4_venc_rate_mjpgvbr){ .statTime = 1, .srcFps = config->framerate,
                    .dstFps = config->framerate , .maxBitrate = MAX(config->bitrate, config->maxBitrate), 
                    .maxQual = config->maxQual, .minQual = config->maxQual }; break;
            case HAL_VIDMODE_QP:
                channel.rate.mode = V4_VENC_RATEMODE_MJPGQP;
                channel.rate.mjpgQp = (v4_venc_rate_mjpgqp){ .srcFps = config->framerate,
                    .dstFps = config->framerate, .quality = config->maxQual }; break;
            default:
                V4_ERROR("MJPEG encoder can only support CBR, VBR or fixed QP modes!");
        }
        goto attach;
    } else if (config->codec == HAL_VIDCODEC_H265) {
        channel.attrib.codec = V4_VENC_CODEC_H265;
        attrib = &channel.attrib.h265;
        switch (config->mode) {
            case HAL_VIDMODE_CBR:
                channel.rate.mode = V4_VENC_RATEMODE_H265CBR;
                channel.rate.h265Cbr = (v4_venc_rate_h26xcbr){ .gop = config->gop,
                    .statTime = 1, .srcFps = config->framerate, .dstFps = config->framerate,
                    .bitrate = config->bitrate, .avgLvl = 1 }; break;
            case HAL_VIDMODE_VBR:
                channel.rate.mode = V4_VENC_RATEMODE_H265VBR;
                channel.rate.h265Vbr = (v4_venc_rate_h26xvbr){ .gop = config->gop,
                    .statTime = 1, .srcFps = config->framerate, .dstFps = config->framerate, 
                    .maxBitrate = MAX(config->bitrate, config->maxBitrate), .maxQual = config->maxQual,
                    .minQual = config->minQual, .minIQual = config->minQual }; break;
            case HAL_VIDMODE_QP:
                channel.rate.mode = V4_VENC_RATEMODE_H265QP;
                channel.rate.h265Qp = (v4_venc_rate_h26xqp){ .gop = config->gop,
                    .srcFps = config->framerate, .dstFps = config->framerate, .interQual = config->maxQual, 
                    .predQual = config->minQual, .bipredQual = config->minQual }; break;
            case HAL_VIDMODE_AVBR:
                channel.rate.mode = V4_VENC_RATEMODE_H265AVBR;
                channel.rate.h265Avbr = (v4_venc_rate_h26xxvbr){ .gop = config->gop,
                    .statTime = 1, .srcFps = config->framerate, .dstFps = config->framerate,
                    .bitrate = config->bitrate }; break;
            default:
                V4_ERROR("H.265 encoder does not support this mode!");
        }
    } else if (config->codec == HAL_VIDCODEC_H264) {
        channel.attrib.codec = V4_VENC_CODEC_H264;
        attrib = &channel.attrib.h264;
        switch (config->mode) {
            case HAL_VIDMODE_CBR:
                channel.rate.mode = V4_VENC_RATEMODE_H264CBR;
                channel.rate.h264Cbr = (v4_venc_rate_h26xcbr){ .gop = config->gop,
                    .statTime = 1, .srcFps = config->framerate, .dstFps = config->framerate,
                    .bitrate = config->bitrate, .avgLvl = 1 }; break;
            case HAL_VIDMODE_VBR:
                channel.rate.mode = V4_VENC_RATEMODE_H264VBR;
                channel.rate.h264Vbr = (v4_venc_rate_h26xvbr){ .gop = config->gop,
                    .statTime = 1, .srcFps = config->framerate, .dstFps = config->framerate, 
                    .maxBitrate = MAX(config->bitrate, config->maxBitrate), .maxQual = config->maxQual,
                    .minQual = config->minQual, .minIQual = config->minQual }; break;
            case HAL_VIDMODE_QP:
                channel.rate.mode = V4_VENC_RATEMODE_H264QP;
                channel.rate.h264Qp = (v4_venc_rate_h26xqp){ .gop = config->gop,
                    .srcFps = config->framerate, .dstFps = config->framerate, .interQual = config->maxQual, 
                    .predQual = config->minQual, .bipredQual = config->minQual }; break;
            case HAL_VIDMODE_AVBR:
                channel.rate.mode = V4_VENC_RATEMODE_H264AVBR;
                channel.rate.h264Avbr = (v4_venc_rate_h26xxvbr){ .gop = config->gop,
                    .statTime = 1, .srcFps = config->framerate, .dstFps = config->framerate,
                    .bitrate = config->bitrate }; break;
            default:
                V4_ERROR("H.264 encoder does not support this mode!");
        }
    } else V4_ERROR("This codec is not supported by the hardware!");
    attrib->maxWidth = ALIGN_BACK(config->width, 16);
    attrib->maxHeight = ALIGN_BACK(config->height, 16);
    attrib->bufSize = ALIGN_BACK(config->height, 16) * ALIGN_BACK(config->width, 16);
    attrib->profile = config->profile;
    attrib->byFrame = 1;
    attrib->width = config->width;
    attrib->height = config->height;
attach:
    if (ret = v4_venc.fnCreateChannel(index, &channel))
        return ret;

    {
        int count = 0;
        if (config->codec != HAL_VIDCODEC_JPG && 
            (ret = v4_venc.fnStartReceivingEx(index, &count)))
            return ret;
    }
    
    v4_state[index].payload = config->codec;

    return EXIT_SUCCESS;
}

int v4_video_destroy(char index)
{
    int ret;
    int _v4_vpss_grp = index / V4_VPSS_CHN_NUM;
    int vpss_chn = index - _v4_vpss_grp * V4_VPSS_CHN_NUM;

    v4_state[index].payload = HAL_VIDCODEC_UNSPEC;

    if (ret = v4_venc.fnStopReceiving(index))
        return ret;

    {
        v4_sys_bind source = { .module = V4_SYS_MOD_VPSS, 
            .device = _v4_vpss_grp, .channel = vpss_chn };
        v4_sys_bind dest = { .module = V4_SYS_MOD_VENC,
            .device = _v4_venc_dev, .channel = index };
        if (ret = v4_sys.fnUnbind(&source, &dest))
            return ret;
    }

    if (ret = v4_venc.fnDestroyChannel(index))
        return ret;
    
    if (ret = v4_vpss.fnDisableChannel(_v4_vpss_grp, vpss_chn))
        return ret;

    return EXIT_SUCCESS;
}
    
int v4_video_destroy_all(void)
{
    int ret;

    for (char i = 0; i < V4_VENC_CHN_NUM; i++)
        if (v4_state[i].enable)
            if (ret = v4_video_destroy(i))
                return ret;

    return EXIT_SUCCESS;
}

int v4_video_snapshot_grab(char index, short width, short height, 
    char quality, hal_jpegdata *jpeg)
{
    int ret;

    if (ret = v4_channel_bind(index)) {
        fprintf(stderr, "[v4_venc] Binding the encoder channel "
            "%d failed with %#x!\n", index, ret);
        goto abort;
    }
    return ret;

    v4_venc_jpg param;
    memset(&param, 0, sizeof(param));
    if (ret = v4_venc.fnGetJpegParam(index, &param)) {
        fprintf(stderr, "[v4_venc] Reading the JPEG settings "
            "%d failed with %#x!\n", index, ret);
        goto abort;
    }
    return ret;
        return ret;
    param.quality = quality;
    if (ret = v4_venc.fnSetJpegParam(index, &param)) {
        fprintf(stderr, "[v4_venc] Writing the JPEG settings "
            "%d failed with %#x!\n", index, ret);
        goto abort;
    }

    unsigned int count = 1;
    if (v4_venc.fnStartReceivingEx(index, &count)) {
        fprintf(stderr, "[v4_venc] Requesting one frame "
            "%d failed with %#x!\n", index, ret);
        goto abort;
    }

    int fd = v4_venc.fnGetDescriptor(index);

    struct timeval timeout = { .tv_sec = 2, .tv_usec = 0 };
    fd_set readFds;
    FD_ZERO(&readFds);
    FD_SET(fd, &readFds);
    ret = select(fd + 1, &readFds, NULL, NULL, &timeout);
    if (ret < 0) {
        fprintf(stderr, "[v4_venc] Select operation failed!\n");
        goto abort;
    } else if (ret == 0) {
        fprintf(stderr, "[v4_venc] Capture stream timed out!\n");
        goto abort;
    }

    if (FD_ISSET(fd, &readFds)) {
        v4_venc_stat stat;
        if (v4_venc.fnQuery(index, &stat)) {
            fprintf(stderr, "[v4_venc] Querying the encoder channel "
                "%d failed with %#x!\n", index, ret);
            goto abort;
        }

        if (!stat.curPacks) {
            fprintf(stderr, "[v4_venc] Current frame is empty, skipping it!\n");
            goto abort;
        }

        v4_venc_strm strm;
        memset(&strm, 0, sizeof(strm));
        strm.packet = (v4_venc_pack*)malloc(sizeof(v4_venc_pack) * stat.curPacks);
        if (!strm.packet) {
            fprintf(stderr, "[v4_venc] Memory allocation on channel %d failed!\n", index);
            goto abort;
        }
        strm.count = stat.curPacks;

        if (ret = v4_venc.fnGetStream(index, &strm, stat.curPacks)) {
            fprintf(stderr, "[v4_venc] Getting the stream on "
                "channel %d failed with %#x!\n", index, ret);
            free(strm.packet);
            strm.packet = NULL;
            goto abort;
        }

        {
            jpeg->jpegSize = 0;
            for (unsigned int i = 0; i < strm.count; i++) {
                v4_venc_pack *pack = &strm.packet[i];
                unsigned int packLen = pack->length - pack->offset;
                unsigned char *packData = pack->data + pack->offset;

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
        v4_venc.fnFreeStream(index, &strm);
    }

    v4_venc.fnFreeDescriptor(index);

    v4_venc.fnStopReceiving(index);

    v4_channel_unbind(index);

    return ret;
}

void *v4_video_thread(void)
{
    int ret;
    int maxFd = 0;

    for (int i = 0; i < V4_VENC_CHN_NUM; i++) {
        if (!v4_state[i].enable) continue;
        if (!v4_state[i].mainLoop) continue;

        ret = v4_venc.fnGetDescriptor(i);
        if (ret < 0) {
            fprintf(stderr, "[v4_venc] Getting the encoder descriptor failed with %#x!\n", ret);
            return NULL;
        }
        v4_state[i].fileDesc = ret;

        if (maxFd <= v4_state[i].fileDesc)
            maxFd = v4_state[i].fileDesc;
    }

    v4_venc_stat stat;
    v4_venc_strm stream;
    struct timeval timeout;
    fd_set readFds;

    while (keepRunning) {
        FD_ZERO(&readFds);
        for(int i = 0; i < V4_VENC_CHN_NUM; i++) {
            if (!v4_state[i].enable) continue;
            if (!v4_state[i].mainLoop) continue;
            FD_SET(v4_state[i].fileDesc, &readFds);
        }

        timeout.tv_sec = 2;
        timeout.tv_usec = 0;
        ret = select(maxFd + 1, &readFds, NULL, NULL, &timeout);
        if (ret < 0) {
            fprintf(stderr, "[v4_venc] Select operation failed!\n");
            break;
        } else if (ret == 0) {
            fprintf(stderr, "[v4_venc] Main stream loop timed out!\n");
            continue;
        } else {
            for (int i = 0; i < V4_VENC_CHN_NUM; i++) {
                if (!v4_state[i].enable) continue;
                if (!v4_state[i].mainLoop) continue;
                if (FD_ISSET(v4_state[i].fileDesc, &readFds)) {
                    memset(&stream, 0, sizeof(stream));
                    
                    if (ret = v4_venc.fnQuery(i, &stat)) {
                        fprintf(stderr, "[v4_venc] Querying the encoder channel "
                            "%d failed with %#x!\n", i, ret);
                        break;
                    }

                    if (!stat.curPacks) {
                        fprintf(stderr, "[v4_venc] Current frame is empty, skipping it!\n");
                        continue;
                    }

                    stream.packet = (v4_venc_pack*)malloc(
                        sizeof(v4_venc_pack) * stat.curPacks);
                    if (!stream.packet) {
                        fprintf(stderr, "[v4_venc] Memory allocation on channel %d failed!\n", i);
                        break;
                    }
                    stream.count = stat.curPacks;

                    if (ret = v4_venc.fnGetStream(i, &stream, stat.curPacks)) {
                        fprintf(stderr, "[v4_venc] Getting the stream on "
                            "channel %d failed with %#x!\n", i, ret);
                        break;
                    }

                    if (v4_venc_cb) {
                        hal_vidstream outStrm;
                        hal_vidpack outPack[stat.curPacks];
                        outStrm.count = stream.count;
                        outStrm.seq = stream.sequence;
                        for (int j = 0; j < stat.curPacks; j++) {
                            outPack[j].data = stream.packet[j].data;
                            outPack[j].length = stream.packet[j].length;
                            outPack[j].offset = stream.packet[j].offset;
                        }
                        outStrm.pack = outPack;
                        (*v4_venc_cb)(i, &outStrm);
                    }

                    if (ret = v4_venc.fnFreeStream(i, &stream)) {
                        fprintf(stderr, "[v4_venc] Releasing the stream on "
                            "channel %d failed with %#x!\n", i, ret);
                    }
                    free(stream.packet);
                    stream.packet = NULL;
                }
            }
        }
    }
    fprintf(stderr, "[v4_venc] Shutting down encoding thread...\n");
}

int v4_system_calculate_block(short width, short height, v4_common_pixfmt pixFmt,
    unsigned int alignWidth)
{
    if (alignWidth & 0b0001111) {
        fprintf(stderr, "[v4_sys] Alignment width (%d) "
            "is invalid!\n", alignWidth);
        return -1;
    }

    unsigned int bufSize = CEILING_2_POWER(width, alignWidth) *
        CEILING_2_POWER(height, alignWidth) *
        (pixFmt == V4_PIXFMT_YUV422SP ? 2 : 1.5);
    unsigned int headSize;
    if (pixFmt == V4_PIXFMT_YUV422SP || pixFmt >= V4_PIXFMT_RGB_BAYER_8BPP)
        headSize = 16 * height * 2;
    else if (pixFmt == V4_PIXFMT_YUV420SP)
        headSize = (16 * height * 3) >> 1;
    return bufSize + headSize;
}

void v4_system_deinit(void)
{
    v4_isp.fnUnregisterAWB(_v4_isp_dev, &v4_ae_lib);
    v4_isp.fnUnregisterAE(_v4_isp_dev, &v4_awb_lib);

    v4_isp_drv.obj->pfnUnRegisterCallback(_v4_isp_dev, &v4_ae_lib, &v4_awb_lib);

    v4_sys.fnExit();
    v4_vb.fnExit();

    v4_sensor_deinit();
}

int v4_system_init(unsigned int alignWidth, unsigned int blockCnt, 
    unsigned int poolCnt, char *snrConfig)
{
    int ret;

    {
        v4_sys_ver version;
        v4_sys.fnGetVersion(&version);
        printf("App built with headers v%s\n", V4_SYS_API);
        printf("%s\n", version.version);
    }

    if (v4_parse_sensor_config(snrConfig, &v4_config) != CONFIG_OK)
        V4_ERROR("Can't load sensor config\n");

    if (ret = v4_sensor_init(v4_config.dll_file, v4_config.sensor_type))
        return ret;

    v4_sys.fnExit();
    v4_vb.fnExit();

    {
        v4_vb_pool pool = {
            .count = poolCnt,
            .comm = {
                {
                    .blockSize = v4_system_calculate_block(
                        v4_config.vichn.capt.width,
                        v4_config.vichn.capt.height,
                        v4_config.vichn.pixFmt,
                        alignWidth),
                    .blockCnt = blockCnt
                }
            }
        };
        if (ret = v4_vb.fnConfigPool(&pool))
            return ret;
    }
    {
        v4_vb_supl supl = V4_VB_JPEG_MASK;
        if (ret = v4_vb.fnConfigSupplement(&supl))
            return ret;
    }
    if (ret = v4_vb.fnInit())
        return ret;

    if (ret = v4_sys.fnSetAlignment(&alignWidth))
        return ret;
    if (ret = v4_sys.fnInit())
        return ret;

    if (ret = v4_sensor_config())
        return ret;

    if (!(v4_isp_drv.obj->pfnRegisterCallback(_v4_isp_dev, &v4_ae_lib, &v4_awb_lib)))
        return ret;

    if (ret = v4_isp.fnRegisterAE(_v4_isp_dev, &v4_ae_lib))
        return ret;
    if (ret = v4_isp.fnRegisterAWB(_v4_isp_dev, &v4_awb_lib))
        return ret;
    if (ret = v4_isp.fnMemInit(_v4_isp_dev))
        return ret;
    if (ret = v4_isp.fnSetDeviceConfig(_v4_isp_dev, &v4_config.isp))
        return ret;
    if (ret = v4_isp.fnInit(_v4_isp_dev))
        return ret;

    return EXIT_SUCCESS;
}