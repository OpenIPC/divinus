#if defined(__riscv) || defined(__riscv__)

#include "cvi_hal.h"

cvi_isp_alg      cvi_ae_lib = { .id = 0, .libName = "ae_lib" };
cvi_aud_impl     cvi_aud;
cvi_isp_alg      cvi_awb_lib = { .id = 0, .libName = "awb_lib" };
cvi_config_impl  cvi_config;
cvi_isp_impl     cvi_isp;
cvi_snr_drv_impl cvi_snr_drv;
cvi_rgn_impl     cvi_rgn;
cvi_sys_impl     cvi_sys;
cvi_vb_impl      cvi_vb;
cvi_venc_impl    cvi_venc;
cvi_vi_impl      cvi_vi;
cvi_vpss_impl    cvi_vpss;

hal_chnstate cvi_state[CVI_VENC_CHN_NUM] = {0};
int (*cvi_aud_cb)(hal_audframe*);
int (*cvi_vid_cb)(char, hal_vidstream*);

char _cvi_aud_chn = 0;
char _cvi_aud_dev = 0;
char _cvi_isp_chn = 0;
char _cvi_isp_dev = 0;
char _cvi_venc_dev = 0;
char _cvi_vi_chn = 0;
char _cvi_vi_dev = 0;
char _cvi_vi_pipe = 0;
char _cvi_vpss_chn = 0;
char _cvi_vpss_grp = 0;

void cvi_hal_deinit(void)
{
    cvi_vpss_unload(&cvi_vpss);
    cvi_vi_unload(&cvi_vi);
    cvi_venc_unload(&cvi_venc);
    cvi_vb_unload(&cvi_vb);
    cvi_rgn_unload(&cvi_rgn);
    cvi_isp_unload(&cvi_isp);
    cvi_aud_unload(&cvi_aud);
    cvi_sys_unload(&cvi_sys);
}

int cvi_hal_init(void)
{
    int ret;

    if (ret = cvi_sys_load(&cvi_sys))
        return ret;
    if (ret = cvi_aud_load(&cvi_aud))
        return ret;
    if (ret = cvi_isp_load(&cvi_isp))
        return ret;
    if (ret = cvi_rgn_load(&cvi_rgn))
        return ret;
    if (ret = cvi_vb_load(&cvi_vb))
        return ret;
    if (ret = cvi_venc_load(&cvi_venc))
        return ret;
    if (ret = cvi_vi_load(&cvi_vi))
        return ret;
    if (ret = cvi_vpss_load(&cvi_vpss))
        return ret;

    return EXIT_SUCCESS;
}

void cvi_audio_deinit(void)
{
    cvi_aud.fnDisableChannel(_cvi_aud_dev, _cvi_aud_chn);

    cvi_aud.fnDisableDevice(_cvi_aud_dev);
}

int cvi_audio_init(int samplerate)
{
    int ret;

    {
        cvi_aud_cnf config;
        config.rate = samplerate;
        config.bit = CVI_AUD_BIT_16;
        config.intf = CVI_AUD_INTF_I2S_MASTER;
        config.stereoOn = 0;
        config.expandOn = 0;
        config.frmNum = 30;
        config.packNumPerFrm = 320;
        config.chnNum = 1;
        config.syncRxClkOn = 0;
        config.i2sType = CVI_AUD_I2ST_INNERCODEC;
        if (ret = cvi_aud.fnSetDeviceConfig(_cvi_aud_dev, &config))
            return ret;
    }
    if (ret = cvi_aud.fnEnableDevice(_cvi_aud_dev))
        return ret;
    
    if (ret = cvi_aud.fnEnableChannel(_cvi_aud_dev, _cvi_aud_chn))
        return ret;

    return EXIT_SUCCESS;
}

void *cvi_audio_thread(void)
{
    int ret;

    cvi_aud_frm frame;
    cvi_aud_efrm echoFrame;
    memset(&frame, 0, sizeof(frame));
    memset(&echoFrame, 0, sizeof(echoFrame));

    while (keepRunning) {
        if (ret = cvi_aud.fnGetFrame(_cvi_aud_dev, _cvi_aud_chn, 
            &frame, &echoFrame, 128)) {
            HAL_WARNING("cvi_aud", "Getting the frame failed "
                "with %#x!\n", ret);
            continue;
        }

        if (cvi_aud_cb) {
            hal_audframe outFrame;
            outFrame.channelCnt = 1;
            outFrame.data[0] = frame.addr[0];
            outFrame.length[0] = frame.length;
            outFrame.seq = frame.sequence;
            outFrame.timestamp = frame.timestamp;
            (cvi_aud_cb)(&outFrame);
        }

        if (ret = cvi_aud.fnFreeFrame(_cvi_aud_dev, _cvi_aud_chn,
            &frame, &echoFrame)) {
            HAL_WARNING("cvi_aud", "Releasing the frame failed"
                " with %#x!\n", ret);
        }
    }
    fprintf(stderr, "[cvi_aud] Shutting down capture thread...\n");
}

int cvi_channel_bind(char index)
{
    int ret;

    if (ret = cvi_vpss.fnEnableChannel(_cvi_vpss_grp, index))
        return ret;

    {
        cvi_sys_bind source = { .module = CVI_SYS_MOD_VPSS, 
            .device = _cvi_vpss_grp, .channel = index };
        cvi_sys_bind dest = { .module = CVI_SYS_MOD_VENC,
            .device = _cvi_venc_dev, .channel = index };
        if (ret = cvi_sys.fnBind(&source, &dest))
            return ret;
    }

    return EXIT_SUCCESS;
}

int cvi_channel_create(char index, char mirror, char flip, char framerate)
{
    int ret;

    {
        cvi_vpss_chn channel;
        memset(&channel, 0, sizeof(channel));
        channel.dest.width = cvi_config.isp.capt.width;
        channel.dest.height = cvi_config.isp.capt.height;
        channel.pixFmt = CVI_PIXFMT_YUV420P;
        channel.srcFps = cvi_config.isp.framerate;
        channel.dstFps = framerate;
        channel.mirror = mirror;
        channel.flip = flip;
        if (ret = cvi_vpss.fnSetChannelConfig(_cvi_vpss_grp, index, &channel))
            return ret;
    }

    return EXIT_SUCCESS;
}

int cvi_channel_grayscale(char enable)
{
    int ret;

    for (char i = 0; i < CVI_VENC_CHN_NUM; i++) {
        cvi_venc_para param;
        if (!cvi_state[i].enable) continue;
        if (ret = cvi_venc.fnGetChannelParam(i, &param))
            return ret;
        param.grayscaleOn = enable;
        if (ret = cvi_venc.fnSetChannelParam(i, &param))
            return ret;
    }

    return EXIT_SUCCESS;
}

int cvi_channel_unbind(char index)
{
    int ret;

    if (ret = cvi_vpss.fnDisableChannel(_cvi_vpss_grp, index))
        return ret;

    {
        cvi_sys_bind source = { .module = CVI_SYS_MOD_VPSS, 
            .device = _cvi_vpss_grp, .channel = index };
        cvi_sys_bind dest = { .module = CVI_SYS_MOD_VENC,
            .device = _cvi_venc_dev, .channel = index };
        if (ret = cvi_sys.fnUnbind(&source, &dest))
            return ret;
    }

    return EXIT_SUCCESS;
}

void *cvi_image_thread(void)
{
    int ret;

    if (ret = cvi_isp.fnRun(_cvi_isp_dev))
        HAL_DANGER("cvi_isp", "Operation failed with %#x!\n", ret);
    HAL_INFO("cvi_isp", "Shutting down ISP thread...\n");
}

int cvi_pipeline_create(void)
{
    int ret;

    cvi_vpss.fnDestroyGroup(_cvi_vpss_grp);

    {
        cvi_sys_oper mode[4];
        cvi_sys.fnGetViVpssMode((cvi_sys_oper**)&mode);
        for (int i = 0; i < 4; i++)
            mode[i] = CVI_SYS_OPER_VIOFF_VPSSOFF;
        if (ret = cvi_sys.fnSetViVpssMode((cvi_sys_oper**)&mode))
            return ret;
    }

    if (ret = cvi_vi.fnSetDeviceConfig(_cvi_vi_dev, &cvi_config.videv))
        return ret;
    if (ret = cvi_vi.fnEnableDevice(_cvi_vi_dev))
        return ret;

    {
        cvi_vi_bind bind;
        bind.num = 1;
        bind.pipeId[0] = _cvi_vi_pipe;
        if (ret = cvi_vi.fnBindPipe(_cvi_vi_dev, &bind))
            return ret;
    }

    {
        cvi_vi_pipe pipe;
        pipe.bypass = 0;
        pipe.yuvSkipOn = 0;
        pipe.ispBypassOn = 0;
        pipe.maxSize.width = cvi_config.isp.capt.width;
        pipe.maxSize.height = cvi_config.isp.capt.height;
        pipe.pixFmt = CVI_PIXFMT_RGB_BAYER_8BPP + cvi_config.mipi.prec;
        pipe.compress = CVI_COMPR_NONE;
        pipe.prec = cvi_config.mipi.prec;
        pipe.nRedOn = 0;
        pipe.sharpenOn = 1;
        pipe.srcFps = -1;
        pipe.dstFps = -1;
        if (ret = cvi_vi.fnCreatePipe(_cvi_vi_pipe, &pipe))
            return ret;
    }
    if (ret = cvi_vi.fnStartPipe(_cvi_vi_pipe))
        return ret;

    {
        cvi_vi_chn channel;
        channel.size.width = cvi_config.isp.capt.width;
        channel.size.height = cvi_config.isp.capt.height;
        channel.pixFmt = CVI_PIXFMT_YUV420P;
        channel.dynRange = CVI_HDR_SDR8;
        channel.compress = CVI_COMPR_NONE;
        channel.mirror = 0;
        channel.flip = 0;
        channel.depth = 0;
        channel.srcFps = cvi_config.isp.framerate;
        channel.dstFps = cvi_config.isp.framerate;
        if (ret = cvi_vi.fnSetChannelConfig(_cvi_vi_pipe, _cvi_vi_chn, &channel))
            return ret;
    }
    if (ret = cvi_vi.fnEnableChannel(_cvi_vi_pipe, _cvi_vi_chn))
        return ret;

    if (ret = cvi_snr_drv.obj->pfnRegisterCallback(_cvi_vi_pipe, &cvi_ae_lib, &cvi_awb_lib))
        return ret;
    
    if (ret = cvi_isp.fnRegisterAE(_cvi_vi_pipe, &cvi_ae_lib))
        return ret;
    if (ret = cvi_isp.fnRegisterAWB(_cvi_vi_pipe, &cvi_awb_lib))
        return ret;
    if (ret = cvi_isp.fnMemInit(_cvi_vi_pipe))
        return ret;

    cvi_config.isp.capt.x = 0;
    cvi_config.isp.capt.y = 0;
    if (ret = cvi_isp.fnSetDeviceConfig(_cvi_vi_pipe, &cvi_config.isp))
        return ret;
    if (ret = cvi_isp.fnInit(_cvi_vi_pipe))
        return ret;
    
    {
        cvi_vpss_grp group;
        memset(&group, 0, sizeof(group));
        group.dest.width = cvi_config.isp.capt.width;
        group.dest.height = cvi_config.isp.capt.height;
        group.pixFmt = CVI_PIXFMT_YUV420P;
        group.srcFps = cvi_config.isp.framerate;
        group.dstFps = cvi_config.isp.framerate;
        if (ret = cvi_vpss.fnCreateGroup(_cvi_vpss_grp, &group))
            return ret;
    }
    if (ret = cvi_vpss.fnStartGroup(_cvi_vpss_grp))
        return ret;

    {
        cvi_sys_bind source = { .module = CVI_SYS_MOD_VI, 
            .device = _cvi_vi_dev, .channel = _cvi_vi_chn };
        cvi_sys_bind dest = { .module = CVI_SYS_MOD_VPSS, 
            .device = _cvi_vpss_grp, .channel = 0 };
        if (ret = cvi_sys.fnBind(&source, &dest))
            return ret;
    }

    return EXIT_SUCCESS;
}

void cvi_pipeline_destroy(void)
{
    cvi_isp.fnExit(_cvi_vi_pipe);
    cvi_isp.fnUnregisterAE(_cvi_vi_pipe, &cvi_ae_lib);
    cvi_isp.fnUnregisterAWB(_cvi_vi_pipe, &cvi_awb_lib);

    cvi_snr_drv.obj->pfnUnRegisterCallback(_cvi_vi_pipe, &cvi_ae_lib, &cvi_awb_lib);

    for (char grp = 0; grp < CVI_VPSS_GRP_NUM; grp++)
    {
        for (char chn = 0; chn < CVI_VPSS_CHN_NUM; chn++)
            cvi_vpss.fnDisableChannel(grp, chn);

        {
            cvi_sys_bind source = { .module = CVI_SYS_MOD_VI, 
                .device = _cvi_vi_dev, .channel = _cvi_vi_chn };
            cvi_sys_bind dest = { .module = CVI_SYS_MOD_VPSS,
                .device = grp, .channel = 0 };
            cvi_sys.fnUnbind(&source, &dest);
        }

        cvi_vpss.fnStopGroup(grp);
        cvi_vpss.fnDestroyGroup(grp);
    }
    
    cvi_vi.fnDisableChannel(_cvi_vi_pipe, _cvi_vi_chn);

    cvi_vi.fnStopPipe(_cvi_vi_pipe);
    cvi_vi.fnDestroyPipe(_cvi_vi_pipe);

    cvi_vi.fnDisableDevice(_cvi_vi_dev);
}

int cvi_region_create(char handle, hal_rect rect, short opacity)
{
    int ret;

    cvi_sys_bind channel = { .module = CVI_SYS_MOD_VENC,
        .device = _cvi_venc_dev, .channel = 0 };
    cvi_rgn_cnf region, regionCurr;
    cvi_rgn_chn attrib, attribCurr;

    memset(&region, 0, sizeof(region));
    region.type = CVI_RGN_TYPE_OVERLAY;
    region.overlay.pixFmt = CVI_PIXFMT_ARGB1555;
    region.overlay.size.width = rect.width;
    region.overlay.size.height = rect.height;
    region.overlay.canvas = handle + 1;

    if (cvi_rgn.fnGetRegionConfig(handle, &regionCurr)) {
        HAL_INFO("cvi_rgn", "Creating region %d...\n", handle);
        if (ret = cvi_rgn.fnCreateRegion(handle, &region))
            return ret;
    } else if (regionCurr.overlay.size.height != region.overlay.size.height || 
        regionCurr.overlay.size.width != region.overlay.size.width) {
        HAL_INFO("cvi_rgn", "Parameters are different, recreating "
            "region %d...\n", handle);
        cvi_rgn.fnDetachChannel(handle, &channel);
        cvi_rgn.fnDestroyRegion(handle);
        if (ret = cvi_rgn.fnCreateRegion(handle, &region))
            return ret;
    }

    if (cvi_rgn.fnGetChannelConfig(handle, &channel, &attribCurr))
        HAL_INFO("cvi_rgn", "Attaching region %d...\n", handle);
    else if (attribCurr.overlay.point.x != rect.x || attribCurr.overlay.point.x != rect.y) {
        HAL_INFO("cvi_rgn", "Position has changed, reattaching "
            "region %d...\n", handle);
        cvi_rgn.fnDetachChannel(handle, &channel);
    }

    memset(&attrib, 0, sizeof(attrib));
    attrib.show = 1;
    attrib.type = CVI_RGN_TYPE_OVERLAY;
    attrib.overlay.point.x = rect.x;
    attrib.overlay.point.y = rect.y;
    attrib.overlay.layer = 7;

    cvi_rgn.fnAttachChannel(handle, &channel, &attrib);

    return EXIT_SUCCESS;
}

void cvi_region_destroy(char handle)
{
    cvi_sys_bind channel = { .module = CVI_SYS_MOD_VENC,
        .device = _cvi_venc_dev, .channel = 0 };
    
    cvi_rgn.fnDetachChannel(handle, &channel);
    cvi_rgn.fnDestroyRegion(handle);
}

int cvi_region_setbitmap(int handle, hal_bitmap *bitmap)
{
    cvi_rgn_bmp nativeBmp = { .data = bitmap->data, .pixFmt = CVI_PIXFMT_ARGB1555,
        .size.height = bitmap->dim.height, .size.width = bitmap->dim.width };

    return cvi_rgn.fnSetBitmap(handle, &nativeBmp);
}

int cvi_sensor_config(void) {
    int device = 0, fd;
    cvi_snr_dev config;
    memset(&config, 0, sizeof(config));
    config.device = device;
    config.input = cvi_config.input_mode;
    config.dest.width = cvi_config.isp.capt.width;
    config.dest.height = cvi_config.isp.capt.height;
    if (config.input == CVI_SNR_INPUT_MIPI)
        memcpy(&config.mipi, &cvi_config.mipi, sizeof(cvi_snr_mipi));
    else if (config.input == CVI_SNR_INPUT_LVDS)
        memcpy(&config.lvds, &cvi_config.lvds, sizeof(cvi_snr_lvds));

    cvi_isp.fnResetSensor(device, 1);

    cvi_isp.fnResetIntf(device, 1);

    cvi_isp.fnSetIntfConfig(_cvi_vi_pipe, &config);

    cvi_isp.fnSetSensorClock(device, 1);

    cvi_isp.fnResetSensor(device, 0);

    return EXIT_SUCCESS;
}

void cvi_sensor_deconfig(void) {
    int device = 0, fd;

    cvi_isp.fnResetSensor(device, 1);

    cvi_isp.fnSetSensorClock(device, 0);

    cvi_isp.fnResetIntf(device, 0);

    cvi_isp.fnResetSensor(device, 0);
}

void cvi_sensor_deinit(void)
{
    dlclose(cvi_snr_drv.handle);
    cvi_snr_drv.handle = NULL;
}

int cvi_sensor_init(char *name, char *obj)
{
    char path[128];
    char* dirs[] = {"%s", "./%s", "/usr/lib/sensors/%s", "/usr/lib/%s",
        "/mnt/system/lib/%s", "/mnt/system/lib/libsns_full.so"};
    char **dir = dirs;

    while (*dir) {
        sprintf(path, *dir++, name);
        if (cvi_snr_drv.handle = dlopen(path, RTLD_LAZY | RTLD_GLOBAL))
            break;
    } if (!cvi_snr_drv.handle)
        HAL_ERROR("cvi_snr", "Failed to load the sensor driver\n");
    
    if (!(cvi_snr_drv.obj = (cvi_snr_obj*)dlsym(cvi_snr_drv.handle, obj)))
        HAL_ERROR("cvi_snr", "Failed to connect the sensor object\nError: %s\n", dlerror());

    return EXIT_SUCCESS;
}

int cvi_video_create(char index, hal_vidconfig *config)
{
    int ret;
    cvi_venc_chn channel;
    memset(&channel, 0, sizeof(channel));
    channel.gop.mode = CVI_VENC_GOPMODE_NORMALP;
    if (config->codec == HAL_VIDCODEC_JPG || config->codec == HAL_VIDCODEC_MJPG) {
        channel.attrib.codec = CVI_VENC_CODEC_MJPG;
        switch (config->mode) {
            case HAL_VIDMODE_CBR:
                channel.rate.mode = CVI_VENC_RATEMODE_MJPGCBR;
                channel.rate.mjpgCbr = (cvi_venc_rate_mjpgbr){ .statTime = 1, .srcFps = config->framerate,
                    .dstFps = config->framerate, .maxBitrate = config->bitrate }; break;
            case HAL_VIDMODE_VBR:
                channel.rate.mode = CVI_VENC_RATEMODE_MJPGVBR;
                channel.rate.mjpgVbr = (cvi_venc_rate_mjpgbr){ .statTime = 1, 
                    .srcFps = config->framerate, .dstFps = config->framerate, 
                    .maxBitrate = MAX(config->bitrate, config->maxBitrate) }; break;
            case HAL_VIDMODE_QP:
                channel.rate.mode = CVI_VENC_RATEMODE_MJPGQP;
                channel.rate.mjpgQp = (cvi_venc_rate_mjpgqp){ .srcFps = config->framerate,
                    .dstFps = config->framerate, .quality = config->maxQual }; break;
            default:
                HAL_ERROR("cvi_venc", "MJPEG encoder can only support CBR, VBR or fixed QP modes!");
        }
    } else if (config->codec == HAL_VIDCODEC_H265) {
        channel.attrib.codec = CVI_VENC_CODEC_H265;
        channel.gop.normalP.ipQualDelta = config->gop / config->framerate;
        switch (config->mode) {
            case HAL_VIDMODE_CBR:
                channel.rate.mode = CVI_VENC_RATEMODE_H265CBR;
                channel.rate.h265Cbr = (cvi_venc_rate_h26xbr){ .gop = config->gop,
                    .statTime = 1, .srcFps = config->framerate, .dstFps = config->framerate,
                    .maxBitrate = config->bitrate }; break;
            case HAL_VIDMODE_VBR:
                channel.rate.mode = CVI_VENC_RATEMODE_H265VBR;
                channel.rate.h265Vbr = (cvi_venc_rate_h26xbr){ .gop = config->gop,
                    .statTime = 1, .srcFps = config->framerate, .dstFps = config->framerate, 
                    .maxBitrate = MAX(config->bitrate, config->maxBitrate) }; break;
            case HAL_VIDMODE_QP:
                channel.rate.mode = CVI_VENC_RATEMODE_H265QP;
                channel.rate.h265Qp = (cvi_venc_rate_h26xqp){ .gop = config->gop,
                    .srcFps = config->framerate, .dstFps = config->framerate, .interQual = config->maxQual, 
                    .predQual = config->minQual, .bipredQual = config->minQual }; break;
            case HAL_VIDMODE_AVBR:
                channel.rate.mode = CVI_VENC_RATEMODE_H265AVBR;
                channel.rate.h265Avbr = (cvi_venc_rate_h26xbr){ .gop = config->gop,
                    .statTime = 1, .srcFps = config->framerate, .dstFps = config->framerate,
                    .maxBitrate = config->bitrate }; break;
            default:
                HAL_ERROR("cvi_venc", "H.265 encoder does not support this mode!");
        }
    } else if (config->codec == HAL_VIDCODEC_H264) {
        channel.attrib.codec = CVI_VENC_CODEC_H264;
        channel.gop.normalP.ipQualDelta = config->gop / config->framerate;
        switch (config->mode) {
            case HAL_VIDMODE_CBR:
                channel.rate.mode = CVI_VENC_RATEMODE_H264CBR;
                channel.rate.h264Cbr = (cvi_venc_rate_h26xbr){ .gop = config->gop,
                    .statTime = 1, .srcFps = config->framerate, .dstFps = config->framerate,
                    .maxBitrate = config->bitrate }; break;
            case HAL_VIDMODE_VBR:
                channel.rate.mode = CVI_VENC_RATEMODE_H264VBR;
                channel.rate.h264Vbr = (cvi_venc_rate_h26xbr){ .gop = config->gop,
                    .statTime = 1, .srcFps = config->framerate, .dstFps = config->framerate, 
                    .maxBitrate = MAX(config->bitrate, config->maxBitrate) }; break;
            case HAL_VIDMODE_QP:
                channel.rate.mode = CVI_VENC_RATEMODE_H264QP;
                channel.rate.h264Qp = (cvi_venc_rate_h26xqp){ .gop = config->gop,
                    .srcFps = config->framerate, .dstFps = config->framerate, .interQual = config->maxQual, 
                    .predQual = config->minQual, .bipredQual = config->minQual }; break;
            case HAL_VIDMODE_AVBR:
                channel.rate.mode = CVI_VENC_RATEMODE_H264AVBR;
                channel.rate.h264Avbr = (cvi_venc_rate_h26xbr){ .gop = config->gop,
                    .statTime = 1, .srcFps = config->framerate, .dstFps = config->framerate,
                    .maxBitrate = config->bitrate }; break;
            default:
                HAL_ERROR("cvi_venc", "H.264 encoder does not support this mode!");
        }
    } else HAL_ERROR("cvi_venc", "This codec is not supported by the hardware!");
    channel.attrib.maxPic.width = config->width;
    channel.attrib.maxPic.height = config->height;
    channel.attrib.bufSize = ALIGN_UP(config->height * config->width * 3 / 4, 64);
    if (channel.attrib.codec == CVI_VENC_CODEC_H264)
        channel.attrib.profile = MAX(config->profile, 2);
    channel.attrib.byFrame = 1;
    channel.attrib.pic.width = config->width;
    channel.attrib.pic.height = config->height;

    if (ret = cvi_venc.fnCreateChannel(index, &channel))
        return ret;

    {
        int count = -1;
        if (config->codec != HAL_VIDCODEC_JPG && 
            (ret = cvi_venc.fnStartReceivingEx(index, &count)))
            return ret;
    }
    
    cvi_state[index].payload = config->codec;

    return EXIT_SUCCESS;
}

int cvi_video_destroy(char index)
{
    int ret;

    cvi_state[index].enable = 0;
    cvi_state[index].payload = HAL_VIDCODEC_UNSPEC;

    cvi_venc.fnStopReceiving(index);

    {
        cvi_sys_bind source = { .module = CVI_SYS_MOD_VPSS, 
            .device = _cvi_vpss_grp, .channel = index };
        cvi_sys_bind dest = { .module = CVI_SYS_MOD_VENC,
            .device = _cvi_venc_dev, .channel = index };
        if (ret = cvi_sys.fnUnbind(&source, &dest))
            return ret;
    }

    if (ret = cvi_venc.fnDestroyChannel(index))
        return ret;
    
    if (ret = cvi_vpss.fnDisableChannel(_cvi_vpss_grp, index))
        return ret;

    return EXIT_SUCCESS;
}
    
int cvi_video_destroy_all(void)
{
    int ret;

    for (char i = 0; i < CVI_VENC_CHN_NUM; i++)
        if (cvi_state[i].enable)
            if (ret = cvi_video_destroy(i))
                return ret;

    return EXIT_SUCCESS;
}

void cvi_video_request_idr(char index)
{
    cvi_venc.fnRequestIdr(index, 1);
}

int cvi_video_snapshot_grab(char index, hal_jpegdata *jpeg)
{
    int ret;

    if (ret = cvi_channel_bind(index)) {
        HAL_DANGER("cvi_venc", "Binding the encoder channel "
            "%d failed with %#x!\n", index, ret);
        goto abort;
    }

    unsigned int count = 1;
    if (cvi_venc.fnStartReceivingEx(index, &count)) {
        HAL_DANGER("cvi_venc", "Requesting one frame "
            "%d failed with %#x!\n", index, ret);
        goto abort;
    }

    int fd = cvi_venc.fnGetDescriptor(index);

    struct timeval timeout = { .tv_sec = 2, .tv_usec = 0 };
    fd_set readFds;
    FD_ZERO(&readFds);
    FD_SET(fd, &readFds);
    ret = select(fd + 1, &readFds, NULL, NULL, &timeout);
    if (ret < 0) {
        HAL_DANGER("cvi_venc", "Select operation failed!\n");
        goto abort;
    } else if (ret == 0) {
        HAL_DANGER("cvi_venc", "Capture stream timed out!\n");
        goto abort;
    }

    if (FD_ISSET(fd, &readFds)) {
        cvi_venc_stat stat;
        if (cvi_venc.fnQuery(index, &stat)) {
            HAL_DANGER("cvi_venc", "Querying the encoder channel "
                "%d failed with %#x!\n", index, ret);
            goto abort;
        }

        if (!stat.curPacks) {
            HAL_DANGER("cvi_venc", "Current frame is empty, skipping it!\n");
            goto abort;
        }

        cvi_venc_strm strm;
        memset(&strm, 0, sizeof(strm));
        strm.packet = (cvi_venc_pack*)malloc(sizeof(cvi_venc_pack) * stat.curPacks);
        if (!strm.packet) {
            HAL_DANGER("cvi_venc", "Memory allocation on channel %d failed!\n", index);
            goto abort;
        }
        strm.count = stat.curPacks;

        if (ret = cvi_venc.fnGetStream(index, &strm, stat.curPacks)) {
            HAL_DANGER("cvi_venc", "Getting the stream on "
                "channel %d failed with %#x!\n", index, ret);
            free(strm.packet);
            strm.packet = NULL;
            goto abort;
        }

        {
            jpeg->jpegSize = 0;
            for (unsigned int i = 0; i < strm.count; i++) {
                cvi_venc_pack *pack = &strm.packet[i];
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
        cvi_venc.fnFreeStream(index, &strm);
    }

    cvi_venc.fnFreeDescriptor(index);

    cvi_venc.fnStopReceiving(index);

    cvi_channel_unbind(index);

    return ret;
}

void *cvi_video_thread(void)
{
    int ret;
    int maxFd = 0;

    for (int i = 0; i < CVI_VENC_CHN_NUM; i++) {
        if (!cvi_state[i].enable) continue;
        if (!cvi_state[i].mainLoop) continue;

        ret = cvi_venc.fnGetDescriptor(i);
        if (ret < 0) {
            HAL_DANGER("cvi_venc", "Getting the encoder descriptor failed with %#x!\n", ret);
            return NULL;
        }
        cvi_state[i].fileDesc = ret;

        if (maxFd <= cvi_state[i].fileDesc)
            maxFd = cvi_state[i].fileDesc;
    }

    cvi_venc_stat stat;
    cvi_venc_strm stream;
    struct timeval timeout;
    fd_set readFds;

    while (keepRunning) {
        FD_ZERO(&readFds);
        for(int i = 0; i < CVI_VENC_CHN_NUM; i++) {
            if (!cvi_state[i].enable) continue;
            if (!cvi_state[i].mainLoop) continue;
            FD_SET(cvi_state[i].fileDesc, &readFds);
        }

        timeout.tv_sec = 2;
        timeout.tv_usec = 0;
        ret = select(maxFd + 1, &readFds, NULL, NULL, &timeout);
        if (ret < 0) {
            HAL_DANGER("cvi_venc", "Select operation failed!\n");
            break;
        } else if (ret == 0) {
            HAL_WARNING("cvi_venc", "Main stream loop timed out!\n");
            continue;
        } else {
            for (int i = 0; i < CVI_VENC_CHN_NUM; i++) {
                if (!cvi_state[i].enable) continue;
                if (!cvi_state[i].mainLoop) continue;
                if (FD_ISSET(cvi_state[i].fileDesc, &readFds)) {
                    memset(&stream, 0, sizeof(stream));
                    
                    if (ret = cvi_venc.fnQuery(i, &stat)) {
                        HAL_DANGER("cvi_venc", "Querying the encoder channel "
                            "%d failed with %#x!\n", i, ret);
                        break;
                    }

                    if (!stat.curPacks) {
                        HAL_WARNING("cvi_venc", "Current frame is empty, skipping it!\n");
                        continue;
                    }

                    stream.packet = (cvi_venc_pack*)malloc(
                        sizeof(cvi_venc_pack) * stat.curPacks);
                    if (!stream.packet) {
                        HAL_DANGER("cvi_venc", "Memory allocation on channel %d failed!\n", i);
                        break;
                    }
                    stream.count = stat.curPacks;

                    if (ret = cvi_venc.fnGetStream(i, &stream, 40)) {
                        HAL_DANGER("cvi_venc", "Getting the stream on "
                            "channel %d failed with %#x!\n", i, ret);
                        break;
                    }

                    if (cvi_vid_cb) {
                        hal_vidstream outStrm;
                        hal_vidpack outPack[stream.count];
                        outStrm.count = stream.count;
                        outStrm.seq = stream.sequence;
                        for (int j = 0; j < stream.count; j++) {
                            cvi_venc_pack *pack = &stream.packet[j];
                            outPack[j].data = pack->data;
                            outPack[j].length = pack->length;
                            outPack[j].naluCnt = 1;
                            outPack[j].nalu[0].length = pack->length;
                            outPack[j].nalu[0].offset = pack->offset;
                            switch (cvi_state[i].payload) {
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
                        (*cvi_vid_cb)(i, &outStrm);
                    }

                    if (ret = cvi_venc.fnFreeStream(i, &stream)) {
                        HAL_WARNING("cvi_venc", "Releasing the stream on "
                            "channel %d failed with %#x!\n", i, ret);
                    }
                    free(stream.packet);
                    stream.packet = NULL;
                }
            }
        }
    }
    HAL_INFO("cvi_venc", "Shutting down encoding thread...\n");
}

void cvi_system_deinit(void)
{
    cvi_sys.fnExit();
    cvi_vb.fnExit();

    cvi_sensor_deinit();
}

int cvi_system_init(char *snrConfig)
{
    int ret;

    printf("App built with headers v%s\n", CVI_SYS_API);

    {
        cvi_sys_ver version;
        cvi_sys.fnGetVersion(&version);
        puts(version.version);
    }

    if (cvi_parse_sensor_config(snrConfig, &cvi_config) != CONFIG_OK)
        HAL_ERROR("cvi_sys", "Can't load sensor config\n");

    if (ret = cvi_sensor_init(cvi_config.dll_file, cvi_config.sensor_type))
        return ret;

    cvi_sys.fnExit();
    cvi_vb.fnExit();

    {
        cvi_vb_pool pool;
        memset(&pool, 0, sizeof(pool));

        pool.count = 2;
        pool.comm[0].blockSize = 1920 * 1080 * 3;
        pool.comm[0].blockCnt = 4;
        pool.comm[1].blockSize = 1920 * 1080 * 3;
        pool.comm[1].blockCnt = 4;

        if (ret = cvi_vb.fnConfigPool(&pool))
            return ret;
    }
    if (ret = cvi_vb.fnInit())
        return ret;

    if (ret = cvi_sys.fnInit())
        return ret;

    return EXIT_SUCCESS;
}

#endif