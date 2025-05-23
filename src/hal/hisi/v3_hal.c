#if defined(__arm__) && !defined(__ARM_PCS_VFP)

#include "v3_hal.h"

v3_isp_alg      v3_ae_lib = { .id = 0, .libName = "hisi_ae_lib" };
v3_aud_impl     v3_aud;
v3_isp_alg      v3_awb_lib = { .id = 0, .libName = "hisi_awb_lib" };
v3_config_impl  v3_config;
v3_isp_impl     v3_isp;
v3_snr_drv_impl v3_snr_drv;
v3_rgn_impl     v3_rgn;
v3_sys_impl     v3_sys;
v3_vb_impl      v3_vb;
v3_venc_impl    v3_venc;
v3_vi_impl      v3_vi;
v3_vpss_impl    v3_vpss;

hal_chnstate v3_state[V3_VENC_CHN_NUM] = {0};
int (*v3_aud_cb)(hal_audframe*);
int (*v3_vid_cb)(char, hal_vidstream*);

char _v3_aud_chn = 0;
char _v3_aud_dev = 0;
char _v3_isp_chn = 0;
char _v3_isp_dev = 0;
char _v3_venc_dev = 0;
char _v3_vi_chn = 0;
char _v3_vi_dev = 0;
char _v3_vpss_chn = 0;
char _v3_vpss_grp = 0;

void v3_hal_deinit(void)
{
    v3_vpss_unload(&v3_vpss);
    v3_vi_unload(&v3_vi);
    v3_venc_unload(&v3_venc);
    v3_vb_unload(&v3_vb);
    v3_rgn_unload(&v3_rgn);
    v3_isp_unload(&v3_isp);
    v3_aud_unload(&v3_aud);
    v3_sys_unload(&v3_sys);
}

int v3_hal_init(void)
{
    int ret;

    if (ret = v3_sys_load(&v3_sys))
        return ret;
    if (ret = v3_aud_load(&v3_aud))
        return ret;
    if (ret = v3_isp_load(&v3_isp))
        return ret;
    if (ret = v3_rgn_load(&v3_rgn))
        return ret;
    if (ret = v3_vb_load(&v3_vb))
        return ret;
    if (ret = v3_venc_load(&v3_venc))
        return ret;
    if (ret = v3_vi_load(&v3_vi))
        return ret;
    if (ret = v3_vpss_load(&v3_vpss))
        return ret;

    return EXIT_SUCCESS;
}

void v3_audio_deinit(void)
{
    v3_aud.fnDisableChannel(_v3_aud_dev, _v3_aud_chn);

    v3_aud.fnDisableDevice(_v3_aud_dev);
}

int v3_audio_init(int samplerate)
{
    int ret;

    {
        v3_aud_cnf config;
        config.rate = samplerate;
        config.bit = V3_AUD_BIT_16;
        config.intf = V3_AUD_INTF_I2S_MASTER;
        config.stereoOn = 0;
        config.expandOn = 0;
        config.frmNum = 30;
        config.packNumPerFrm = 320;
        config.chnNum = 1;
        config.syncRxClkOn = 0;
        if (ret = v3_aud.fnSetDeviceConfig(_v3_aud_dev, &config))
            return ret;
    }
    if (ret = v3_aud.fnEnableDevice(_v3_aud_dev))
        return ret;
    
    if (ret = v3_aud.fnEnableChannel(_v3_aud_dev, _v3_aud_chn))
        return ret;

    return EXIT_SUCCESS;
}

void *v3_audio_thread(void)
{
    int ret;

    v3_aud_frm frame;
    v3_aud_efrm echoFrame;
    memset(&frame, 0, sizeof(frame));
    memset(&echoFrame, 0, sizeof(echoFrame));

    while (keepRunning && audioOn) {
        if (ret = v3_aud.fnGetFrame(_v3_aud_dev, _v3_aud_chn, 
            &frame, &echoFrame, 128)) {
            HAL_WARNING("v3_aud", "Getting the frame failed "
                "with %#x!\n", ret);
            continue;
        }

        if (v3_aud_cb) {
            hal_audframe outFrame;
            outFrame.channelCnt = 1;
            outFrame.data[0] = frame.addr[0];
            outFrame.length[0] = frame.length;
            outFrame.seq = frame.sequence;
            outFrame.timestamp = frame.timestamp;
            (v3_aud_cb)(&outFrame);
        }

        if (ret = v3_aud.fnFreeFrame(_v3_aud_dev, _v3_aud_chn,
            &frame, &echoFrame)) {
            HAL_WARNING("v3_aud", "Releasing the frame failed"
                " with %#x!\n", ret);
        }
    }
    HAL_INFO("v3_aud", "Shutting down capture thread...\n");
}

int v3_channel_bind(char index)
{
    int ret;

    if (ret = v3_vpss.fnEnableChannel(_v3_vpss_grp, index))
        return ret;

    {
        v3_sys_bind source = { .module = V3_SYS_MOD_VPSS, 
            .device = _v3_vpss_grp, .channel = index };
        v3_sys_bind dest = { .module = V3_SYS_MOD_VENC,
            .device = _v3_venc_dev, .channel = index };
        if (ret = v3_sys.fnBind(&source, &dest))
            return ret;
    }

    return EXIT_SUCCESS;
}

int v3_channel_create(char index, short width, short height, char mirror, char flip, char framerate)
{
    int ret;

    {
        v3_vpss_chn channel;
        memset(&channel, 0, sizeof(channel));
        channel.srcFps = v3_config.isp.framerate;
        channel.dstFps = framerate;
        channel.mirror = mirror;
        channel.flip = flip;
        if (ret = v3_vpss.fnSetChannelConfig(_v3_vpss_grp, index, &channel))
            return ret;
    }
    {
        v3_vpss_mode mode;
        mode.userModeOn = 1;
        mode.dest.width = width;
        mode.dest.height = height;
        mode.doubleOn = 0;
        mode.pixFmt = V3_PIXFMT_YUV420SP;
        mode.compress = V3_COMPR_NONE;
        if (ret = v3_vpss.fnSetChannelMode(_v3_vpss_grp, index, &mode))
            return ret;
    }

    return EXIT_SUCCESS;
}

int v3_channel_grayscale(char enable)
{
    int ret;
    int active = enable;

    for (char i = 0; i < V3_VENC_CHN_NUM; i++)
        if (v3_state[i].enable)
            if (ret = v3_venc.fnSetColorToGray(i, &active))
                return ret;

    return EXIT_SUCCESS;
}

int v3_channel_unbind(char index)
{
    int ret;

    if (ret = v3_vpss.fnDisableChannel(_v3_vpss_grp, index))
        return ret;

    {
        v3_sys_bind source = { .module = V3_SYS_MOD_VPSS, 
            .device = _v3_vpss_grp, .channel = index };
        v3_sys_bind dest = { .module = V3_SYS_MOD_VENC,
            .device = _v3_venc_dev, .channel = index };
        if (ret = v3_sys.fnUnbind(&source, &dest))
            return ret;
    }

    return EXIT_SUCCESS;
}

void *v3_image_thread(void)
{
    int ret;

    if (ret = v3_isp.fnRun(_v3_isp_dev))
        HAL_DANGER("v3_isp", "Operation failed with %#x!\n", ret);
    HAL_INFO("v3_isp", "Shutting down ISP thread...\n");
}

int v3_pipeline_create(void)
{
    int ret;

    if (ret = v3_sensor_config())
        return ret;

    if (ret = v3_vi.fnSetDeviceConfig(_v3_vi_dev, &v3_config.videv))
        return ret;
    if (ret = v3_vi.fnEnableDevice(_v3_vi_dev))
        return ret;

    {
        v3_vi_chn channel;
        channel.capt.width = v3_config.vichn.capt.width ? 
            v3_config.vichn.capt.width : v3_config.videv.rect.width;
        channel.capt.height = v3_config.vichn.capt.height ? 
            v3_config.vichn.capt.height : v3_config.videv.rect.height;
        channel.capt.x = 0;
        channel.capt.y = 0;
        channel.dest.width = v3_config.vichn.dest.width ? 
            v3_config.vichn.dest.width : v3_config.videv.rect.width;
        channel.dest.height = v3_config.vichn.dest.height ? 
            v3_config.vichn.dest.height : v3_config.videv.rect.height;
        channel.field = v3_config.vichn.field;
        channel.pixFmt = V3_PIXFMT_YUV420SP;
        channel.compress = V3_COMPR_NONE;
        channel.mirror = 0;
        channel.flip = 0;
        channel.srcFps = -1;
        channel.dstFps = -1;
        if (ret = v3_vi.fnSetChannelConfig(_v3_vi_chn, &channel))
            return ret;
    }
    if (ret = v3_vi.fnEnableChannel(_v3_vi_chn))
        return ret;

    if (ret = v3_snr_drv.fnRegisterCallback())
        return ret;
    
    if (ret = v3_isp.fnRegisterAE(_v3_vi_dev, &v3_ae_lib))
        return ret;
    if (ret = v3_isp.fnRegisterAWB(_v3_vi_dev, &v3_awb_lib))
        return ret;
    if (ret = v3_isp.fnMemInit(_v3_vi_dev))
        return ret;

    if (ret = v3_isp.fnSetWDRMode(_v3_vi_dev, &v3_config.mode))
        return ret;
    if (ret = v3_isp.fnSetDeviceConfig(_v3_vi_dev, &v3_config.isp))
        return ret;
    if (ret = v3_isp.fnInit(_v3_vi_dev))
        return ret;
    
    {
        v3_vpss_grp group;
        memset(&group, 0, sizeof(group));
        group.dest.width = v3_config.vichn.capt.width ? 
            v3_config.vichn.capt.width : v3_config.videv.rect.width;
        group.dest.height = v3_config.vichn.capt.height ? 
            v3_config.vichn.capt.height : v3_config.videv.rect.height;
        group.pixFmt = V3_PIXFMT_YUV420SP;
        group.nredOn = 1;
        group.interlMode = v3_config.videv.progressiveOn ? 1 : 2;
        if (ret = v3_vpss.fnCreateGroup(_v3_vpss_grp, &group))
            return ret;
    }
    if (ret = v3_vpss.fnStartGroup(_v3_vpss_grp))
        return ret;

    {
        v3_sys_bind source = { .module = V3_SYS_MOD_VIU, 
            .device = _v3_vi_dev, .channel = _v3_vi_chn };
        v3_sys_bind dest = { .module = V3_SYS_MOD_VPSS, 
            .device = _v3_vpss_grp, .channel = 0 };
        if (ret = v3_sys.fnBind(&source, &dest))
            return ret;
    }

    return EXIT_SUCCESS;
}

void v3_pipeline_destroy(void)
{
    v3_isp.fnExit(_v3_vi_dev);
    v3_isp.fnUnregisterAWB(_v3_vi_dev, &v3_awb_lib);
    v3_isp.fnUnregisterAE(_v3_vi_dev, &v3_ae_lib);

    v3_snr_drv.fnUnRegisterCallback();

    for (char grp = 0; grp < V3_VPSS_GRP_NUM; grp++)
    {
        for (char chn = 0; chn < V3_VPSS_CHN_NUM; chn++)
            v3_vpss.fnDisableChannel(grp, chn);

        {
            v3_sys_bind source = { .module = V3_SYS_MOD_VIU, 
                .device = _v3_vi_dev, .channel = _v3_vi_chn };
            v3_sys_bind dest = { .module = V3_SYS_MOD_VPSS,
                .device = grp, .channel = 0 };
            v3_sys.fnUnbind(&source, &dest);
        }

        v3_vpss.fnStopGroup(grp);
        v3_vpss.fnDestroyGroup(grp);
    }
    
    v3_vi.fnDisableChannel(_v3_vi_chn);

    v3_vi.fnDisableDevice(_v3_vi_dev);

    v3_sensor_deconfig();
}

int v3_region_create(char handle, hal_rect rect, short opacity)
{
    int ret;

    v3_sys_bind dest = { .module = V3_SYS_MOD_VENC, .device = _v3_venc_dev };
    v3_rgn_cnf region, regionCurr;
    v3_rgn_chn attrib, attribCurr;

    rect.height += rect.height & 1;
    rect.width += rect.width & 1;

    memset(&region, 0, sizeof(region));
    region.type = V3_RGN_TYPE_OVERLAY;
    region.overlay.pixFmt = V3_PIXFMT_ARGB1555;
    region.overlay.size.width = rect.width;
    region.overlay.size.height = rect.height;
    region.overlay.canvas = handle + 1;

    if (v3_rgn.fnGetRegionConfig(handle, &regionCurr)) {
        HAL_INFO("v3_rgn", "Creating region %d...\n", handle);
        if (ret = v3_rgn.fnCreateRegion(handle, &region))
            return ret;
    } else if (regionCurr.type != region.type ||
        regionCurr.overlay.size.height != region.overlay.size.height || 
        regionCurr.overlay.size.width != region.overlay.size.width) {
        HAL_INFO("v3_rgn", "Parameters are different, recreating "
            "region %d...\n", handle);
        v3_rgn.fnDetachChannel(handle, &dest);
        v3_rgn.fnDestroyRegion(handle);
        if (ret = v3_rgn.fnCreateRegion(handle, &region))
            return ret;
    }

    if (v3_rgn.fnGetChannelConfig(handle, &dest, &attribCurr))
        HAL_INFO("v3_rgn", "Attaching region %d...\n", handle);
    else if (attribCurr.overlay.point.x != rect.x || attribCurr.overlay.point.x != rect.y) {
        HAL_INFO("v3_rgn", "Position has changed, reattaching "
            "region %d...\n", handle);
        v3_rgn.fnDetachChannel(handle, &dest);
    }

    memset(&attrib, 0, sizeof(attrib));
    attrib.show = 1;
    attrib.type = V3_RGN_TYPE_OVERLAY;
    attrib.overlay.bgAlpha = 0;
    attrib.overlay.fgAlpha = opacity >> 1;
    attrib.overlay.point.x = rect.x;
    attrib.overlay.point.y = rect.y;
    attrib.overlay.layer = 7;

    v3_rgn.fnAttachChannel(handle, &dest, &attrib);

    return EXIT_SUCCESS;
}

void v3_region_destroy(char handle)
{
    v3_sys_bind dest = { .module = V3_SYS_MOD_VENC, .device = _v3_venc_dev };
    
    v3_rgn.fnDetachChannel(handle, &dest);
    v3_rgn.fnDestroyRegion(handle);
}

int v3_region_setbitmap(int handle, hal_bitmap *bitmap)
{
    v3_rgn_bmp nativeBmp = { .data = bitmap->data, .pixFmt = V3_PIXFMT_ARGB1555,
        .size.height = bitmap->dim.height, .size.width = bitmap->dim.width };

    return v3_rgn.fnSetBitmap(handle, &nativeBmp);
}

int v3_sensor_config(void) {
    int fd;
    v3_snr_dev config;
    memset(&config, 0, sizeof(config));
    config.device = 0;
    config.input = v3_config.input_mode;
    if (config.input == V3_SNR_INPUT_MIPI)
        memcpy(&config.mipi, &v3_config.mipi, sizeof(v3_snr_mipi));
    else if (config.input == V3_SNR_INPUT_LVDS)
        memcpy(&config.lvds, &v3_config.lvds, sizeof(v3_snr_lvds));

    if (!access(v3_snr_endp, F_OK))
        fd = open(v3_snr_endp, O_RDWR);
    else fd = open("/dev/mipi", O_RDWR);
    if (fd < 0)
        HAL_ERROR("v3_snr", "Opening imaging device has failed!\n");

    ioctl(fd, _IOW(V3_SNR_IOC_MAGIC, V3_SNR_CMD_RST_MIPI, unsigned int), &config.device);

    ioctl(fd, _IOW(V3_SNR_IOC_MAGIC, V3_SNR_CMD_RST_SENS, unsigned int), &config.device);
    
    if (ioctl(fd, _IOW(V3_SNR_IOC_MAGIC, V3_SNR_CMD_CONF_DEV, v3_snr_dev), &config))
        HAL_ERROR("v3_snr", "Configuring imaging device has failed!\n");

    ioctl(fd, _IOW(V3_SNR_IOC_MAGIC, V3_SNR_CMD_UNRST_MIPI, unsigned int), &config.device);

    ioctl(fd, _IOW(V3_SNR_IOC_MAGIC, V3_SNR_CMD_UNRST_SENS, unsigned int), &config.device);

    close(fd);

    return EXIT_SUCCESS;
}

void v3_sensor_deconfig(void) {
    int fd;
    v3_snr_dev config;
    config.device = 0;

    if (!access(v3_snr_endp, F_OK))
        fd = open(v3_snr_endp, O_RDWR);
    else fd = open("/dev/mipi", O_RDWR);
    if (fd < 0)
        HAL_DANGER("v3_snr", "Opening imaging device has failed!\n");

    ioctl(fd, _IOW(V3_SNR_IOC_MAGIC, V3_SNR_CMD_RST_SENS, unsigned int), &config.device);

    ioctl(fd, _IOW(V3_SNR_IOC_MAGIC, V3_SNR_CMD_RST_MIPI, unsigned int), &config.device);

    close(fd);
}

void v3_sensor_deinit(void)
{
    dlclose(v3_snr_drv.handle);
    v3_snr_drv.handle = NULL;
}

int v3_sensor_init(char *name, char *obj)
{
    char path[128];
    char* dirs[] = {"%s", "./%s", "/usr/lib/sensors/%s", NULL};
    char **dir = dirs;

    while (*dir) {
        sprintf(path, *dir++, name);
        if (v3_snr_drv.handle = dlopen(path, RTLD_NOW | RTLD_GLOBAL))
            break;
    } if (!v3_snr_drv.handle)
        HAL_ERROR("v3_snr", "Failed to load the sensor driver");
    
    if (!(v3_snr_drv.fnRegisterCallback = (int(*)(void))dlsym(v3_snr_drv.handle, "sensor_register_callback")))
        HAL_ERROR("v3_snr", "Failed to connect the callback register function");
    if (!(v3_snr_drv.fnUnRegisterCallback = (int(*)(void))dlsym(v3_snr_drv.handle, "sensor_unregister_callback")))
        HAL_ERROR("v3_snr", "Failed to connect the callback unregister function");

    return EXIT_SUCCESS;
}

int v3_video_create(char index, hal_vidconfig *config)
{
    int ret;
    v3_venc_chn channel;
    v3_venc_attr_h26x *attrib;
    memset(&channel, 0, sizeof(channel));

    if (config->codec == HAL_VIDCODEC_JPG) {
        channel.attrib.codec = V3_VENC_CODEC_JPEG;
        channel.attrib.jpg.maxPic.width = config->width;
        channel.attrib.jpg.maxPic.height = config->height;
        channel.attrib.jpg.bufSize =
            ALIGN_UP(config->height, 16) * ALIGN_UP(config->width, 16);
        channel.attrib.jpg.byFrame = 1;
        channel.attrib.jpg.pic.width = config->width;
        channel.attrib.jpg.pic.height = config->height;
        channel.attrib.jpg.dcfThumbs = 0;
        goto attach;
    } else if (config->codec == HAL_VIDCODEC_MJPG) {
        channel.attrib.codec = V3_VENC_CODEC_MJPG;
        channel.attrib.mjpg.maxPic.width = config->width;
        channel.attrib.mjpg.maxPic.height = config->height;
        channel.attrib.mjpg.bufSize =
            ALIGN_UP(config->height, 16) * ALIGN_UP(config->width, 16);
        channel.attrib.mjpg.byFrame = 1;
        channel.attrib.mjpg.pic.width = config->width;
        channel.attrib.mjpg.pic.height = config->height;
        switch (config->mode) {
            case HAL_VIDMODE_CBR:
                channel.rate.mode = V3_VENC_RATEMODE_MJPGCBR;
                channel.rate.mjpgCbr = (v3_venc_rate_mjpgcbr){ .statTime = 1, .srcFps = config->framerate,
                    .dstFps = config->framerate, .bitrate = config->bitrate, .avgLvl = 1 }; break;
            case HAL_VIDMODE_VBR:
                channel.rate.mode = V3_VENC_RATEMODE_MJPGVBR;
                channel.rate.mjpgVbr = (v3_venc_rate_mjpgvbr){ .statTime = 1, .srcFps = config->framerate,
                    .dstFps = config->framerate , .maxBitrate = MAX(config->bitrate, config->maxBitrate), 
                    .maxQual = config->maxQual, .minQual = config->maxQual }; break;
            case HAL_VIDMODE_QP:
                channel.rate.mode = V3_VENC_RATEMODE_MJPGQP;
                channel.rate.mjpgQp = (v3_venc_rate_mjpgqp){ .srcFps = config->framerate,
                    .dstFps = config->framerate, .quality = config->maxQual }; break;
            default:
                HAL_ERROR("v3_venc", "MJPEG encoder can only support CBR, VBR or fixed QP modes!");
        }
        goto attach;
    } else if (config->codec == HAL_VIDCODEC_H265) {
        channel.attrib.codec = V3_VENC_CODEC_H265;
        attrib = &channel.attrib.h265;
        switch (config->mode) {
            case HAL_VIDMODE_CBR:
                channel.rate.mode = V3_VENC_RATEMODE_H265CBR;
                channel.rate.h265Cbr = (v3_venc_rate_h26xcbr){ .gop = config->gop,
                    .statTime = 1, .srcFps = config->framerate, .dstFps = config->framerate,
                    .bitrate = config->bitrate, .avgLvl = 1 }; break;
            case HAL_VIDMODE_VBR:
                channel.rate.mode = V3_VENC_RATEMODE_H265VBR;
                channel.rate.h265Vbr = (v3_venc_rate_h26xvbr){ .gop = config->gop,
                    .statTime = 1, .srcFps = config->framerate, .dstFps = config->framerate, 
                    .maxBitrate = MAX(config->bitrate, config->maxBitrate), .maxQual = config->maxQual,
                    .minQual = config->minQual, .minIQual = config->minQual }; break;
            case HAL_VIDMODE_QP:
                channel.rate.mode = V3_VENC_RATEMODE_H265QP;
                channel.rate.h265Qp = (v3_venc_rate_h26xqp){ .gop = config->gop,
                    .srcFps = config->framerate, .dstFps = config->framerate, .interQual = config->maxQual, 
                    .predQual = config->minQual, .bipredQual = config->minQual }; break;
            case HAL_VIDMODE_AVBR:
                channel.rate.mode = V3_VENC_RATEMODE_H265AVBR;
                channel.rate.h265Avbr = (v3_venc_rate_h26xxvbr){ .gop = config->gop,
                    .statTime = 1, .srcFps = config->framerate, .dstFps = config->framerate,
                    .bitrate = config->bitrate }; break;
            default:
                HAL_ERROR("v3_venc", "H.265 encoder does not support this mode!");
        }
    } else if (config->codec == HAL_VIDCODEC_H264) {
        channel.attrib.codec = V3_VENC_CODEC_H264;
        attrib = &channel.attrib.h264;
        switch (config->mode) {
            case HAL_VIDMODE_CBR:
                channel.rate.mode = V3_VENC_RATEMODE_H264CBR;
                channel.rate.h264Cbr = (v3_venc_rate_h26xcbr){ .gop = config->gop,
                    .statTime = 1, .srcFps = config->framerate, .dstFps = config->framerate,
                    .bitrate = config->bitrate, .avgLvl = 1 }; break;
            case HAL_VIDMODE_VBR:
                channel.rate.mode = V3_VENC_RATEMODE_H264VBR;
                channel.rate.h264Vbr = (v3_venc_rate_h26xvbr){ .gop = config->gop,
                    .statTime = 1, .srcFps = config->framerate, .dstFps = config->framerate, 
                    .maxBitrate = MAX(config->bitrate, config->maxBitrate), .maxQual = config->maxQual,
                    .minQual = config->minQual, .minIQual = config->minQual }; break;
            case HAL_VIDMODE_QP:
                channel.rate.mode = V3_VENC_RATEMODE_H264QP;
                channel.rate.h264Qp = (v3_venc_rate_h26xqp){ .gop = config->gop,
                    .srcFps = config->framerate, .dstFps = config->framerate, .interQual = config->maxQual, 
                    .predQual = config->minQual, .bipredQual = config->minQual }; break;
            case HAL_VIDMODE_AVBR:
                channel.rate.mode = V3_VENC_RATEMODE_H264AVBR;
                channel.rate.h264Avbr = (v3_venc_rate_h26xxvbr){ .gop = config->gop,
                    .statTime = 1, .srcFps = config->framerate, .dstFps = config->framerate,
                    .bitrate = config->bitrate }; break;
            default:
                HAL_ERROR("v3_venc", "H.264 encoder does not support this mode!");
        }
    } else HAL_ERROR("v3_venc", "This codec is not supported by the hardware!");
    attrib->maxPic.width = config->width;
    attrib->maxPic.height = config->height;
    attrib->bufSize = config->height * config->width;
    attrib->profile = config->profile;
    attrib->byFrame = 1;
    attrib->pic.width = config->width;
    attrib->pic.height = config->height;
attach:
    if (ret = v3_venc.fnCreateChannel(index, &channel))
        return ret;

    if (config->codec != HAL_VIDCODEC_JPG && 
        (ret = v3_venc.fnStartReceiving(index)))
        return ret;

    v3_state[index].payload = config->codec;

    return EXIT_SUCCESS;
}

int v3_video_destroy(char index)
{
    int ret;

    v3_state[index].enable = 0;
    v3_state[index].payload = HAL_VIDCODEC_UNSPEC;

    v3_venc.fnStopReceiving(index);

    {
        v3_sys_bind source = { .module = V3_SYS_MOD_VPSS, 
            .device = _v3_vpss_grp, .channel = index };
        v3_sys_bind dest = { .module = V3_SYS_MOD_VENC,
            .device = _v3_venc_dev, .channel = index };
        if (ret = v3_sys.fnUnbind(&source, &dest))
            return ret;
    }

    if (ret = v3_venc.fnDestroyChannel(index))
        return ret;
    
    if (ret = v3_vpss.fnDisableChannel(_v3_vpss_grp, index))
        return ret;

    return EXIT_SUCCESS;
}
    
int v3_video_destroy_all(void)
{
    int ret;

    for (char i = 0; i < V3_VENC_CHN_NUM; i++)
        if (v3_state[i].enable)
            if (ret = v3_video_destroy(i))
                return ret;

    return EXIT_SUCCESS;
}

void v3_video_request_idr(char index)
{
    v3_venc.fnRequestIdr(index, 1);
}

int v3_video_snapshot_grab(char index, hal_jpegdata *jpeg)
{
    int ret;

    if (ret = v3_channel_bind(index)) {
        HAL_DANGER("v3_venc", "Binding the encoder channel "
            "%d failed with %#x!\n", index, ret);
        goto abort;
    }

    unsigned int count = 1;
    if (ret = v3_venc.fnStartReceivingEx(index, &count)) {
        HAL_DANGER("v3_venc", "Requesting one frame "
            "%d failed with %#x!\n", index, ret);
        goto abort;
    }

    int fd = v3_venc.fnGetDescriptor(index);

    struct timeval timeout = { .tv_sec = 2, .tv_usec = 0 };
    fd_set readFds;
    FD_ZERO(&readFds);
    FD_SET(fd, &readFds);
    ret = select(fd + 1, &readFds, NULL, NULL, &timeout);
    if (ret < 0) {
        HAL_DANGER("v3_venc", "Select operation failed!\n");
        goto abort;
    } else if (ret == 0) {
        HAL_DANGER("v3_venc", "Capture stream timed out!\n");
        goto abort;
    }

    if (FD_ISSET(fd, &readFds)) {
        v3_venc_stat stat;
        if (ret = v3_venc.fnQuery(index, &stat)) {
            HAL_DANGER("v3_venc", "Querying the encoder channel "
                "%d failed with %#x!\n", index, ret);
            goto abort;
        }

        if (!stat.curPacks) {
            HAL_DANGER("v3_venc", "Current frame is empty, skipping it!\n");
            goto abort;
        }

        v3_venc_strm strm;
        memset(&strm, 0, sizeof(strm));
        strm.packet = (v3_venc_pack*)malloc(sizeof(v3_venc_pack) * stat.curPacks);
        if (!strm.packet) {
            HAL_DANGER("v3_venc", "Memory allocation on channel %d failed!\n", index);
            goto abort;
        }
        strm.count = stat.curPacks;

        if (ret = v3_venc.fnGetStream(index, &strm, stat.curPacks)) {
            HAL_DANGER("v3_venc", "Getting the stream on "
                "channel %d failed with %#x!\n", index, ret);
            free(strm.packet);
            strm.packet = NULL;
            goto abort;
        }

        {
            jpeg->jpegSize = 0;
            for (unsigned int i = 0; i < strm.count; i++) {
                v3_venc_pack *pack = &strm.packet[i];
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
        v3_venc.fnFreeStream(index, &strm);
    }

    v3_venc.fnFreeDescriptor(index);

    v3_venc.fnStopReceiving(index);

    v3_channel_unbind(index);

    return ret;
}

void *v3_video_thread(void)
{
    int ret, maxFd = 0;

    for (int i = 0; i < V3_VENC_CHN_NUM; i++) {
        if (!v3_state[i].enable) continue;
        if (!v3_state[i].mainLoop) continue;

        ret = v3_venc.fnGetDescriptor(i);
        if (ret < 0) {
            HAL_DANGER("v3_venc", "Getting the encoder descriptor failed with %#x!\n", ret);
            return (void*)0;
        }
        v3_state[i].fileDesc = ret;

        if (maxFd <= v3_state[i].fileDesc)
            maxFd = v3_state[i].fileDesc;
    }

    v3_venc_stat stat;
    v3_venc_strm stream;
    struct timeval timeout;
    fd_set readFds;

    while (keepRunning) {
        FD_ZERO(&readFds);
        for(int i = 0; i < V3_VENC_CHN_NUM; i++) {
            if (!v3_state[i].enable) continue;
            if (!v3_state[i].mainLoop) continue;
            FD_SET(v3_state[i].fileDesc, &readFds);
        }

        timeout.tv_sec = 2;
        timeout.tv_usec = 0;
        ret = select(maxFd + 1, &readFds, NULL, NULL, &timeout);
        if (ret < 0) {
            HAL_DANGER("v3_venc", "Select operation failed!\n");
            break;
        } else if (ret == 0) {
            HAL_WARNING("v3_venc", "Main stream loop timed out!\n");
            continue;
        } else {
            for (int i = 0; i < V3_VENC_CHN_NUM; i++) {
                if (!v3_state[i].enable) continue;
                if (!v3_state[i].mainLoop) continue;
                if (FD_ISSET(v3_state[i].fileDesc, &readFds)) {
                    memset(&stream, 0, sizeof(stream));
                    
                    if (ret = v3_venc.fnQuery(i, &stat)) {
                        HAL_DANGER("v3_venc", "Querying the encoder channel "
                            "%d failed with %#x!\n", i, ret);
                        break;
                    }

                    if (!stat.curPacks) {
                        HAL_WARNING("v3_venc", " Current frame is empty, skipping it!\n");
                        continue;
                    }

                    stream.packet = (v3_venc_pack*)malloc(
                        sizeof(v3_venc_pack) * stat.curPacks);
                    if (!stream.packet) {
                        HAL_DANGER("v3_venc", "Memory allocation on channel %d failed!\n", i);
                        break;
                    }
                    stream.count = stat.curPacks;

                    if (ret = v3_venc.fnGetStream(i, &stream, 40)) {
                        HAL_DANGER("v3_venc", "Getting the stream on "
                            "channel %d failed with %#x!\n", i, ret);
                        break;
                    }

                    if (v3_vid_cb) {
                        hal_vidstream outStrm;
                        hal_vidpack outPack[stream.count];
                        outStrm.count = stream.count;
                        outStrm.seq = stream.sequence;
                        for (int j = 0; j < stream.count; j++) {
                            v3_venc_pack *pack = &stream.packet[j];
                            outPack[j].data = pack->data;
                            outPack[j].length = pack->length;
                            outPack[j].naluCnt = 1;
                            outPack[j].nalu[0].length = pack->length;
                            outPack[j].nalu[0].offset = pack->offset;
                            switch (v3_state[i].payload) {
                                case HAL_VIDCODEC_H264:
                                    outPack[j].nalu[0].type = pack->naluType.h264Nalu;
                                    break;
                                case HAL_VIDCODEC_H265:
                                    outPack[j].nalu[0].type = pack->naluType.h265Nalu;
                                    break;
                            }
                            outPack[j].offset = pack->offset;
                            outPack[j].timestamp = pack->timestamp;
                        }
                        outStrm.pack = outPack;
                        (*v3_vid_cb)(i, &outStrm);
                    }

                    if (ret = v3_venc.fnFreeStream(i, &stream)) {
                        HAL_WARNING("v3_venc", "Releasing the stream on "
                            "channel %d failed with %#x!\n", i, ret);
                    }
                    free(stream.packet);
                    stream.packet = NULL;
                }
            }
        }
    }

    HAL_INFO("v3_venc", "Shutting down encoding thread...\n");
}

void v3_system_deinit(void)
{
    v3_sys.fnExit();
    v3_vb.fnExit();

    v3_sensor_deinit();
}

int v3_system_init(char *snrConfig)
{
    int ret;

    printf("App built with headers v%s\n", V3_SYS_API);

    {
        v3_sys_ver version;
        v3_sys.fnGetVersion(&version);
        puts(version.version);
    }

    if (v3_parse_sensor_config(snrConfig, &v3_config) != CONFIG_OK)
        HAL_ERROR("v3_sys", "Can't load sensor config\n");

    if (ret = v3_sensor_init(v3_config.dll_file, v3_config.sensor_type))
        return ret;

    v3_sys.fnExit();
    v3_vb.fnExit();

    {
        int alignWidth = 16;
        v3_vb_pool pool;

        memset(&pool, 0, sizeof(pool)); 
        
        pool.count = 1;
        pool.comm[0].blockSize = v3_buffer_calculate_venc(
            v3_config.vichn.capt.width ? 
                v3_config.vichn.capt.width : v3_config.videv.rect.width,
            v3_config.vichn.capt.height ? 
                v3_config.vichn.capt.height : v3_config.videv.rect.height,
            V3_PIXFMT_YUV420SP, alignWidth);
        pool.comm[0].blockCnt = 4;

        if (ret = v3_vb.fnConfigPool(&pool))
            return ret;
        v3_vb_supl supl = V3_VB_JPEG_MASK;
        if (ret = v3_vb.fnConfigSupplement(&supl))
            return ret;
        if (ret = v3_vb.fnInit())
            return ret;
        
        if (ret = v3_sys.fnSetAlignment(&alignWidth))
            return ret;
    }
    if (ret = v3_sys.fnInit())
        return ret;

    return EXIT_SUCCESS;
}

float v3_system_readtemp(void)
{
    char v3a_device = 0;
    int val, prep = 0x60fa0000;
    float result = 0.0 / 0.0;

    if (EQUALS(chip, "Hi3516AV200") ||
        EQUALS(chip, "Hi3519V101") ||
        EQUALS(chip, "Hi3556V100") ||
        EQUALS(chip, "Hi3559V100"))
        v3a_device = 1;

    if (hal_registry(v3a_device ? 0x120a0110 : 0x1203009c, &val, OP_READ) && prep != val)
        hal_registry(v3a_device ? 0x120a0110 : 0x1203009c, &prep, OP_WRITE);

    if (!hal_registry(v3a_device ? 0x120a0118 : 0x120300a4, &val, OP_READ))
        return result;

    result = val & ((1 << 10) - 1);
    return ((result - 125) / 806) * 165 - 40;
}

#endif