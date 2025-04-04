#ifdef __arm__

#include "v1_hal.h"

v1_isp_alg      v1_ae_lib = { .id = 0, .libName = "hisi_ae_lib" };
v1_aud_impl     v1_aud;
v1_isp_alg      v1_awb_lib = { .id = 0, .libName = "hisi_awb_lib" };
v1_config_impl  v1_config;
v1_isp_impl     v1_isp;
v1_snr_drv_impl v1_snr_drv;
v1_rgn_impl     v1_rgn;
v1_sys_impl     v1_sys;
v1_vb_impl      v1_vb;
v1_venc_impl    v1_venc;
v1_vi_impl      v1_vi;
v1_vpss_impl    v1_vpss;

hal_chnstate v1_state[V1_VENC_CHN_NUM] = {0};
int (*v1_aud_cb)(hal_audframe*);
int (*v1_vid_cb)(char, hal_vidstream*);

char _v1_aud_chn = 0;
char _v1_aud_dev = 0;
char _v1_isp_chn = 0;
char _v1_isp_dev = 0;
char _v1_venc_dev = 0;
char _v1_vi_chn = 0;
char _v1_vi_dev = 0;
char _v1_vpss_chn = 0;
char _v1_vpss_grp = 0;

void v1_hal_deinit(void)
{
    v1_vpss_unload(&v1_vpss);
    v1_vi_unload(&v1_vi);
    v1_venc_unload(&v1_venc);
    v1_vb_unload(&v1_vb);
    v1_rgn_unload(&v1_rgn);
    v1_isp_unload(&v1_isp);
    v1_aud_unload(&v1_aud);
    v1_sys_unload(&v1_sys);
}

int v1_hal_init(void)
{
    int ret;

    if (ret = v1_sys_load(&v1_sys))
        return ret;
    if (ret = v1_aud_load(&v1_aud))
        return ret;
    if (ret = v1_isp_load(&v1_isp))
        return ret;
    if (ret = v1_rgn_load(&v1_rgn))
        return ret;
    if (ret = v1_vb_load(&v1_vb))
        return ret;
    if (ret = v1_venc_load(&v1_venc))
        return ret;
    if (ret = v1_vi_load(&v1_vi))
        return ret;
    if (ret = v1_vpss_load(&v1_vpss))
        return ret;

    return EXIT_SUCCESS;
}

void v1_audio_deinit(void)
{
    v1_aud.fnDisableChannel(_v1_aud_dev, _v1_aud_chn);

    v1_aud.fnDisableDevice(_v1_aud_dev);
}

int v1_audio_init(int samplerate)
{
    int ret;

    {
        v1_aud_cnf config;
        config.rate = samplerate;
        config.bit = V1_AUD_BIT_16;
        config.intf = V1_AUD_INTF_I2S_MASTER;
        config.stereoOn = 0;
        config.expandOn = 0;
        config.frmNum = 30;
        config.packNumPerFrm = 320;
        config.chnNum = 2;
        config.syncRxClkOn = 0;
        if (ret = v1_aud.fnSetDeviceConfig(_v1_aud_dev, &config))
            return ret;
    }
    if (ret = v1_aud.fnEnableDevice(_v1_aud_dev))
        return ret;

    if (ret = v1_aud.fnEnableChannel(_v1_aud_dev, _v1_aud_chn))
        return ret;

    return EXIT_SUCCESS;
}

void *v1_audio_thread(void)
{
    int ret;

    v1_aud_frm frame;
    v1_aud_efrm echoFrame;
    memset(&frame, 0, sizeof(frame));
    memset(&echoFrame, 0, sizeof(echoFrame));

    while (keepRunning && audioOn) {
        if (ret = v1_aud.fnGetFrame(_v1_aud_dev, _v1_aud_chn, 
            &frame, &echoFrame, 128)) {
            HAL_WARNING("v1_aud", "Getting the frame failed "
                "with %#x!\n", ret);
            continue;
        }

        if (v1_aud_cb) {
            hal_audframe outFrame;
            outFrame.channelCnt = 1;
            outFrame.data[0] = frame.addr[0];
            outFrame.length[0] = frame.length;
            outFrame.seq = frame.sequence;
            outFrame.timestamp = frame.timestamp;
            (v1_aud_cb)(&outFrame);
        }

        if (ret = v1_aud.fnFreeFrame(_v1_aud_dev, _v1_aud_chn,
            &frame, &echoFrame)) {
            HAL_WARNING("v1_aud", "Releasing the frame failed"
                " with %#x!\n", ret);
        }
    }
    HAL_INFO("v1_aud", "Shutting down capture thread...\n");
}

int v1_channel_bind(char index)
{
    int ret;

    if (ret = v1_vpss.fnEnableChannel(_v1_vpss_grp, index))
        return ret;

    {
        v1_sys_bind source = { .module = V1_SYS_MOD_VPSS, 
            .device = _v1_vpss_grp, .channel = index };
        v1_sys_bind dest = { .module = V1_SYS_MOD_GROUP,
            .device = index, .channel = 0 };
        if (ret = v1_sys.fnBind(&source, &dest))
            return ret;
    }

    return EXIT_SUCCESS;
}

int v1_channel_create(char index, short width, short height, char mirror, char flip, char framerate)
{
    int ret;

    {
        v1_vpss_chn channel;
        memset(&channel, 0, sizeof(channel));
        channel.srcFps = v1_config.img.framerate;
        channel.dstFps = framerate;
        channel.mirror = mirror;
        channel.flip = flip;
        if (ret = v1_vpss.fnSetChannelConfig(_v1_vpss_grp, index, &channel))
            return ret;
    }
    {
        v1_vpss_mode mode;
        mode.userModeOn = 1;
        mode.dest.width = width;
        mode.dest.height = height;
        mode.doubleOn = 0;
        mode.pixFmt = V1_PIXFMT_YUV420SP;
        mode.compress = V1_COMPR_NONE;
        if (ret = v1_vpss.fnSetChannelMode(_v1_vpss_grp, index, &mode))
            return ret;
    }

    return EXIT_SUCCESS;
}

int v1_channel_grayscale(char enable)
{
    int ret;
    int active = enable;

    for (char i = 0; i < V1_VENC_CHN_NUM; i++)
        if (v1_state[i].enable)
            if (ret = v1_venc.fnSetColorToGray(i, &active))
                return ret;

    return EXIT_SUCCESS;
}

int v1_channel_unbind(char index)
{
    int ret;

    if (ret = v1_vpss.fnDisableChannel(_v1_vpss_grp, index))
        return ret;

    {
        v1_sys_bind source = { .module = V1_SYS_MOD_VPSS, 
            .device = _v1_vpss_grp, .channel = index };
        v1_sys_bind dest = { .module = V1_SYS_MOD_GROUP,
            .device = index, .channel = 0 };
        if (ret = v1_sys.fnUnbind(&source, &dest))
            return ret;
    }

    return EXIT_SUCCESS;
}

void *v1_image_thread(void)
{
    int ret;

    if (ret = v1_isp.fnRun())
        HAL_DANGER("v1_isp", "Operation failed with %#x!\n", ret);
    HAL_INFO("v1_isp", "Shutting down ISP thread...\n");
}

int v1_pipeline_create(void)
{
    int ret;

    if (ret = v1_snr_drv.fnInit())
        return ret;
    if (ret = v1_snr_drv.fnRegisterCallback())
        return ret;
    
    if (ret = v1_isp.fnRegisterAE(&v1_ae_lib))
        return ret;
    if (ret = v1_isp.fnRegisterAWB(&v1_awb_lib))
        return ret;

    if (ret = v1_isp.fnInit())
        return ret;
    if (ret = v1_isp.fnSetWDRMode(&v1_config.mode))
        return ret;
    if (ret = v1_isp.fnSetImageConfig(&v1_config.img))
        return ret;
    v1_config.tim.mode = V1_ISP_WIN_BOTH;
    if (ret = v1_isp.fnSetInputTiming(&v1_config.tim))
        return ret;
    
    if (ret = v1_vi.fnSetDeviceConfig(_v1_vi_dev, &v1_config.videv))
        return ret;
    if (ret = v1_vi.fnEnableDevice(_v1_vi_dev))
        return ret;

    {
        v1_vi_chn channel;
        channel.capt.width = v1_config.vichn.capt.width ? 
            v1_config.vichn.capt.width : v1_config.videv.rect.width;
        channel.capt.height = v1_config.vichn.capt.height ? 
            v1_config.vichn.capt.height : v1_config.videv.rect.height;
        channel.capt.x = 0;
        channel.capt.y = 0;
        channel.dest.width = v1_config.vichn.dest.width ? 
            v1_config.vichn.dest.width : v1_config.videv.rect.width;
        channel.dest.height = v1_config.vichn.dest.height ? 
            v1_config.vichn.dest.height : v1_config.videv.rect.height;
        channel.field = v1_config.vichn.field;
        channel.pixFmt = V1_PIXFMT_YUV420SP;
        channel.compress = V1_COMPR_NONE;
        channel.mirror = 0;
        channel.flip = 0;
        channel.srcFps = -1;
        channel.dstFps = -1;
        if (ret = v1_vi.fnSetChannelConfig(_v1_vi_chn, &channel))
            return ret;
    }
    if (ret = v1_vi.fnEnableChannel(_v1_vi_chn))
        return ret;

    {
        v1_vpss_grp group;
        memset(&group, 0, sizeof(group));
        group.dest.width = v1_config.vichn.capt.width ? 
            v1_config.vichn.capt.width : v1_config.videv.rect.width;
        group.dest.height = v1_config.vichn.capt.height ? 
            v1_config.vichn.capt.height : v1_config.videv.rect.height;
        group.pixFmt = V1_PIXFMT_YUV420SP;
        group.nredOn = 1;
        group.interlMode = v1_config.videv.progressiveOn ? 1 : 2;
        if (ret = v1_vpss.fnCreateGroup(_v1_vpss_grp, &group))
            return ret;
    }
    if (ret = v1_vpss.fnEnableChannel(_v1_vpss_grp, _v1_vpss_chn))
        return ret;
    if (ret = v1_vpss.fnStartGroup(_v1_vpss_grp))
        return ret;

    {
        v1_sys_bind source = { .module = V1_SYS_MOD_VIU, 
            .device = _v1_vi_dev, .channel = _v1_vi_chn };
        v1_sys_bind dest = { .module = V1_SYS_MOD_VPSS, 
            .device = _v1_vpss_grp, .channel = 0 };
        if (ret = v1_sys.fnBind(&source, &dest))
            return ret;
    }

    return EXIT_SUCCESS;
}

void v1_pipeline_destroy(void)
{
    v1_isp.fnExit();
    v1_isp.fnUnregisterAWB(&v1_awb_lib);
    v1_isp.fnUnregisterAE(&v1_ae_lib);

    v1_snr_drv.fnUnRegisterCallback();

    for (char grp = 0; grp < V1_VPSS_GRP_NUM; grp++)
    {
        for (char chn = 0; chn < V1_VPSS_CHN_NUM; chn++)
            v1_vpss.fnDisableChannel(grp, chn);

        {
            v1_sys_bind source = { .module = V1_SYS_MOD_VIU, 
                .device = _v1_vi_dev, .channel = _v1_vi_chn };
            v1_sys_bind dest = { .module = V1_SYS_MOD_VPSS,
                .device = grp, .channel = 0 };
            v1_sys.fnUnbind(&source, &dest);
        }

        v1_vpss.fnStopGroup(grp);
        v1_vpss.fnDisableChannel(grp, _v1_vpss_chn);
        v1_vpss.fnDestroyGroup(grp);
    }
    
    v1_vi.fnDisableChannel(_v1_vi_chn);

    v1_vi.fnDisableDevice(_v1_vi_dev);
}

int v1_region_create(char handle, hal_rect rect, short opacity)
{
    int ret;

    v1_sys_bind dest = { .module = V1_SYS_MOD_GROUP, .device = _v1_vpss_grp };
    v1_rgn_cnf region, regionCurr;
    v1_rgn_chn attrib, attribCurr;

    rect.height += rect.height & 1;
    rect.width += rect.width & 1;

    memset(&region, 0, sizeof(region));
    region.type = V1_RGN_TYPE_OVERLAY;
    region.overlay.pixFmt = V1_PIXFMT_ARGB1555;
    region.overlay.size.width = rect.width;
    region.overlay.size.height = rect.height;
    region.overlay.canvas = handle + 1;

    if (v1_rgn.fnGetRegionConfig(handle, &regionCurr)) {
        HAL_INFO("v1_rgn", "Creating region %d...\n", handle);
        if (ret = v1_rgn.fnCreateRegion(handle, &region))
            return ret;
    } else if (regionCurr.type != region.type ||
        regionCurr.overlay.size.height != region.overlay.size.height || 
        regionCurr.overlay.size.width != region.overlay.size.width) {
        HAL_INFO("v1_rgn", "Parameters are different, recreating "
            "region %d...\n", handle);
        v1_rgn.fnDetachChannel(handle, &dest);
        v1_rgn.fnDestroyRegion(handle);
        if (ret = v1_rgn.fnCreateRegion(handle, &region))
            return ret;
    }

    if (v1_rgn.fnGetChannelConfig(handle, &dest, &attribCurr))
        HAL_INFO("v1_rgn", "Attaching region %d...\n", handle);
    else if (attribCurr.overlay.point.x != rect.x || attribCurr.overlay.point.x != rect.y) {
        HAL_INFO("v1_rgn", "Position has changed, reattaching "
            "region %d...\n", handle);
        v1_rgn.fnDetachChannel(handle, &dest);
    }

    memset(&attrib, 0, sizeof(attrib));
    attrib.show = 1;
    attrib.type = V1_RGN_TYPE_OVERLAY;
    attrib.overlay.bgAlpha = 0;
    attrib.overlay.fgAlpha = opacity >> 1;
    attrib.overlay.point.x = rect.x;
    attrib.overlay.point.y = rect.y;
    attrib.overlay.layer = 7;

    v1_rgn.fnAttachChannel(handle, &dest, &attrib);

    return EXIT_SUCCESS;
}

void v1_region_destroy(char handle)
{
    v1_sys_bind dest = { .module = V1_SYS_MOD_GROUP, .device = _v1_vpss_grp };
    
    v1_rgn.fnDetachChannel(handle, &dest);
    v1_rgn.fnDestroyRegion(handle);
}

int v1_region_setbitmap(int handle, hal_bitmap *bitmap)
{
    v1_rgn_bmp nativeBmp = { .data = bitmap->data, .pixFmt = V1_PIXFMT_ARGB1555,
        .size.height = bitmap->dim.height, .size.width = bitmap->dim.width };

    return v1_rgn.fnSetBitmap(handle, &nativeBmp);
}

void v1_sensor_deinit(void)
{
    dlclose(v1_snr_drv.handle);
    v1_snr_drv.handle = NULL;
}

int v1_sensor_init(char *name, char *obj)
{
    char path[128];
    char* dirs[] = {"%s", "./%s", "/usr/lib/sensors/%s"};
    char **dir = dirs;

    while (*dir) {
        sprintf(path, *dir++, name);
        if (v1_snr_drv.handle = dlopen(path, RTLD_NOW | RTLD_GLOBAL))
            break;
    } if (!v1_snr_drv.handle)
        HAL_ERROR("v1_snr", "Failed to load the sensor driver");

    if (!(v1_snr_drv.fnInit = (int(*)(void))dlsym(v1_snr_drv.handle, "sensor_init")))
        HAL_ERROR("v1_snr", "Failed to connect the init function");
    if (!(v1_snr_drv.fnRegisterCallback = (int(*)(void))dlsym(v1_snr_drv.handle, "sensor_register_callback")))
        HAL_ERROR("v1_snr", "Failed to connect the callback register function");
    if (!(v1_snr_drv.fnUnRegisterCallback = (int(*)(void))dlsym(v1_snr_drv.handle, "sensor_unregister_callback")))
        HAL_ERROR("v1_snr", "Failed to connect the callback unregister function");

    return EXIT_SUCCESS;
}

int v1_video_create(char index, hal_vidconfig *config)
{
    int ret;
    v1_venc_chn channel;
    v1_venc_attr_h264 *attrib;
    memset(&channel, 0, sizeof(channel));

    if (config->codec == HAL_VIDCODEC_JPG) {
        channel.attrib.codec = V1_VENC_CODEC_JPEG;
        channel.attrib.jpg.maxPic.width = config->width;
        channel.attrib.jpg.maxPic.height = config->height;
        channel.attrib.jpg.bufSize =
            ALIGN_UP(config->height, 16) * ALIGN_UP(config->width, 16);
        channel.attrib.jpg.byFrame = 1;
        channel.attrib.jpg.fieldOrFrame = 0;
        channel.attrib.jpg.priority = 0;
        channel.attrib.jpg.pic.width = config->width;
        channel.attrib.jpg.pic.height = config->height;
        goto attach;
    } else if (config->codec == HAL_VIDCODEC_MJPG) {
        channel.attrib.codec = V1_VENC_CODEC_MJPG;
        channel.attrib.mjpg.maxPic.width = config->width;
        channel.attrib.mjpg.maxPic.height = config->height;
        channel.attrib.mjpg.bufSize = 
            ALIGN_UP(config->height, 16) * ALIGN_UP(config->width, 16);
        channel.attrib.mjpg.byFrame = 1;
        channel.attrib.mjpg.mainStrmOn = 1;
        channel.attrib.mjpg.fieldOrFrame = 0;
        channel.attrib.mjpg.priority = 0;
        channel.attrib.mjpg.pic.width = config->width;
        channel.attrib.mjpg.pic.height = config->height;
        switch (config->mode) {
            case HAL_VIDMODE_CBR:
                channel.rate.mode = V1_VENC_RATEMODE_MJPGCBR;
                channel.rate.mjpgCbr = (v1_venc_rate_mjpgcbr){ .statTime = 1, .srcFps = config->framerate,
                    .dstFps = config->framerate, .bitrate = config->bitrate, .avgLvl = 0 }; break;
            case HAL_VIDMODE_VBR:
                channel.rate.mode = V1_VENC_RATEMODE_MJPGVBR;
                channel.rate.mjpgVbr = (v1_venc_rate_mjpgvbr){ .statTime = 1, .srcFps = config->framerate,
                    .dstFps = config->framerate , .maxBitrate = MAX(config->bitrate, config->maxBitrate), 
                    .maxQual = config->maxQual, .minQual = config->maxQual }; break;
            case HAL_VIDMODE_QP:
                channel.rate.mode = V1_VENC_RATEMODE_MJPGQP;
                channel.rate.mjpgQp = (v1_venc_rate_mjpgqp){ .srcFps = config->framerate,
                    .dstFps = config->framerate, .quality = config->maxQual }; break;
            default:
                HAL_ERROR("v1_venc", "MJPEG encoder can only support CBR, VBR or fixed QP modes!");
        }
        goto attach;
    } else if (config->codec == HAL_VIDCODEC_H264) {
        channel.attrib.codec = V1_VENC_CODEC_H264;
        attrib = &channel.attrib.h264;
        switch (config->mode) {
            case HAL_VIDMODE_CBR:
                channel.rate.mode = V1_VENC_RATEMODE_H264CBRv2;
                channel.rate.h264Cbr = (v1_venc_rate_h264cbr){ .gop = config->gop,
                    .statTime = 1, .srcFps = config->framerate, .dstFps = config->framerate,
                    .bitrate = config->bitrate, .avgLvl = 0 }; break;
            case HAL_VIDMODE_VBR:
                channel.rate.mode = V1_VENC_RATEMODE_H264VBRv2;
                channel.rate.h264Vbr = (v1_venc_rate_h264vbr){ .gop = config->gop,
                    .statTime = 1, .srcFps = config->framerate, .dstFps = config->framerate, 
                    .maxBitrate = MAX(config->bitrate, config->maxBitrate), .maxQual = config->maxQual,
                    .minQual = config->minQual }; break;
            case HAL_VIDMODE_QP:
                channel.rate.mode = V1_VENC_RATEMODE_H264QP;
                channel.rate.h264Qp = (v1_venc_rate_h264qp){ .gop = config->gop,
                    .srcFps = config->framerate, .dstFps = config->framerate, .interQual = config->maxQual, 
                    .predQual = config->minQual }; break;
            default:
                HAL_ERROR("v1_venc", "H.264 encoder does not support this mode!");
        }
    } else HAL_ERROR("v1_venc", "This codec is not supported by the hardware!");
    attrib->maxPic.width = config->width;
    attrib->maxPic.height = config->height;
    attrib->bufSize = config->height * config->width;
    attrib->profile = MIN(config->profile, 1);
    attrib->byFrame = 1;
    attrib->fieldOn = 0;
    attrib->mainStrmOn = 1;
    attrib->priority = 0;
    attrib->fieldOrFrame = 0;
    attrib->pic.width = config->width;
    attrib->pic.height = config->height;
attach:
    if (ret = v1_venc.fnCreateGroup(index))
        return ret;

    if (ret = v1_venc.fnCreateChannel(index, &channel))
        return ret;

    if (ret = v1_venc.fnRegisterChannel(index, index))
        return ret;

    if (config->codec != HAL_VIDCODEC_JPG && 
        (ret = v1_venc.fnStartReceiving(index)))
        return ret;

    v1_state[index].payload = config->codec;

    return EXIT_SUCCESS;
}

int v1_video_destroy(char index)
{
    int ret;

    v1_state[index].enable = 0;
    v1_state[index].payload = HAL_VIDCODEC_UNSPEC;

    v1_venc.fnStopReceiving(index);

    {
        v1_sys_bind source = { .module = V1_SYS_MOD_VPSS, 
            .device = _v1_vpss_grp, .channel = index };
        v1_sys_bind dest = { .module = V1_SYS_MOD_GROUP,
            .device = index, .channel = 0 };
        if (ret = v1_sys.fnUnbind(&source, &dest))
            return ret;
    }

    if (ret = v1_venc.fnUnregisterChannel(index))
        return ret;

    if (ret = v1_venc.fnDestroyChannel(index))
        return ret;

    if (ret = v1_venc.fnDestroyGroup(index))
        return ret;
    
    if (ret = v1_vpss.fnDisableChannel(_v1_vpss_grp, index))
        return ret;

    return EXIT_SUCCESS;
}
    
int v1_video_destroy_all(void)
{
    int ret;

    for (char i = 0; i < V1_VENC_CHN_NUM; i++)
        if (v1_state[i].enable)
            if (ret = v1_video_destroy(i))
                return ret;

    return EXIT_SUCCESS;
}

void v1_video_request_idr(char index)
{
    v1_venc.fnRequestIdr(index, 1);
}

int v1_video_snapshot_grab(char index, hal_jpegdata *jpeg)
{
    int ret;

    if (ret = v1_channel_bind(index)) {
        HAL_DANGER("v1_venc", "Binding the encoder channel "
            "%d failed with %#x!\n", index, ret);
        goto abort;
    }

    unsigned int count = 1;
    if (ret = v1_venc.fnStartReceivingEx(index, &count)) {
        HAL_DANGER("v1_venc", "Requesting one frame "
            "%d failed with %#x!\n", index, ret);
        goto abort;
    }

    int fd = v1_venc.fnGetDescriptor(index);

    struct timeval timeout = { .tv_sec = 2, .tv_usec = 0 };
    fd_set readFds;
    FD_ZERO(&readFds);
    FD_SET(fd, &readFds);
    ret = select(fd + 1, &readFds, NULL, NULL, &timeout);
    if (ret < 0) {
        HAL_DANGER("v1_venc", "Select operation failed!\n");
        goto abort;
    } else if (ret == 0) {
        HAL_DANGER("v1_venc", "Capture stream timed out!\n");
        goto abort;
    }

    if (FD_ISSET(fd, &readFds)) {
        v1_venc_stat stat;
        if (ret = v1_venc.fnQuery(index, &stat)) {
            HAL_DANGER("v1_venc", "Querying the encoder channel "
                "%d failed with %#x!\n", index, ret);
            goto abort;
        }

        if (!stat.curPacks) {
            HAL_DANGER("v1_venc", "Current frame is empty, skipping it!\n");
            goto abort;
        }

        v1_venc_strm strm;
        memset(&strm, 0, sizeof(strm));
        strm.packet = (v1_venc_pack*)malloc(sizeof(v1_venc_pack) * stat.curPacks);
        if (!strm.packet) {
            HAL_DANGER("v1_venc", "Memory allocation on channel %d failed!\n", index);
            goto abort;
        }
        strm.count = stat.curPacks;

        if (ret = v1_venc.fnGetStream(index, &strm, stat.curPacks)) {
            HAL_DANGER("v1_venc", "Getting the stream on "
                "channel %d failed with %#x!\n", index, ret);
            free(strm.packet);
            strm.packet = NULL;
            goto abort;
        }

        {
            jpeg->jpegSize = 0;
            for (unsigned int i = 0; i < strm.count; i++) {
                v1_venc_pack *pack = &strm.packet[i];
                unsigned int packLen = pack->length[0] + pack->length[1] - pack->offset;
                unsigned char *packData = pack->data[0] + pack->offset;

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
        v1_venc.fnFreeStream(index, &strm);
    }

    v1_venc.fnStopReceiving(index);

    v1_channel_unbind(index);

    return ret;
}

void *v1_video_thread(void)
{
    int ret, maxFd = 0;

    for (int i = 0; i < V1_VENC_CHN_NUM; i++) {
        if (!v1_state[i].enable) continue;
        if (!v1_state[i].mainLoop) continue;

        ret = v1_venc.fnGetDescriptor(i);
        if (ret < 0) {
            HAL_DANGER("v1_venc", "Getting the encoder descriptor failed with %#x!\n", ret);
            return (void*)0;
        }
        v1_state[i].fileDesc = ret;

        if (maxFd <= v1_state[i].fileDesc)
            maxFd = v1_state[i].fileDesc;
    }

    v1_venc_stat stat;
    v1_venc_strm stream;
    struct timeval timeout;
    fd_set readFds;

    while (keepRunning) {
        FD_ZERO(&readFds);
        for(int i = 0; i < V1_VENC_CHN_NUM; i++) {
            if (!v1_state[i].enable) continue;
            if (!v1_state[i].mainLoop) continue;
            FD_SET(v1_state[i].fileDesc, &readFds);
        }

        timeout.tv_sec = 2;
        timeout.tv_usec = 0;
        ret = select(maxFd + 1, &readFds, NULL, NULL, &timeout);
        if (ret < 0) {
            HAL_DANGER("v1_venc", "Select operation failed!\n");
            break;
        } else if (ret == 0) {
            HAL_WARNING("v1_venc", "Main stream loop timed out!\n");
            continue;
        } else {
            for (int i = 0; i < V1_VENC_CHN_NUM; i++) {
                if (!v1_state[i].enable) continue;
                if (!v1_state[i].mainLoop) continue;
                if (FD_ISSET(v1_state[i].fileDesc, &readFds)) {
                    memset(&stream, 0, sizeof(stream));
                    
                    if (ret = v1_venc.fnQuery(i, &stat)) {
                        HAL_DANGER("v1_venc", "Querying the encoder channel "
                            "%d failed with %#x!\n", i, ret);
                        break;
                    }

                    if (!stat.curPacks) {
                        HAL_WARNING("v1_venc", " Current frame is empty, skipping it!\n");
                        continue;
                    }

                    stream.packet = (v1_venc_pack*)malloc(
                        sizeof(v1_venc_pack) * stat.curPacks);
                    if (!stream.packet) {
                        HAL_DANGER("v1_venc", "Memory allocation on channel %d failed!\n", i);
                        break;
                    }
                    stream.count = stat.curPacks;

                    if (ret = v1_venc.fnGetStream(i, &stream, 0)) {
                        HAL_DANGER("v1_venc", "Getting the stream on "
                            "channel %d failed with %#x!\n", i, ret);
                        break;
                    }

                    if (v1_vid_cb) {
                        hal_vidstream outStrm;
                        hal_vidpack outPack[stream.count];
                        outStrm.count = stream.count;
                        outStrm.seq = stream.sequence;
                        for (int j = 0; j < stream.count; j++) {
                            v1_venc_pack *pack = &stream.packet[j];
                            outPack[j].data = pack->data[0];
                            outPack[j].length = pack->length[0] + pack->length[1];
                            outPack[j].naluCnt = 1;
                            outPack[j].nalu[0].length = pack->length[0] + pack->length[1];
                            outPack[j].nalu[0].offset = pack->offset;
                            switch (v1_state[i].payload) {
                                case HAL_VIDCODEC_H264:
                                    outPack[j].nalu[0].type = pack->naluType.h264Nalu;
                                    break;
                            }
                            outPack[j].offset = pack->offset;
                            outPack[j].timestamp = pack->timestamp;
                        }
                        outStrm.pack = outPack;
                        (*v1_vid_cb)(i, &outStrm);
                    }

                    if (ret = v1_venc.fnFreeStream(i, &stream)) {
                        HAL_WARNING("v1_venc", "Releasing the stream on "
                            "channel %d failed with %#x!\n", i, ret);
                    }
                    free(stream.packet);
                    stream.packet = NULL;
                }
            }
        }
    }

    HAL_INFO("v1_venc", "Shutting down encoding thread...\n");
}

void v1_system_deinit(void)
{
    v1_sys.fnExit();
    v1_vb.fnExit();

    v1_sensor_deinit();
}

int v1_system_init(char *snrConfig)
{
    int ret;

    printf("App built with headers v%s\n", V1_SYS_API);

    {
        v1_sys_ver version;
        v1_sys.fnGetVersion(&version);
        puts(version.version);
    }

    if (v1_parse_sensor_config(snrConfig, &v1_config) != CONFIG_OK)
        HAL_ERROR("v1_sys", "Can't load sensor config\n");

    v1_sys.fnExit();
    v1_vb.fnExit();

    {
        int alignWidth = 16;
        v1_vb_pool pool;

        memset(&pool, 0, sizeof(pool)); 
        
        pool.count = 1;
        pool.comm[0].blockSize = v1_buffer_calculate_venc(
            v1_config.vichn.capt.width ? 
                v1_config.vichn.capt.width : v1_config.videv.rect.width,
            v1_config.vichn.capt.height ? 
                v1_config.vichn.capt.height : v1_config.videv.rect.height,
            V1_PIXFMT_YUV420SP, alignWidth);
        pool.comm[0].blockCnt = 10;

        if (ret = v1_vb.fnConfigPool(&pool))
            return ret;
        if (ret = v1_vb.fnInit())
            return ret;
        
        if (ret = v1_sys.fnSetAlignment(&alignWidth))
            return ret;
    }
    if (ret = v1_sys.fnInit())
        return ret;

    if (ret = v1_sensor_init(v1_config.dll_file, v1_config.sensor_type))
        return ret;

    return EXIT_SUCCESS;
}

#endif