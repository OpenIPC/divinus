#if defined(__ARM_PCS)

#include "v2_hal.h"

v2_isp_alg      v2_ae_lib = { .id = 0, .libName = "hisi_ae_lib" };
v2_aud_impl     v2_aud;
v2_isp_alg      v2_awb_lib = { .id = 0, .libName = "hisi_awb_lib" };
v2_config_impl  v2_config;
v2_isp_impl     v2_isp;
v2_snr_drv_impl v2_snr_drv;
v2_rgn_impl     v2_rgn;
v2_sys_impl     v2_sys;
v2_vb_impl      v2_vb;
v2_venc_impl    v2_venc;
v2_vi_impl      v2_vi;
v2_vpss_impl    v2_vpss;

hal_chnstate v2_state[V2_VENC_CHN_NUM] = {0};
int (*v2_aud_cb)(hal_audframe*);
int (*v2_vid_cb)(char, hal_vidstream*);

char _v2_aud_chn = 0;
char _v2_aud_dev = 0;
char _v2_isp_chn = 0;
char _v2_isp_dev = 0;
char _v2_venc_dev = 0;
char _v2_vi_chn = 0;
char _v2_vi_dev = 0;
char _v2_vpss_chn = 0;
char _v2_vpss_grp = 0;

void v2_hal_deinit(void)
{
    v2_vpss_unload(&v2_vpss);
    v2_vi_unload(&v2_vi);
    v2_venc_unload(&v2_venc);
    v2_vb_unload(&v2_vb);
    v2_rgn_unload(&v2_rgn);
    v2_isp_unload(&v2_isp);
    v2_aud_unload(&v2_aud);
    v2_sys_unload(&v2_sys);
}

int v2_hal_init(void)
{
    int ret;

    if (ret = v2_sys_load(&v2_sys))
        return ret;
    if (ret = v2_aud_load(&v2_aud))
        return ret;
    if (ret = v2_isp_load(&v2_isp))
        return ret;
    if (ret = v2_rgn_load(&v2_rgn))
        return ret;
    if (ret = v2_vb_load(&v2_vb))
        return ret;
    if (ret = v2_venc_load(&v2_venc))
        return ret;
    if (ret = v2_vi_load(&v2_vi))
        return ret;
    if (ret = v2_vpss_load(&v2_vpss))
        return ret;

    return EXIT_SUCCESS;
}

void v2_audio_deinit(void)
{
    v2_aud.fnDisableChannel(_v2_aud_dev, _v2_aud_chn);

    v2_aud.fnDisableDevice(_v2_aud_dev);
}

int v2_audio_init(int samplerate)
{
    int ret;

    {
        v2_aud_cnf config;
        config.rate = samplerate;
        config.bit = V2_AUD_BIT_16;
        config.intf = V2_AUD_INTF_I2S_MASTER;
        config.stereoOn = 0;
        config.expandOn = 0;
        config.frmNum = 30;
        config.packNumPerFrm = 320;
        config.chnNum = 1;
        config.syncRxClkOn = 0;
        if (ret = v2_aud.fnSetDeviceConfig(_v2_aud_dev, &config))
            return ret;
    }
    if (ret = v2_aud.fnEnableDevice(_v2_aud_dev))
        return ret;
    
    {
        v2_aud_para para;
        para.userFrmDepth = 30;
        if (ret = v2_aud.fnSetChannelParam(_v2_aud_dev, _v2_aud_chn, &para))
            return ret;
    }
    if (ret = v2_aud.fnEnableChannel(_v2_aud_dev, _v2_aud_chn))
        return ret;

    return EXIT_SUCCESS;
}

void *v2_audio_thread(void)
{
    int ret;

    v2_aud_frm frame;
    v2_aud_efrm echoFrame;
    memset(&frame, 0, sizeof(frame));
    memset(&echoFrame, 0, sizeof(echoFrame));

    while (keepRunning && audioOn) {
        if (ret = v2_aud.fnGetFrame(_v2_aud_dev, _v2_aud_chn, 
            &frame, &echoFrame, 128)) {
            HAL_WARNING("v2_aud", "Getting the frame failed "
                "with %#x!\n", ret);
            continue;
        }

        if (v2_aud_cb) {
            hal_audframe outFrame;
            outFrame.channelCnt = 1;
            outFrame.data[0] = frame.addr[0];
            outFrame.length[0] = frame.length;
            outFrame.seq = frame.sequence;
            outFrame.timestamp = frame.timestamp;
            (v2_aud_cb)(&outFrame);
        }

        if (ret = v2_aud.fnFreeFrame(_v2_aud_dev, _v2_aud_chn,
            &frame, &echoFrame)) {
            HAL_WARNING("v2_aud", "Releasing the frame failed"
                " with %#x!\n", ret);
        }
    }
    HAL_INFO("v2_aud", "Shutting down capture thread...\n");
}

int v2_channel_bind(char index)
{
    int ret;

    if (ret = v2_vpss.fnEnableChannel(_v2_vpss_grp, index))
        return ret;

    {
        v2_sys_bind source = { .module = V2_SYS_MOD_VPSS, 
            .device = _v2_vpss_grp, .channel = index };
        v2_sys_bind dest = { .module = V2_SYS_MOD_VENC,
            .device = _v2_venc_dev, .channel = index };
        if (ret = v2_sys.fnBind(&source, &dest))
            return ret;
    }

    return EXIT_SUCCESS;
}

int v2_channel_create(char index, short width, short height, char mirror, char flip, char framerate)
{
    int ret;

    {
        v2_vpss_chn channel;
        memset(&channel, 0, sizeof(channel));
        channel.srcFps = v2_config.isp.framerate;
        channel.dstFps = framerate;
        channel.mirror = mirror;
        channel.flip = flip;
        if (ret = v2_vpss.fnSetChannelConfig(_v2_vpss_grp, index, &channel))
            return ret;
    }
    {
        v2_vpss_mode mode;
        mode.userModeOn = 1;
        mode.dest.width = width;
        mode.dest.height = height;
        mode.doubleOn = 0;
        mode.pixFmt = V2_PIXFMT_YUV420SP;
        mode.compress = V2_COMPR_NONE;
        if (ret = v2_vpss.fnSetChannelMode(_v2_vpss_grp, index, &mode))
            return ret;
    }

    return EXIT_SUCCESS;
}

int v2_channel_grayscale(char enable)
{
    int ret;
    int active = enable;

    for (char i = 0; i < V2_VENC_CHN_NUM; i++)
        if (v2_state[i].enable)
            if (ret = v2_venc.fnSetColorToGray(i, &active))
                return ret;

    return EXIT_SUCCESS;
}

int v2_channel_unbind(char index)
{
    int ret;

    if (ret = v2_vpss.fnDisableChannel(_v2_vpss_grp, index))
        return ret;

    {
        v2_sys_bind source = { .module = V2_SYS_MOD_VPSS, 
            .device = _v2_vpss_grp, .channel = index };
        v2_sys_bind dest = { .module = V2_SYS_MOD_VENC,
            .device = _v2_venc_dev, .channel = index };
        if (ret = v2_sys.fnUnbind(&source, &dest))
            return ret;
    }

    return EXIT_SUCCESS;
}

void *v2_image_thread(void)
{
    int ret;

    if (ret = v2_isp.fnRun(_v2_isp_dev))
        HAL_DANGER("v2_isp", "Operation failed with %#x!\n", ret);
    HAL_INFO("v2_isp", "Shutting down ISP thread...\n");
}

int v2_pipeline_create(void)
{
    int ret;

    if (ret = v2_sensor_config())
        return ret;

    if (ret = v2_vi.fnSetDeviceConfig(_v2_vi_dev, &v2_config.videv))
        return ret;
    if (ret = v2_vi.fnEnableDevice(_v2_vi_dev))
        return ret;

    {
        v2_vi_chn channel;
        channel.capt.width = v2_config.vichn.capt.width ? 
            v2_config.vichn.capt.width : v2_config.videv.rect.width;
        channel.capt.height = v2_config.vichn.capt.height ? 
            v2_config.vichn.capt.height : v2_config.videv.rect.height;
        channel.capt.x = 0;
        channel.capt.y = 0;
        channel.dest.width = v2_config.vichn.dest.width ? 
            v2_config.vichn.dest.width : v2_config.videv.rect.width;
        channel.dest.height = v2_config.vichn.dest.height ? 
            v2_config.vichn.dest.height : v2_config.videv.rect.height;
        channel.field = v2_config.vichn.field;
        channel.pixFmt = V2_PIXFMT_YUV420SP;
        channel.compress = V2_COMPR_NONE;
        channel.mirror = 0;
        channel.flip = 0;
        channel.srcFps = -1;
        channel.dstFps = -1;
        if (ret = v2_vi.fnSetChannelConfig(_v2_vi_chn, &channel))
            return ret;
    }
    if (ret = v2_vi.fnEnableChannel(_v2_vi_chn))
        return ret;

    if (ret = v2_snr_drv.fnRegisterCallback())
        return ret;
    
    if (ret = v2_isp.fnRegisterAE(_v2_vi_dev, &v2_ae_lib))
        return ret;
    if (ret = v2_isp.fnRegisterAWB(_v2_vi_dev, &v2_awb_lib))
        return ret;
    if (ret = v2_isp.fnMemInit(_v2_vi_dev))
        return ret;

    if (ret = v2_isp.fnSetWDRMode(_v2_vi_dev, &v2_config.mode))
        return ret;
    if (ret = v2_isp.fnSetDeviceConfig(_v2_vi_dev, &v2_config.isp))
        return ret;
    if (ret = v2_isp.fnInit(_v2_vi_dev))
        return ret;
    
    {
        v2_vpss_grp group;
        memset(&group, 0, sizeof(group));
        group.dest.width = v2_config.vichn.capt.width ? 
            v2_config.vichn.capt.width : v2_config.videv.rect.width;
        group.dest.height = v2_config.vichn.capt.height ? 
            v2_config.vichn.capt.height : v2_config.videv.rect.height;
        group.pixFmt = V2_PIXFMT_YUV420SP;
        group.nredOn = 1;
        group.interlMode = v2_config.videv.progressiveOn ? 1 : 2;
        if (ret = v2_vpss.fnCreateGroup(_v2_vpss_grp, &group))
            return ret;
    }
    if (ret = v2_vpss.fnStartGroup(_v2_vpss_grp))
        return ret;

    {
        v2_sys_bind source = { .module = V2_SYS_MOD_VIU, 
            .device = _v2_vi_dev, .channel = _v2_vi_chn };
        v2_sys_bind dest = { .module = V2_SYS_MOD_VPSS, 
            .device = _v2_vpss_grp, .channel = 0 };
        if (ret = v2_sys.fnBind(&source, &dest))
            return ret;
    }

    return EXIT_SUCCESS;
}

void v2_pipeline_destroy(void)
{
    v2_isp.fnExit(_v2_vi_dev);
    v2_isp.fnUnregisterAWB(_v2_vi_dev, &v2_awb_lib);
    v2_isp.fnUnregisterAE(_v2_vi_dev, &v2_ae_lib);

    v2_snr_drv.fnUnRegisterCallback();

    for (char grp = 0; grp < V2_VPSS_GRP_NUM; grp++)
    {
        for (char chn = 0; chn < V2_VPSS_CHN_NUM; chn++)
            v2_vpss.fnDisableChannel(grp, chn);

        {
            v2_sys_bind source = { .module = V2_SYS_MOD_VIU, 
                .device = _v2_vi_dev, .channel = _v2_vi_chn };
            v2_sys_bind dest = { .module = V2_SYS_MOD_VPSS,
                .device = grp, .channel = 0 };
            v2_sys.fnUnbind(&source, &dest);
        }

        v2_vpss.fnStopGroup(grp);
        v2_vpss.fnDestroyGroup(grp);
    }
    
    v2_vi.fnDisableChannel(_v2_vi_chn);

    v2_vi.fnDisableDevice(_v2_vi_dev);
}

int v2_region_create(char handle, hal_rect rect, short opacity)
{
    int ret;

    v2_sys_bind dest = { .module = V2_SYS_MOD_VENC, .device = _v2_venc_dev };
    v2_rgn_cnf region, regionCurr;
    v2_rgn_chn attrib, attribCurr;

    rect.height += rect.height & 1;
    rect.width += rect.width & 1;

    memset(&region, 0, sizeof(region));
    region.type = V2_RGN_TYPE_OVERLAY;
    region.overlay.pixFmt = V2_PIXFMT_ARGB1555;
    region.overlay.size.width = rect.width;
    region.overlay.size.height = rect.height;
    region.overlay.canvas = handle + 1;

    if (v2_rgn.fnGetRegionConfig(handle, &regionCurr)) {
        HAL_INFO("v2_rgn", "Creating region %d...\n", handle);
        if (ret = v2_rgn.fnCreateRegion(handle, &region))
            return ret;
    } else if (regionCurr.type != region.type ||
        regionCurr.overlay.size.height != region.overlay.size.height || 
        regionCurr.overlay.size.width != region.overlay.size.width) {
        HAL_INFO("v2_rgn", "Parameters are different, recreating "
            "region %d...\n", handle);
        v2_rgn.fnDetachChannel(handle, &dest);
        v2_rgn.fnDestroyRegion(handle);
        if (ret = v2_rgn.fnCreateRegion(handle, &region))
            return ret;
    }

    if (v2_rgn.fnGetChannelConfig(handle, &dest, &attribCurr))
        HAL_INFO("v2_rgn", "Attaching region %d...\n", handle);
    else if (attribCurr.overlay.point.x != rect.x || attribCurr.overlay.point.x != rect.y) {
        HAL_INFO("v2_rgn", "Position has changed, reattaching "
            "region %d...\n", handle);
        v2_rgn.fnDetachChannel(handle, &dest);
    }

    memset(&attrib, 0, sizeof(attrib));
    attrib.show = 1;
    attrib.type = V2_RGN_TYPE_OVERLAY;
    attrib.overlay.bgAlpha = 0;
    attrib.overlay.fgAlpha = opacity >> 1;
    attrib.overlay.point.x = rect.x;
    attrib.overlay.point.y = rect.y;
    attrib.overlay.layer = 7;

    v2_rgn.fnAttachChannel(handle, &dest, &attrib);

    return EXIT_SUCCESS;
}

void v2_region_destroy(char handle)
{
    v2_sys_bind dest = { .module = V2_SYS_MOD_VENC, .device = _v2_venc_dev };
    
    v2_rgn.fnDetachChannel(handle, &dest);
    v2_rgn.fnDestroyRegion(handle);
}

int v2_region_setbitmap(int handle, hal_bitmap *bitmap)
{
    v2_rgn_bmp nativeBmp = { .data = bitmap->data, .pixFmt = V2_PIXFMT_ARGB1555,
        .size.height = bitmap->dim.height, .size.width = bitmap->dim.width };

    return v2_rgn.fnSetBitmap(handle, &nativeBmp);
}

int v2_sensor_config(void) {
    int fd;
    v2_snr_dev config;
    memset(&config, 0, sizeof(config));
    config.input = v2_config.input_mode;
    if (config.input == V2_SNR_INPUT_MIPI)
        memcpy(&config.mipi, &v2_config.mipi, sizeof(v2_snr_mipi));
    else if (config.input == V2_SNR_INPUT_LVDS)
        memcpy(&config.lvds, &v2_config.lvds, sizeof(v2_snr_lvds));

    if (!access(v2_snr_endp, F_OK))
        fd = open(v2_snr_endp, O_RDWR);
    else
        HAL_ERROR("v2_snr", "Imaging device doesn't exist, "
            "maybe a kernel module is missing?\n");
    if (fd < 0)
        HAL_ERROR("v2_snr", "Opening imaging device has failed!\n");

    if (ioctl(fd, _IOW(V2_SNR_IOC_MAGIC, V2_SNR_CMD_CONF_DEV, v2_snr_dev), &config))
        HAL_ERROR("v2_snr", "Configuring imaging device has failed!\n");

    close(fd);

    return EXIT_SUCCESS;
}

void v2_sensor_deinit(void)
{
    dlclose(v2_snr_drv.handle);
    v2_snr_drv.handle = NULL;
}

int v2_sensor_init(char *name, char *obj)
{
    char path[128];
    char* dirs[] = {"%s", "./%s", "/usr/lib/sensors/%s", NULL};
    char **dir = dirs;

    while (*dir) {
        sprintf(path, *dir++, name);
        if (v2_snr_drv.handle = dlopen(path, RTLD_NOW | RTLD_GLOBAL))
            break;
    } if (!v2_snr_drv.handle)
        HAL_ERROR("v2_snr", "Failed to load the sensor driver");
    
    if (!(v2_snr_drv.fnRegisterCallback = (int(*)(void))dlsym(v2_snr_drv.handle, "sensor_register_callback")))
        HAL_ERROR("v2_snr", "Failed to connect the callback register function");
    if (!(v2_snr_drv.fnUnRegisterCallback = (int(*)(void))dlsym(v2_snr_drv.handle, "sensor_unregister_callback")))
        HAL_ERROR("v2_snr", "Failed to connect the callback unregister function");

    return EXIT_SUCCESS;
}

int v2_video_create(char index, hal_vidconfig *config)
{
    int ret;
    v2_venc_chn channel;
    v2_venc_attr_h26x *attrib;
    memset(&channel, 0, sizeof(channel));

    if (config->codec == HAL_VIDCODEC_JPG) {
        channel.attrib.codec = V2_VENC_CODEC_JPEG;
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
        channel.attrib.codec = V2_VENC_CODEC_MJPG;
        channel.attrib.mjpg.maxPic.width = config->width;
        channel.attrib.mjpg.maxPic.height = config->height;
        channel.attrib.mjpg.bufSize =
            ALIGN_UP(config->height, 16) * ALIGN_UP(config->width, 16);
        channel.attrib.mjpg.byFrame = 1;
        channel.attrib.mjpg.pic.width = config->width;
        channel.attrib.mjpg.pic.height = config->height;
        switch (config->mode) {
            case HAL_VIDMODE_CBR:
                channel.rate.mode = V2_VENC_RATEMODE_MJPGCBR;
                channel.rate.mjpgCbr = (v2_venc_rate_mjpgcbr){ .statTime = 1, .srcFps = config->framerate,
                    .dstFps = config->framerate, .bitrate = config->bitrate, .avgLvl = 1 }; break;
            case HAL_VIDMODE_VBR:
                channel.rate.mode = V2_VENC_RATEMODE_MJPGVBR;
                channel.rate.mjpgVbr = (v2_venc_rate_mjpgvbr){ .statTime = 1, .srcFps = config->framerate,
                    .dstFps = config->framerate , .maxBitrate = MAX(config->bitrate, config->maxBitrate), 
                    .maxQual = config->maxQual, .minQual = config->maxQual }; break;
            case HAL_VIDMODE_QP:
                channel.rate.mode = V2_VENC_RATEMODE_MJPGQP;
                channel.rate.mjpgQp = (v2_venc_rate_mjpgqp){ .srcFps = config->framerate,
                    .dstFps = config->framerate, .quality = config->maxQual }; break;
            default:
                HAL_ERROR("v2_venc", "MJPEG encoder can only support CBR, VBR or fixed QP modes!");
        }
        goto attach;
    } else if (config->codec == HAL_VIDCODEC_H265) {
        channel.attrib.codec = V2_VENC_CODEC_H265;
        attrib = &channel.attrib.h265;
        switch (config->mode) {
            case HAL_VIDMODE_CBR:
                channel.rate.mode = V2_VENC_RATEMODE_H265CBR;
                channel.rate.h265Cbr = (v2_venc_rate_h26xcbr){ .gop = config->gop,
                    .statTime = 1, .srcFps = config->framerate, .dstFps = config->framerate,
                    .bitrate = config->bitrate, .avgLvl = 1 }; break;
            case HAL_VIDMODE_VBR:
                channel.rate.mode = V2_VENC_RATEMODE_H265VBR;
                channel.rate.h265Vbr = (v2_venc_rate_h26xvbr){ .gop = config->gop,
                    .statTime = 1, .srcFps = config->framerate, .dstFps = config->framerate, 
                    .maxBitrate = MAX(config->bitrate, config->maxBitrate), .maxQual = config->maxQual,
                    .minQual = config->minQual }; break;
            case HAL_VIDMODE_QP:
                channel.rate.mode = V2_VENC_RATEMODE_H265QP;
                channel.rate.h265Qp = (v2_venc_rate_h26xqp){ .gop = config->gop,
                    .srcFps = config->framerate, .dstFps = config->framerate, .interQual = config->maxQual, 
                    .predQual = config->minQual }; break;
            case HAL_VIDMODE_AVBR:
                channel.rate.mode = V2_VENC_RATEMODE_H265AVBR;
                channel.rate.h265Avbr = (v2_venc_rate_h26xavbr){ .gop = config->gop,
                    .statTime = 1, .srcFps = config->framerate, .dstFps = config->framerate,
                    .maxBitrate = config->bitrate }; break;
            default:
                HAL_ERROR("v2_venc", "H.265 encoder does not support this mode!");
        }
    } else if (config->codec == HAL_VIDCODEC_H264) {
        channel.attrib.codec = V2_VENC_CODEC_H264;
        attrib = &channel.attrib.h264;
        switch (config->mode) {
            case HAL_VIDMODE_CBR:
                channel.rate.mode = V2_VENC_RATEMODE_H264CBR;
                channel.rate.h264Cbr = (v2_venc_rate_h26xcbr){ .gop = config->gop,
                    .statTime = 1, .srcFps = config->framerate, .dstFps = config->framerate,
                    .bitrate = config->bitrate, .avgLvl = 1 }; break;
            case HAL_VIDMODE_VBR:
                channel.rate.mode = V2_VENC_RATEMODE_H264VBR;
                channel.rate.h264Vbr = (v2_venc_rate_h26xvbr){ .gop = config->gop,
                    .statTime = 1, .srcFps = config->framerate, .dstFps = config->framerate, 
                    .maxBitrate = MAX(config->bitrate, config->maxBitrate), .maxQual = config->maxQual,
                    .minQual = config->minQual }; break;
            case HAL_VIDMODE_QP:
                channel.rate.mode = V2_VENC_RATEMODE_H264QP;
                channel.rate.h264Qp = (v2_venc_rate_h26xqp){ .gop = config->gop,
                    .srcFps = config->framerate, .dstFps = config->framerate, .interQual = config->maxQual, 
                    .predQual = config->minQual }; break;
            case HAL_VIDMODE_AVBR:
                channel.rate.mode = V2_VENC_RATEMODE_H264AVBR;
                channel.rate.h264Avbr = (v2_venc_rate_h26xavbr){ .gop = config->gop,
                    .statTime = 1, .srcFps = config->framerate, .dstFps = config->framerate,
                    .maxBitrate = config->bitrate }; break;
            default:
                HAL_ERROR("v2_venc", "H.264 encoder does not support this mode!");
        }
    } else HAL_ERROR("v2_venc", "This codec is not supported by the hardware!");
    attrib->maxPic.width = config->width;
    attrib->maxPic.height = config->height;
    attrib->bufSize = config->height * config->width;
    attrib->profile = config->profile;
    attrib->byFrame = 1;
    attrib->pic.width = config->width;
    attrib->pic.height = config->height;
    attrib->bFrameNum = 0;
    attrib->refNum = 1;
attach:
    if (ret = v2_venc.fnCreateChannel(index, &channel))
        return ret;

    if (config->codec != HAL_VIDCODEC_JPG && 
        (ret = v2_venc.fnStartReceiving(index)))
        return ret;

    v2_state[index].payload = config->codec;

    return EXIT_SUCCESS;
}

int v2_video_destroy(char index)
{
    int ret;

    v2_state[index].enable = 0;
    v2_state[index].payload = HAL_VIDCODEC_UNSPEC;

    v2_venc.fnStopReceiving(index);

    {
        v2_sys_bind source = { .module = V2_SYS_MOD_VPSS, 
            .device = _v2_vpss_grp, .channel = index };
        v2_sys_bind dest = { .module = V2_SYS_MOD_VENC,
            .device = _v2_venc_dev, .channel = index };
        if (ret = v2_sys.fnUnbind(&source, &dest))
            return ret;
    }

    if (ret = v2_venc.fnDestroyChannel(index))
        return ret;
    
    if (ret = v2_vpss.fnDisableChannel(_v2_vpss_grp, index))
        return ret;

    return EXIT_SUCCESS;
}
    
int v2_video_destroy_all(void)
{
    int ret;

    for (char i = 0; i < V2_VENC_CHN_NUM; i++)
        if (v2_state[i].enable)
            if (ret = v2_video_destroy(i))
                return ret;

    return EXIT_SUCCESS;
}

void v2_video_request_idr(char index)
{
    v2_venc.fnRequestIdr(index, 1);
}

int v2_video_snapshot_grab(char index, hal_jpegdata *jpeg)
{
    int ret;

    if (ret = v2_channel_bind(index)) {
        HAL_DANGER("v2_venc", "Binding the encoder channel "
            "%d failed with %#x!\n", index, ret);
        goto abort;
    }

    unsigned int count = 1;
    if (ret = v2_venc.fnStartReceivingEx(index, &count)) {
        HAL_DANGER("v2_venc", "Requesting one frame "
            "%d failed with %#x!\n", index, ret);
        goto abort;
    }

    int fd = v2_venc.fnGetDescriptor(index);

    struct timeval timeout = { .tv_sec = 2, .tv_usec = 0 };
    fd_set readFds;
    FD_ZERO(&readFds);
    FD_SET(fd, &readFds);
    ret = select(fd + 1, &readFds, NULL, NULL, &timeout);
    if (ret < 0) {
        HAL_DANGER("v2_venc", "Select operation failed!\n");
        goto abort;
    } else if (ret == 0) {
        HAL_DANGER("v2_venc", "Capture stream timed out!\n");
        goto abort;
    }

    if (FD_ISSET(fd, &readFds)) {
        v2_venc_stat stat;
        if (ret = v2_venc.fnQuery(index, &stat)) {
            HAL_DANGER("v2_venc", "Querying the encoder channel "
                "%d failed with %#x!\n", index, ret);
            goto abort;
        }

        if (!stat.curPacks) {
            HAL_DANGER("v2_venc", "Current frame is empty, skipping it!\n");
            goto abort;
        }

        v2_venc_strm strm;
        memset(&strm, 0, sizeof(strm));
        strm.packet = (v2_venc_pack*)malloc(sizeof(v2_venc_pack) * stat.curPacks);
        if (!strm.packet) {
            HAL_DANGER("v2_venc", "Memory allocation on channel %d failed!\n", index);
            goto abort;
        }
        strm.count = stat.curPacks;

        if (ret = v2_venc.fnGetStream(index, &strm, stat.curPacks)) {
            HAL_DANGER("v2_venc", "Getting the stream on "
                "channel %d failed with %#x!\n", index, ret);
            free(strm.packet);
            strm.packet = NULL;
            goto abort;
        }

        {
            jpeg->jpegSize = 0;
            for (unsigned int i = 0; i < strm.count; i++) {
                v2_venc_pack *pack = &strm.packet[i];
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
        v2_venc.fnFreeStream(index, &strm);
    }

    v2_venc.fnFreeDescriptor(index);

    v2_venc.fnStopReceiving(index);

    v2_channel_unbind(index);

    return ret;
}

void *v2_video_thread(void)
{
    int ret, maxFd = 0;

    for (int i = 0; i < V2_VENC_CHN_NUM; i++) {
        if (!v2_state[i].enable) continue;
        if (!v2_state[i].mainLoop) continue;

        ret = v2_venc.fnGetDescriptor(i);
        if (ret < 0) {
            HAL_DANGER("v2_venc", "Getting the encoder descriptor failed with %#x!\n", ret);
            return (void*)0;
        }
        v2_state[i].fileDesc = ret;

        if (maxFd <= v2_state[i].fileDesc)
            maxFd = v2_state[i].fileDesc;
    }

    v2_venc_stat stat;
    v2_venc_strm stream;
    struct timeval timeout;
    fd_set readFds;

    while (keepRunning) {
        FD_ZERO(&readFds);
        for(int i = 0; i < V2_VENC_CHN_NUM; i++) {
            if (!v2_state[i].enable) continue;
            if (!v2_state[i].mainLoop) continue;
            FD_SET(v2_state[i].fileDesc, &readFds);
        }

        timeout.tv_sec = 2;
        timeout.tv_usec = 0;
        ret = select(maxFd + 1, &readFds, NULL, NULL, &timeout);
        if (ret < 0) {
            HAL_DANGER("v2_venc", "Select operation failed!\n");
            break;
        } else if (ret == 0) {
            HAL_WARNING("v2_venc", "Main stream loop timed out!\n");
            continue;
        } else {
            for (int i = 0; i < V2_VENC_CHN_NUM; i++) {
                if (!v2_state[i].enable) continue;
                if (!v2_state[i].mainLoop) continue;
                if (FD_ISSET(v2_state[i].fileDesc, &readFds)) {
                    memset(&stream, 0, sizeof(stream));
                    
                    if (ret = v2_venc.fnQuery(i, &stat)) {
                        HAL_DANGER("v2_venc", "Querying the encoder channel "
                            "%d failed with %#x!\n", i, ret);
                        break;
                    }

                    if (!stat.curPacks) {
                        HAL_WARNING("v2_venc", " Current frame is empty, skipping it!\n");
                        continue;
                    }

                    stream.packet = (v2_venc_pack*)malloc(
                        sizeof(v2_venc_pack) * stat.curPacks);
                    if (!stream.packet) {
                        HAL_DANGER("v2_venc", "Memory allocation on channel %d failed!\n", i);
                        break;
                    }
                    stream.count = stat.curPacks;

                    if (ret = v2_venc.fnGetStream(i, &stream, 40)) {
                        HAL_DANGER("v2_venc", "Getting the stream on "
                            "channel %d failed with %#x!\n", i, ret);
                        break;
                    }

                    if (v2_vid_cb) {
                        hal_vidstream outStrm;
                        hal_vidpack outPack[stream.count];
                        outStrm.count = stream.count;
                        outStrm.seq = stream.sequence;
                        for (int j = 0; j < stream.count; j++) {
                            v2_venc_pack *pack = &stream.packet[j];
                            outPack[j].data = pack->data;
                            outPack[j].length = pack->length;
                            outPack[j].naluCnt = 1;
                            outPack[j].nalu[0].length = pack->length;
                            outPack[j].nalu[0].offset = pack->offset;
                            switch (v2_state[i].payload) {
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
                        (*v2_vid_cb)(i, &outStrm);
                    }

                    if (ret = v2_venc.fnFreeStream(i, &stream)) {
                        HAL_WARNING("v2_venc", "Releasing the stream on "
                            "channel %d failed with %#x!\n", i, ret);
                    }
                    free(stream.packet);
                    stream.packet = NULL;
                }
            }
        }
    }

    HAL_INFO("v2_venc", "Shutting down encoding thread...\n");
}

void v2_system_deinit(void)
{
    v2_sys.fnExit();
    v2_vb.fnExit();

    v2_sensor_deinit();
}

int v2_system_init(char *snrConfig)
{
    int ret;

    printf("App built with headers v%s\n", V2_SYS_API);

    {
        v2_sys_ver version;
        v2_sys.fnGetVersion(&version);
        puts(version.version);
    }

    if (v2_parse_sensor_config(snrConfig, &v2_config) != CONFIG_OK)
        HAL_ERROR("v2_sys", "Can't load sensor config\n");

    if (ret = v2_sensor_init(v2_config.dll_file, v2_config.sensor_type))
        return ret;

    v2_sys.fnExit();
    v2_vb.fnExit();

    {
        int alignWidth = 16;
        v2_vb_pool pool;

        memset(&pool, 0, sizeof(pool)); 
        
        pool.count = 1;
        pool.comm[0].blockSize = v2_buffer_calculate_venc(
            v2_config.vichn.capt.width ? 
                v2_config.vichn.capt.width : v2_config.videv.rect.width,
            v2_config.vichn.capt.height ? 
                v2_config.vichn.capt.height : v2_config.videv.rect.height,
            V2_PIXFMT_YUV420SP, alignWidth);
        pool.comm[0].blockCnt = 4;

        if (ret = v2_vb.fnConfigPool(&pool))
            return ret;
        v2_vb_supl supl = V2_VB_JPEG_MASK;
        if (ret = v2_vb.fnConfigSupplement(&supl))
            return ret;
        if (ret = v2_vb.fnInit())
            return ret;
        
        if (ret = v2_sys.fnSetAlignment(&alignWidth))
            return ret;
    }
    if (ret = v2_sys.fnInit())
        return ret;

    return EXIT_SUCCESS;
}

float v2_system_readtemp(void)
{
    int val, prep = 0x60fa0000;
    float result = 0.0 / 0.0;

    if (hal_registry(0x20270110, &val, OP_READ) && prep != val)
        hal_registry(0x20270110, &prep, OP_WRITE);
    
    if (!hal_registry(0x20270114, &val, OP_READ))
        return result;

    result = val & ((1 << 8) - 1);
    return ((result * 180) / 256) - 40;
}

#endif