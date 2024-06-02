#include "v3_hal.h"

v3_isp_alg      v3_ae_lib = { .id = 0, .libName = "ae_lib" };
v3_aud_impl     v3_aud;
v3_isp_alg      v3_awb_lib = { .id = 0, .libName = "awb_lib" };
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
int (*v3_venc_cb)(char, hal_vidstream*);

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

int v3_audio_init(void)
{
    int ret;

    {
        v3_aud_cnf config;
        config.rate = 48000;
        config.bit = V3_AUD_BIT_16;
        config.intf = V3_AUD_INTF_I2S_SLAVE;
        config.stereoOn = 0;
        config.expandOn = 0;
        config.frmNum = 0;
        config.packNumPerFrm = 0;
        config.chnNum = 0;
        config.syncRxClkOn = 0;
        if (ret = v3_aud.fnSetDeviceConfig(_v3_aud_dev, &config))
            return ret;
    }
    if (ret = v3_aud.fnEnableDevice(_v3_aud_dev))
        return ret;
    
    if (ret = v3_aud.fnEnableChannel(_v3_aud_dev, _v3_aud_chn))
        return ret;
    //if (ret = v3_aud.fnSetVolume(_v3_aud_dev, _v3_aud_chn, 0xF6))
    //    return ret;

    return EXIT_SUCCESS;
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

int v3_channel_create(char index, char mirror, char flip, char framerate)
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
        fprintf(stderr, "[v3_isp] Operation failed with %#x!\n", ret);
    fprintf(stderr, "[v3_isp] Shutting down ISP thread...\n");
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
        channel.dest.width = v3_config.isp.capt.width;
        channel.dest.height = v3_config.isp.capt.height;
        channel.pixFmt = V3_PIXFMT_YUV420SP;
        channel.compress = V3_COMPR_NONE;
        channel.mirror = 0;
        channel.flip = 0;
        channel.srcFps = v3_config.isp.framerate;
        channel.dstFps = v3_config.isp.framerate;
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

    if (ret = v3_isp.fnSetDeviceConfig(_v3_vi_dev, &v3_config.isp))
        return ret;
    if (ret = v3_isp.fnInit(_v3_vi_dev))
        return ret;
    
    {
        v3_vpss_grp group;
        memset(&group, 0, sizeof(group));
        group.dest.width = v3_config.isp.capt.width;
        group.dest.height = v3_config.isp.capt.height;
        group.pixFmt = V3_PIXFMT_YUV420SP;
        group.nredOn = 1;
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
    v3_isp.fnUnregisterAE(_v3_vi_dev, &v3_ae_lib);
    v3_isp.fnUnregisterAWB(_v3_vi_dev, &v3_awb_lib);

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

int v3_region_create(char handle, hal_rect rect)
{
    int ret;

    v3_sys_bind channel = { .module = V3_SYS_MOD_VENC,
        .device = _v3_venc_dev, .channel = 0 };
    v3_rgn_cnf region, regionCurr;
    v3_rgn_chn attrib, attribCurr;

    memset(&region, 0, sizeof(region));
    region.type = V3_RGN_TYPE_OVERLAY;
    region.overlay.pixFmt = V3_PIXFMT_ARGB1555;
    region.overlay.size.width = rect.width;
    region.overlay.size.height = rect.height;
    region.overlay.canvas = handle;

    if (ret = v3_rgn.fnGetRegionConfig(handle, &regionCurr)) {
        fprintf(stderr, "[v3_rgn] Creating region %d...\n", handle);
        if (ret = v3_rgn.fnCreateRegion(handle, &region))
            return ret;
    } else if (regionCurr.overlay.size.height != region.overlay.size.height || 
        regionCurr.overlay.size.width != region.overlay.size.width) {
        fprintf(stderr, "[v3_rgn] Parameters are different, recreating "
            "region %d...\n", handle);
        v3_rgn.fnDetachChannel(handle, &channel);
        v3_rgn.fnDestroyRegion(handle);
        if (ret = v3_rgn.fnCreateRegion(handle, &region))
            return ret;
    }

    if (v3_rgn.fnGetChannelConfig(handle, &channel, &attribCurr))
        fprintf(stderr, "[v3_rgn] Attaching region %d...\n", handle);
    else if (attribCurr.overlay.point.x != rect.x || attribCurr.overlay.point.x != rect.y) {
        fprintf(stderr, "[v3_rgn] Position has changed, reattaching "
            "region %d...\n", handle);
        v3_rgn.fnDetachChannel(handle, &channel);
    }

    memset(&attrib, 0, sizeof(attrib));
    attrib.show = 1;
    attrib.type = V3_RGN_TYPE_OVERLAY;
    attrib.overlay.bgAlpha = 0;
    attrib.overlay.fgAlpha = 128;
    attrib.overlay.point.x = rect.x;
    attrib.overlay.point.y = rect.y;
    attrib.overlay.layer = 7;

    v3_rgn.fnAttachChannel(handle, &channel, &attrib);

    return ret;
}

void v3_region_destroy(char handle)
{
    v3_sys_bind channel = { .module = V3_SYS_MOD_VENC,
        .device = _v3_venc_dev, .channel = 0 };
    
    v3_rgn.fnDetachChannel(handle, &channel);
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
        V3_ERROR("Opening imaging device has failed!\n");

    ioctl(fd, _IOW(V3_SNR_IOC_MAGIC, V3_SNR_CMD_RST_MIPI, unsigned int), &config.device);

    ioctl(fd, _IOW(V3_SNR_IOC_MAGIC, V3_SNR_CMD_RST_SENS, unsigned int), &config.device);
    
    if (ioctl(fd, _IOW(V3_SNR_IOC_MAGIC, V3_SNR_CMD_CONF_DEV, v3_snr_dev), &config))
        V3_ERROR("Configuring imaging device has failed!\n");

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
        fprintf(stderr, "[v3_hal] Opening imaging device has failed!\n");

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
    char* dirs[] = {"%s", "./%s", "/usr/lib/sensors/%s"};
    char **dir = dirs;

    while (*dir++) {
        sprintf(path, *dir, name);
        if (v3_snr_drv.handle = dlopen(path, RTLD_LAZY | RTLD_GLOBAL))
            break;
    } if (!v3_snr_drv.handle)
        V3_ERROR("Failed to load the sensor driver");
    
    if (!(v3_snr_drv.fnRegisterCallback = (int(*)(void))dlsym(v3_snr_drv.handle, "sensor_register_callback")))
        V3_ERROR("Failed to connect the callback register function");
    if (!(v3_snr_drv.fnUnRegisterCallback = (int(*)(void))dlsym(v3_snr_drv.handle, "sensor_unregister_callback")))
        V3_ERROR("Failed to connect the callback register function");

    return EXIT_SUCCESS;
}

int v3_video_create(char index, hal_vidconfig *config)
{
    int ret;
    v3_venc_chn channel;
    v3_venc_attr_h26x *attrib;

    if (config->codec == HAL_VIDCODEC_JPG) {
        channel.attrib.codec = V3_VENC_CODEC_JPEGE;
        channel.attrib.jpg.maxPic.width = ALIGN_BACK(config->width, 16);
        channel.attrib.jpg.maxPic.height = ALIGN_BACK(config->height, 16);
        channel.attrib.jpg.bufSize = 
            ALIGN_BACK(config->height, 16) * ALIGN_BACK(config->width, 16);
        channel.attrib.jpg.byFrame = 1;
        channel.attrib.jpg.pic.width = config->width;
        channel.attrib.jpg.pic.height = config->height;
        channel.attrib.jpg.dcfThumbs = 0;
    } else if (config->codec == HAL_VIDCODEC_MJPG) {
        channel.attrib.codec = V3_VENC_CODEC_MJPG;
        channel.attrib.mjpg.maxPic.width = ALIGN_BACK(config->width, 16);
        channel.attrib.mjpg.maxPic.height = ALIGN_BACK(config->height, 16);
        channel.attrib.mjpg.bufSize = 
            ALIGN_BACK(config->height, 16) * ALIGN_BACK(config->width, 16);
        channel.attrib.mjpg.byFrame = 1;
        channel.attrib.mjpg.pic.width = config->width;
        channel.attrib.mjpg.pic.height = config->height;
        switch (config->mode) {
            case HAL_VIDMODE_CBR:
                channel.rate.mode = V3_VENC_RATEMODE_MJPGCBR;
                channel.rate.mjpgCbr = (v3_venc_rate_mjpgcbr){ .statTime = 1, .srcFps = config->framerate,
                    .dstFps = config->framerate, .bitrate = config->bitrate, .avgLvl = 0 }; break;
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
                V3_ERROR("MJPEG encoder can only support CBR, VBR or fixed QP modes!");
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
                V3_ERROR("H.265 encoder does not support this mode!");
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
                V3_ERROR("H.264 encoder does not support this mode!");
        }
    } else V3_ERROR("This codec is not supported by the hardware!");
    attrib->maxPic.width = ALIGN_BACK(config->width, 16);
    attrib->maxPic.height = ALIGN_BACK(config->height, 16);
    attrib->bufSize = ALIGN_BACK(config->height, 16) * ALIGN_BACK(config->width, 16);
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

int v3_video_snapshot_grab(char index, short width, short height, 
    char quality, hal_jpegdata *jpeg)
{
    int ret;

    if (ret = v3_channel_bind(index)) {
        fprintf(stderr, "[v3_venc] Binding the encoder channel "
            "%d failed with %#x!\n", index, ret);
        goto abort;
    }

    unsigned int count = 1;
    if (v3_venc.fnStartReceivingEx(index, &count)) {
        fprintf(stderr, "[v3_venc] Requesting one frame "
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
        fprintf(stderr, "[v3_venc] Select operation failed!\n");
        goto abort;
    } else if (ret == 0) {
        fprintf(stderr, "[v3_venc] Capture stream timed out!\n");
        goto abort;
    }

    if (FD_ISSET(fd, &readFds)) {
        v3_venc_stat stat;
        if (v3_venc.fnQuery(index, &stat)) {
            fprintf(stderr, "[v3_venc] Querying the encoder channel "
                "%d failed with %#x!\n", index, ret);
            goto abort;
        }

        if (!stat.curPacks) {
            fprintf(stderr, "[v3_venc] Current frame is empty, skipping it!\n");
            goto abort;
        }

        v3_venc_strm strm;
        memset(&strm, 0, sizeof(strm));
        strm.packet = (v3_venc_pack*)malloc(sizeof(v3_venc_pack) * stat.curPacks);
        if (!strm.packet) {
            fprintf(stderr, "[v3_venc] Memory allocation on channel %d failed!\n", index);
            goto abort;
        }
        strm.count = stat.curPacks;

        if (ret = v3_venc.fnGetStream(index, &strm, stat.curPacks)) {
            fprintf(stderr, "[v3_venc] Getting the stream on "
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
    int ret;
    int maxFd = 0;

    for (int i = 0; i < V3_VENC_CHN_NUM; i++) {
        if (!v3_state[i].enable) continue;
        if (!v3_state[i].mainLoop) continue;

        ret = v3_venc.fnGetDescriptor(i);
        if (ret < 0) {
            fprintf(stderr, "[v3_venc] Getting the encoder descriptor failed with %#x!\n", ret);
            return NULL;
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
            fprintf(stderr, "[v3_venc] Select operation failed!\n");
            break;
        } else if (ret == 0) {
            fprintf(stderr, "[v3_venc] Main stream loop timed out!\n");
            continue;
        } else {
            for (int i = 0; i < V3_VENC_CHN_NUM; i++) {
                if (!v3_state[i].enable) continue;
                if (!v3_state[i].mainLoop) continue;
                if (FD_ISSET(v3_state[i].fileDesc, &readFds)) {
                    memset(&stream, 0, sizeof(stream));
                    
                    if (ret = v3_venc.fnQuery(i, &stat)) {
                        fprintf(stderr, "[v3_venc] Querying the encoder channel "
                            "%d failed with %#x!\n", i, ret);
                        break;
                    }

                    if (!stat.curPacks) {
                        fprintf(stderr, "[v3_venc] Current frame is empty, skipping it!\n");
                        continue;
                    }

                    stream.packet = (v3_venc_pack*)malloc(
                        sizeof(v3_venc_pack) * stat.curPacks);
                    if (!stream.packet) {
                        fprintf(stderr, "[v3_venc] Memory allocation on channel %d failed!\n", i);
                        break;
                    }
                    stream.count = stat.curPacks;

                    if (ret = v3_venc.fnGetStream(i, &stream, stat.curPacks)) {
                        fprintf(stderr, "[v3_venc] Getting the stream on "
                            "channel %d failed with %#x!\n", i, ret);
                        break;
                    }

                    if (v3_venc_cb) {
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
                        (*v3_venc_cb)(i, &outStrm);
                    }

                    if (ret = v3_venc.fnFreeStream(i, &stream)) {
                        fprintf(stderr, "[v3_venc] Releasing the stream on "
                            "channel %d failed with %#x!\n", i, ret);
                    }
                    free(stream.packet);
                    stream.packet = NULL;
                }
            }
        }
    }
    fprintf(stderr, "[v3_venc] Shutting down encoding thread...\n");
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

    {
        v3_sys_ver version;
        v3_sys.fnGetVersion(&version);
        printf("App built with headers v%s\n", V3_SYS_API);
        printf("%s\n", version.version);
    }

    if (v3_parse_sensor_config(snrConfig, &v3_config) != CONFIG_OK)
        V3_ERROR("Can't load sensor config\n");

    if (ret = v3_sensor_init(v3_config.dll_file, v3_config.sensor_type))
        return ret;

    v3_sys.fnExit();
    v3_vb.fnExit();

    {
        v3_vb_pool pool;
        memset(&pool, 0, sizeof(pool)); 
        
        pool.count = 2;
        pool.comm[0].blockSize = v3_buffer_calculate_vi(v3_config.isp.capt.width,
            v3_config.isp.capt.height, V3_PIXFMT_RGB_BAYER_8BPP + v3_config.mipi.prec,
            V3_COMPR_NONE, 16);
        pool.comm[0].blockCnt = 3;
        pool.comm[1].blockSize = v3_buffer_calculate_venc(
            v3_config.isp.capt.width, v3_config.isp.capt.height, 
            V3_PIXFMT_YUV420SP, 16);
        pool.comm[1].blockCnt = 2;

        if (ret = v3_vb.fnConfigPool(&pool))
            return ret;
    }
    if (ret = v3_vb.fnInit())
        return ret;

    if (ret = v3_sys.fnInit())
        return ret;

    return EXIT_SUCCESS;
}
