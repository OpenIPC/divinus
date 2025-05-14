#if defined(__ARM_PCS_VFP)

#include "i3_hal.h"

i3_aud_impl  i3_aud;
i3_isp_impl  i3_isp;
i3_osd_impl  i3_osd;
i3_sys_impl  i3_sys;
i3_venc_impl i3_venc;
i3_vi_impl   i3_vi;

hal_chnstate i3_state[I3_VENC_CHN_NUM] = {0};
int (*i3_aud_cb)(hal_audframe*);
int (*i3_vid_cb)(char, hal_vidstream*);

char _i3_aud_chn = 0;
char _i3_aud_dev = 0;
char _i3_isp_chn = 0;
char _i3_venc_port = 0;
char _i3_vi_chn = 0;
char _i3_vi_dev = 0;
char _i3_vi_port = 0;

void i3_hal_deinit(void)
{
    i3_vi_unload(&i3_vi);
    i3_venc_unload(&i3_venc);
    i3_osd_unload(&i3_osd);
    i3_isp_unload(&i3_isp);
    i3_aud_unload(&i3_aud);
    i3_sys_unload(&i3_sys);
}

int i3_hal_init(void)
{
    int ret;

    if (ret = i3_sys_load(&i3_sys))
        return ret;
    if (ret = i3_aud_load(&i3_aud))
        return ret;
    if (ret = i3_isp_load(&i3_isp))
        return ret;
    if (ret = i3_osd_load(&i3_osd))
        return ret;
    if (ret = i3_venc_load(&i3_venc))
        return ret;
    if (ret = i3_vi_load(&i3_vi))
        return ret;

    return EXIT_SUCCESS;
}

/*void i3_audio_deinit(void)
{
    i3_aud.fnDisableChannel(_i3_aud_dev, _i3_aud_chn);

    i3_aud.fnDisableDevice(_i3_aud_dev);
}

int i3_audio_init(int samplerate)
{
    int ret;

    {
        i3_aud_cnf config;
        config.rate = samplerate;
        config.bit24On = 0;
        config.intf = I3_AUD_INTF_I2S_SLAVE;
        config.sound = I3_AUD_SND_MONO;
        config.frmNum = 0;
        config.packNumPerFrm = 640;
        config.chnNum = 1;
        if (ret = i3_aud.fnSetDeviceConfig(_i3_aud_dev, &config))
            return ret;
    }
    if (ret = i3_aud.fnEnableDevice(_i3_aud_dev))
        return ret;
    
    if (ret = i3_aud.fnEnableChannel(_i3_aud_dev, _i3_aud_chn))
        return ret;
    if (ret = i3_aud.fnSetVolume(_i3_aud_dev, _i3_aud_chn, 13))
        return ret;

    {
        i3_sys_bind bind = { .module = I3_SYS_MOD_AI, 
            .device = _i3_aud_dev, .channel = _i3_aud_chn };
        if (ret = i3_sys.fnSetOutputDepth(&bind, 2, 4))
            return ret;
    }

    return EXIT_SUCCESS;
}

void *i3_audio_thread(void)
{
    int ret;

    i3_aud_frm frame;
    memset(&frame, 0, sizeof(frame));

    while (keepRunning) {
        if (ret = i3_aud.fnGetFrame(_i3_aud_dev, _i3_aud_chn, 
            &frame, NULL, 128)) {
            HAL_WARNING("i3_aud", "Getting the frame failed "
                "with %#x!\n", ret);
            continue;
        }

        if (i3_aud_cb) {
            hal_audframe outFrame;
            outFrame.channelCnt = 1;
            outFrame.data[0] = frame.addr[0];
            outFrame.length[0] = frame.length;
            outFrame.seq = frame.sequence;
            outFrame.timestamp = frame.timestamp;
            (i3_aud_cb)(&outFrame);
        }

        if (ret = i3_aud.fnFreeFrame(_i3_aud_dev, _i3_aud_chn,
            &frame, NULL)) {
            HAL_WARNING("i3_aud", "Releasing the frame failed"
                " with %#x!\n", ret);
        }
    }
    HAL_INFO("i3_aud", "Shutting down capture thread...\n");
}

int i3_channel_bind(char index, char framerate)
{
    int ret;

    if (ret = i3_vpe.fnEnablePort(_i3_vpe_chn, index))
        return ret;

    {
        unsigned int device;
        if (ret = i3_venc.fnGetChannelDeviceId(index, &device))
            return ret;
        i3_sys_bind source = { .module = I3_SYS_MOD_VPE, 
            .device = _i3_vpe_dev, .channel = _i3_vpe_chn, .port = index };
        i3_sys_bind dest = { .module = I3_SYS_MOD_VENC,
            .device = device, .channel = index, .port = _i3_venc_port };
        if (ret = i3_sys.fnBindExt(&source, &dest, framerate, framerate,
            I3_SYS_LINK_FRAMEBASE, 0))
            return ret;
    }

    return EXIT_SUCCESS;
}

int i3_channel_create(char index, short width, short height, char mirror, char flip, char jpeg)
{
    i3_vpe_port port;
    port.output.width = width;
    port.output.height = height;
    port.mirror = mirror;
    port.flip = flip;
    port.compress = I3_COMPR_NONE;
    port.pixFmt = jpeg ? I3_PIXFMT_YUV422_YUYV : I3_PIXFMT_YUV420SP;

    return i3_vpe.fnSetPortConfig(_i3_vpe_chn, index, &port);
}

int i3_channel_grayscale(char enable)
{
    return i3_isp.fnSetColorToGray(0, &enable);
}

int i3_channel_unbind(char index)
{
    int ret;

    if (ret = i3_vpe.fnDisablePort(_i3_vpe_chn, index))
        return ret;

    {
        unsigned int device;
        if (ret = i3_venc.fnGetChannelDeviceId(index, &device))
            return ret;
        i3_sys_bind source = { .module = I3_SYS_MOD_VPE, 
            .device = _i3_vpe_dev, .channel = _i3_vpe_chn, .port = index };
        i3_sys_bind dest = { .module = I3_SYS_MOD_VENC,
            .device = device, .channel = index, .port = _i3_venc_port };
        if (ret = i3_sys.fnUnbind(&source, &dest))
            return ret;
    }

    return EXIT_SUCCESS; 
}*/

int i3_config_load(char *path)
{
    return i3_isp.fnLoadChannelConfig(_i3_isp_chn, path, 1234);
}

/*int i3_pipeline_create(char sensor, short width, short height, char framerate)
{
    int ret;

    {
        i3_vif_dev device;
        memset(&device, 0, sizeof(device));
        device.intf = _i3_snr_pad.intf;
        device.work = device.intf == I3_INTF_BT656 ? 
            I3_VIF_WORK_1MULTIPLEX : I3_VIF_WORK_RGB_REALTIME;
        device.hdr = I3_HDR_OFF;
        if (device.intf == I3_INTF_MIPI) {
            device.edge = I3_EDGE_DOUBLE;
            device.input = _i3_snr_pad.intfAttr.mipi.input;
        } else if (device.intf == I3_INTF_BT656) {
            device.edge = _i3_snr_pad.intfAttr.bt656.edge;
            device.sync = _i3_snr_pad.intfAttr.bt656.sync;
        }
        if (ret = i3_vif.fnSetDeviceConfig(_i3_vif_dev, &device))
            return ret;
    }
    if (ret = i3_vif.fnEnableDevice(_i3_vif_dev))
        return ret;

    {
        i3_vif_port port;
        port.capt = _i3_snr_plane.capt;
        port.dest.height = _i3_snr_plane.capt.height;
        port.dest.width = _i3_snr_plane.capt.width;
        port.field = 0;
        port.interlaceOn = 0;
        port.pixFmt = (i3_common_pixfmt)(_i3_snr_plane.bayer > I3_BAYER_END ? 
            _i3_snr_plane.pixFmt : (I3_PIXFMT_RGB_BAYER + _i3_snr_plane.precision * I3_BAYER_END + _i3_snr_plane.bayer));
        port.frate = I3_VIF_FRATE_FULL;
        port.frameLineCnt = 0;
        if (ret = i3_vif.fnSetPortConfig(_i3_vif_chn, _i3_vif_port, &port))
            return ret;
    }
    if (ret = i3_vif.fnEnablePort(_i3_vif_chn, _i3_vif_port))
        return ret;

    if (series == 0xF1) {
        i3e_vpe_chn channel;
        memset(&channel, 0, sizeof(channel));
        channel.capt.height = _i3_snr_plane.capt.height;
        channel.capt.width = _i3_snr_plane.capt.width;
        channel.pixFmt = (i3_common_pixfmt)(_i3_snr_plane.bayer > I3_BAYER_END ? 
            _i3_snr_plane.pixFmt : (I3_PIXFMT_RGB_BAYER + _i3_snr_plane.precision * I3_BAYER_END + _i3_snr_plane.bayer));
        channel.hdr = I3_HDR_OFF;
        channel.sensor = (i3_vpe_sens)(_i3_snr_index + 1);
        channel.mode = I3_VPE_MODE_REALTIME;
        if (ret = i3_vpe.fnCreateChannel(_i3_vpe_chn, (i3_vpe_chn*)&channel))
            return ret;
    } else {
        i3_vpe_chn channel;
        memset(&channel, 0, sizeof(channel));
        channel.capt.height = _i3_snr_plane.capt.height;
        channel.capt.width = _i3_snr_plane.capt.width;
        channel.pixFmt = (i3_common_pixfmt)(_i3_snr_plane.bayer > I3_BAYER_END ? 
            _i3_snr_plane.pixFmt : (I3_PIXFMT_RGB_BAYER + _i3_snr_plane.precision * I3_BAYER_END + _i3_snr_plane.bayer));
        channel.hdr = I3_HDR_OFF;
        channel.sensor = (i3_vpe_sens)(_i3_snr_index + 1);
        channel.mode = I3_VPE_MODE_REALTIME;
        if (ret = i3_vpe.fnCreateChannel(_i3_vpe_chn, &channel))
            return ret;
    }

    {
        i3_vpe_para param;
        param.hdr = I3_HDR_OFF;
        param.level3DNR = 1;
        param.mirror = 0;
        param.flip = 0;
        param.lensAdjOn = 0;
        if (ret = i3_vpe.fnSetChannelParam(_i3_vpe_chn, &param))
            return ret;
    }
    if (ret = i3_vpe.fnStartChannel(_i3_vpe_chn))
        return ret;

    {
        i3_sys_bind source = { .module = I3_SYS_MOD_VIF, 
            .device = _i3_vif_dev, .channel = _i3_vif_chn, .port = _i3_vif_port };
        i3_sys_bind dest = { .module = I3_SYS_MOD_VPE,
            .device = _i3_vpe_dev, .channel = _i3_vpe_chn, .port = _i3_vpe_port };
        return i3_sys.fnBindExt(&source, &dest, _i3_snr_framerate, _i3_snr_framerate,
            I3_SYS_LINK_REALTIME, 0);
    }

    return EXIT_SUCCESS;
}

void i3_pipeline_destroy(void)
{
    for (char i = 0; i < 4; i++)
        i3_vpe.fnDisablePort(_i3_vpe_chn, i);

    {
        i3_sys_bind source = { .module = I3_SYS_MOD_VIF, 
            .device = _i3_vif_dev, .channel = _i3_vif_chn, .port = _i3_vif_port };
        i3_sys_bind dest = { .module = I3_SYS_MOD_VPE,
            .device = _i3_vif_dev, .channel = _i3_vpe_chn, .port = _i3_vpe_port };
        i3_sys.fnUnbind(&source, &dest);
    }

    i3_vpe.fnStopChannel(_i3_vpe_chn);
    i3_vpe.fnDestroyChannel(_i3_vpe_chn);

    i3_vif.fnDisablePort(_i3_vif_chn, 0);
    i3_vif.fnDisableDevice(_i3_vif_dev);
}

int i3_region_create(char handle, hal_rect rect, short opacity)
{
    int ret;

    i3_sys_bind dest = { .module = 0,
        .device = _i3_vpe_dev, .channel = _i3_vpe_chn };
    i3_rgn_cnf region, regionCurr;
    i3_rgn_chn attrib, attribCurr;

    region.type = I3_RGN_TYPE_OSD;
    region.pixFmt = I3_RGN_PIXFMT_ARGB1555;
    region.size.width = rect.width;
    region.size.height = rect.height;

    if (i3_rgn.fnGetRegionConfig(handle, &regionCurr)) {
        HAL_INFO("i3_rgn", "Creating region %d...\n", handle);
        if (ret = i3_rgn.fnCreateRegion(handle, &region))
            return ret;
    } else if (regionCurr.type != region.type ||
        regionCurr.size.height != region.size.height || 
        regionCurr.size.width != region.size.width) {
        HAL_INFO("i3_rgn", "Parameters are different, recreating "
            "region %d...\n", handle);
        for (char i = 0; i < I3_VENC_CHN_NUM; i++) {
            if (!i3_state[i].enable) continue;
            dest.port = i;
            i3_rgn.fnDetachChannel(handle, &dest);
        }
        i3_rgn.fnDestroyRegion(handle);
        if (ret = i3_rgn.fnCreateRegion(handle, &region))
            return ret;
    }

    if (i3_rgn.fnGetChannelConfig(handle, &dest, &attribCurr))
        HAL_INFO("i3_rgn", "Attaching region %d...\n", handle);
    else if (attribCurr.point.x != rect.x || attribCurr.point.x != rect.y ||
        attribCurr.osd.bgFgAlpha[1] != opacity) {
        HAL_INFO("i3_rgn", "Parameters are different, reattaching "
            "region %d...\n", handle);
        for (char i = 0; i < I3_VENC_CHN_NUM; i++) {
            if (!i3_state[i].enable) continue;
            dest.port = i;
            i3_rgn.fnDetachChannel(handle, &dest);
        }
    }

    memset(&attrib, 0, sizeof(attrib));
    attrib.show = 1;
    attrib.point.x = rect.x;
    attrib.point.y = rect.y;
    attrib.osd.layer = 0;
    attrib.osd.constAlphaOn = 0;
    attrib.osd.bgFgAlpha[0] = 0;
    attrib.osd.bgFgAlpha[1] = opacity;

    for (char i = 0; i < I3_VENC_CHN_NUM; i++) {
        if (!i3_state[i].enable) continue;
        dest.port = i;
        i3_rgn.fnAttachChannel(handle, &dest, &attrib);
    }

    return EXIT_SUCCESS;
}

void i3_region_destroy(char handle)
{
    i3_sys_bind dest = { .module = 0,
        .device = _i3_vpe_dev, .channel = _i3_vpe_chn };
    
    dest.port = 1;
    i3_rgn.fnDetachChannel(handle, &dest);
    dest.port = 0;
    i3_rgn.fnDetachChannel(handle, &dest);
    i3_rgn.fnDestroyRegion(handle);
}

int i3_region_setbitmap(int handle, hal_bitmap *bitmap)
{
    i3_rgn_bmp nativeBmp = { .data = bitmap->data, .pixFmt = I3_RGN_PIXFMT_ARGB1555,
        .size.height = bitmap->dim.height, .size.width = bitmap->dim.width };

    return i3_rgn.fnSetBitmap(handle, &nativeBmp);
}

int i3_video_create(char index, hal_vidconfig *config)
{
    int ret;
    i3_venc_chn channel;
    i3_venc_attr_h26x *attrib;
    
    if (config->codec == HAL_VIDCODEC_JPG || config->codec == HAL_VIDCODEC_MJPG) {
        channel.attrib.codec = I3_VENC_CODEC_MJPG;
        switch (config->mode) {
            case HAL_VIDMODE_CBR:
                channel.rate.mode = series == 0xEF ? I3OG_VENC_RATEMODE_MJPGCBR : I3_VENC_RATEMODE_MJPGCBR;
                channel.rate.mjpgCbr.bitrate = config->bitrate << 10;
                channel.rate.mjpgCbr.fpsNum = 
                    config->codec == HAL_VIDCODEC_JPG ? 1 : config->framerate;
                channel.rate.mjpgCbr.fpsDen = 1;
                break;
            case HAL_VIDMODE_QP:
                channel.rate.mode = series == 0xEF ? I3OG_VENC_RATEMODE_MJPGQP : I3_VENC_RATEMODE_MJPGQP;
                channel.rate.mjpgQp.fpsNum = 
                    config->codec == HAL_VIDCODEC_JPG ? 1 : config->framerate;
                channel.rate.mjpgCbr.fpsDen = 1;
                channel.rate.mjpgQp.quality = MAX(config->minQual, config->maxQual);
                break;
            default:
                HAL_ERROR("i3_venc", "MJPEG encoder can only support CBR or fixed QP modes!");
        }

        channel.attrib.mjpg.maxHeight = config->height;
        channel.attrib.mjpg.maxWidth = config->width;
        channel.attrib.mjpg.bufSize = config->width * config->height;
        channel.attrib.mjpg.byFrame = 1;
        channel.attrib.mjpg.height = config->height;
        channel.attrib.mjpg.width = config->width;
        channel.attrib.mjpg.dcfThumbs = 0;
        channel.attrib.mjpg.markPerRow = 0;

        goto attach;
    } else if (config->codec == HAL_VIDCODEC_H265) {
        channel.attrib.codec = I3_VENC_CODEC_H265;
        attrib = &channel.attrib.h265;
        switch (config->mode) {
            case HAL_VIDMODE_CBR:
                channel.rate.mode = series == 0xEF ? I3OG_VENC_RATEMODE_H265CBR :
                    I3_VENC_RATEMODE_H265CBR;
                channel.rate.h265Cbr = (i3_venc_rate_h26xcbr){ .gop = config->gop,
                    .statTime = 1, .fpsNum = config->framerate, .fpsDen = 1, .bitrate = 
                    (unsigned int)(config->bitrate) << 10, .avgLvl = 1 }; break;
            case HAL_VIDMODE_VBR:
                channel.rate.mode = series == 0xEF ? I3OG_VENC_RATEMODE_H265VBR :
                    I3_VENC_RATEMODE_H265VBR;
                channel.rate.h265Vbr = (i3_venc_rate_h26xvbr){ .gop = config->gop,
                    .statTime = 1, .fpsNum = config->framerate, .fpsDen = 1, .maxBitrate = 
                    (unsigned int)(MAX(config->bitrate, config->maxBitrate)) << 10,
                    .maxQual = config->maxQual, .minQual = config->minQual }; break;
            case HAL_VIDMODE_QP:
                channel.rate.mode = series == 0xEF ? I3OG_VENC_RATEMODE_H265QP :
                    I3_VENC_RATEMODE_H265QP;
                channel.rate.h265Qp = (i3_venc_rate_h26xqp){ .gop = config->gop,
                    .fpsNum = config->framerate, .fpsDen = 1, .interQual = config->maxQual,
                    .predQual = config->minQual }; break;
            case HAL_VIDMODE_ABR:
                HAL_ERROR("i3_venc", "H.265 encoder does not support ABR mode!");
            case HAL_VIDMODE_AVBR:
                channel.rate.mode = series == 0xEF ? I3OG_VENC_RATEMODE_H265AVBR :
                    I3_VENC_RATEMODE_H265AVBR;
                channel.rate.h265Avbr = (i3_venc_rate_h26xvbr){ .gop = config->gop,
                    .statTime = 1, .fpsNum = config->framerate, .fpsDen = 1, .maxBitrate = 
                    (unsigned int)(MAX(config->bitrate, config->maxBitrate)) << 10,
                    .maxQual = config->maxQual, .minQual = config->minQual }; break;
            default:
                HAL_ERROR("i3_venc", "H.265 encoder does not support this mode!");
        }  
    } else if (config->codec == HAL_VIDCODEC_H264) {
        channel.attrib.codec = I3_VENC_CODEC_H264;
        attrib = &channel.attrib.h264;
        if (series == 0xEF && config->mode == HAL_VIDMODE_ABR)
            config->mode = -1;
        switch (config->mode) {
            case HAL_VIDMODE_CBR:
                channel.rate.mode = series == 0xEF ? I3OG_VENC_RATEMODE_H264CBR :
                    I3_VENC_RATEMODE_H264CBR;
                channel.rate.h264Cbr = (i3_venc_rate_h26xcbr){ .gop = config->gop,
                    .statTime = 1, .fpsNum = config->framerate, .fpsDen = 1, .bitrate = 
                    (unsigned int)(config->bitrate) << 10, .avgLvl = 1 }; break;
            case HAL_VIDMODE_VBR:
                channel.rate.mode = series == 0xEF ? I3OG_VENC_RATEMODE_H264VBR :
                    I3_VENC_RATEMODE_H264VBR;
                channel.rate.h264Vbr = (i3_venc_rate_h26xvbr){ .gop = config->gop,
                    .statTime = 1, .fpsNum = config->framerate, .fpsDen = 1, .maxBitrate = 
                    (unsigned int)(MAX(config->bitrate, config->maxBitrate)) << 10,
                    .maxQual = config->maxQual, .minQual = config->minQual }; break;
            case HAL_VIDMODE_QP:
                channel.rate.mode = series == 0xEF ? I3OG_VENC_RATEMODE_H264QP :
                    I3_VENC_RATEMODE_H264QP;
                channel.rate.h264Qp = (i3_venc_rate_h26xqp){ .gop = config->gop,
                    .fpsNum = config->framerate, .fpsDen = 1, .interQual = config->maxQual,
                    .predQual = config->minQual }; break;
            case HAL_VIDMODE_ABR:
                channel.rate.mode = I3_VENC_RATEMODE_H264ABR;
                channel.rate.h264Abr = (i3_venc_rate_h26xabr){ .gop = config->gop,
                    .statTime = 1, .fpsNum = config->framerate, .fpsDen = 1,
                    .avgBitrate = (unsigned int)(config->bitrate) << 10,
                    .maxBitrate = (unsigned int)(config->maxBitrate) << 10 }; break;
            case HAL_VIDMODE_AVBR:
                channel.rate.mode = series == 0xEF ? I3OG_VENC_RATEMODE_H264AVBR :
                    I3_VENC_RATEMODE_H264AVBR;
                channel.rate.h264Avbr = (i3_venc_rate_h26xvbr){ .gop = config->gop, .statTime = 1,
                    .fpsNum = config->framerate, .fpsDen = 1, .maxBitrate = 
                    (unsigned int)(MAX(config->bitrate, config->maxBitrate)) << 10,
                    .maxQual = config->maxQual, .minQual = config->minQual }; break;
            default:
                HAL_ERROR("i3_venc", "H.264 encoder does not support this mode!");
        }
    } else HAL_ERROR("i3_venc", "This codec is not supported by the hardware!");
    attrib->maxHeight = config->height;
    attrib->maxWidth = config->width;
    attrib->bufSize = config->height * config->width;
    attrib->profile = MIN((series == 0xEF || config->codec == HAL_VIDCODEC_H265) ? 1 : 2,
        config->profile);
    attrib->byFrame = 1;
    attrib->height = config->height;
    attrib->width = config->width;
    attrib->bFrameNum = 0;
    attrib->refNum = 1;
attach:
    if (ret = i3_venc.fnCreateChannel(index, &channel))
        return ret;

    if (config->codec != HAL_VIDCODEC_JPG && 
        (ret = i3_venc.fnStartReceiving(index)))
        return ret;

    i3_state[index].payload = config->codec;

    return EXIT_SUCCESS;
}

int i3_video_destroy(char index)
{
    int ret;

    i3_state[index].enable = 0;
    i3_state[index].payload = HAL_VIDCODEC_UNSPEC;

    i3_venc.fnStopReceiving(index);

    {
        unsigned int device;
        if (ret = i3_venc.fnGetChannelDeviceId(index, &device))
            return ret;
        i3_sys_bind source = { .module = I3_SYS_MOD_VPE, 
            .device = _i3_vpe_dev, .channel = _i3_vpe_chn, .port = index };
        i3_sys_bind dest = { .module = I3_SYS_MOD_VENC,
            .device = device, .channel = index, .port = _i3_venc_port };
        if (ret = i3_sys.fnUnbind(&source, &dest))
            return ret;
    }

    if (ret = i3_venc.fnDestroyChannel(index))
        return ret;
    
    if (ret = i3_vpe.fnDisablePort(_i3_vpe_chn, index))
        return ret;

    return EXIT_SUCCESS;
}

int i3_video_destroy_all(void)
{
    int ret;

    for (char i = 0; i < I3_VENC_CHN_NUM; i++)
        if (i3_state[i].enable)
            if (ret = i3_video_destroy(i))
                return ret;

    return EXIT_SUCCESS;
}

void i3_video_request_idr(char index)
{
    i3_venc.fnRequestIdr(index, 1);
}

int i3_video_snapshot_grab(char index, char quality, hal_jpegdata *jpeg)
{
    int ret;

    if (ret = i3_channel_bind(index, 1)) {
        HAL_DANGER("i3_venc", "Binding the encoder channel "
            "%d failed with %#x!\n", index, ret);
        goto abort;
    }

    i3_venc_jpg param;
    memset(&param, 0, sizeof(param));
    if (ret = i3_venc.fnGetJpegParam(index, &param)) {
        HAL_DANGER("i3_venc", "Reading the JPEG settings "
            "%d failed with %#x!\n", index, ret);
        goto abort;
    }

    param.quality = quality;
    if (ret = i3_venc.fnSetJpegParam(index, &param)) {
        HAL_DANGER("i3_venc", "Writing the JPEG settings "
            "%d failed with %#x!\n", index, ret);
        goto abort;
    }

    unsigned int count = 1;
    if (ret = i3_venc.fnStartReceivingEx(index, &count)) {
        HAL_DANGER("i3_venc", "Requesting one frame "
            "%d failed with %#x!\n", index, ret);
        goto abort;
    }

    int fd = i3_venc.fnGetDescriptor(index);

    struct timeval timeout = { .tv_sec = 2, .tv_usec = 0 };
    fd_set readFds;
    FD_ZERO(&readFds);
    FD_SET(fd, &readFds);
    ret = select(fd + 1, &readFds, NULL, NULL, &timeout);
    if (ret < 0) {
        HAL_DANGER("i3_venc", "Select operation failed!\n");
        goto abort;
    } else if (ret == 0) {
        HAL_DANGER("i3_venc", "Capture stream timed out!\n");
        goto abort;
    }

    if (FD_ISSET(fd, &readFds)) {
        i3_venc_stat stat;
        if (ret = i3_venc.fnQuery(index, &stat)) {
            HAL_DANGER("i3_venc", "Querying the encoder channel "
                "%d failed with %#x!\n", index, ret);
            goto abort;
        }

        if (!stat.curPacks) {
            HAL_DANGER("i3_venc", "Current frame is empty, skipping it!\n");
            goto abort;
        }

        i3_venc_strm strm;
        memset(&strm, 0, sizeof(strm));
        strm.packet = (i3_venc_pack*)malloc(sizeof(i3_venc_pack) * stat.curPacks);
        if (!strm.packet) {
            HAL_DANGER("i3_venc", "Memory allocation on channel %d failed!\n", index);
            goto abort;
        }
        strm.count = stat.curPacks;

        if (ret = i3_venc.fnGetStream(index, &strm, stat.curPacks)) {
            HAL_DANGER("i3_venc", "Getting the stream on "
                "channel %d failed with %#x!\n", index, ret);
            free(strm.packet);
            strm.packet = NULL;
            goto abort;
        }

        {
            jpeg->jpegSize = 0;
            for (unsigned int i = 0; i < strm.count; i++) {
                i3_venc_pack *pack = &strm.packet[i];
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
        i3_venc.fnFreeStream(index, &strm);
    }

    i3_venc.fnStopReceiving(index);

    i3_channel_unbind(index);

    return ret;
}

void *i3_video_thread(void)
{
    int ret, maxFd = 0;

    for (int i = 0; i < I3_VENC_CHN_NUM; i++) {
        if (!i3_state[i].enable) continue;
        if (!i3_state[i].mainLoop) continue;

        ret = i3_venc.fnGetDescriptor(i);
        if (ret < 0) {
            HAL_DANGER("i3_venc", "Getting the encoder descriptor failed with %#x!\n", ret);
            return NULL;
        }
        i3_state[i].fileDesc = ret;

        if (maxFd <= i3_state[i].fileDesc)
            maxFd = i3_state[i].fileDesc;
    }

    i3_venc_stat stat;
    i3_venc_strm stream;
    struct timeval timeout;
    fd_set readFds;

    while (keepRunning) {
        FD_ZERO(&readFds);
        for(int i = 0; i < I3_VENC_CHN_NUM; i++) {
            if (!i3_state[i].enable) continue;
            if (!i3_state[i].mainLoop) continue;
            FD_SET(i3_state[i].fileDesc, &readFds);
        }

        timeout.tv_sec = 2;
        timeout.tv_usec = 0;
        ret = select(maxFd + 1, &readFds, NULL, NULL, &timeout);
        if (ret < 0) {
            HAL_DANGER("i3_venc", "Select operation failed!\n");
            break;
        } else if (ret == 0) {
            HAL_WARNING("i3_venc", "Main stream loop timed out!\n");
            continue;
        } else {
            for (int i = 0; i < I3_VENC_CHN_NUM; i++) {
                if (!i3_state[i].enable) continue;
                if (!i3_state[i].mainLoop) continue;
                if (FD_ISSET(i3_state[i].fileDesc, &readFds)) {
                    memset(&stream, 0, sizeof(stream));
                    
                    if (ret = i3_venc.fnQuery(i, &stat)) {
                        HAL_DANGER("i3_venc", "Querying the encoder channel "
                            "%d failed with %#x!\n", i, ret);
                        break;
                    }

                    if (!stat.curPacks) {
                        HAL_WARNING("i3_venc", "Current frame is empty, skipping it!\n");
                        continue;
                    }

                    stream.packet = (i3_venc_pack*)malloc(
                        sizeof(i3_venc_pack) * stat.curPacks);
                    if (!stream.packet) {
                        HAL_DANGER("i3_venc", "Memory allocation on channel %d failed!\n", i);
                        break;
                    }
                    stream.count = stat.curPacks;

                    if (ret = i3_venc.fnGetStream(i, &stream, 40)) {
                        HAL_DANGER("i3_venc", "Getting the stream on "
                            "channel %d failed with %#x!\n", i, ret);
                        break;
                    }

                    if (i3_vid_cb) {
                        hal_vidstream outStrm;
                        hal_vidpack outPack[stream.count];
                        outStrm.count = stream.count;
                        outStrm.seq = stream.sequence;
                        for (int j = 0; j < stream.count; j++) {
                            i3_venc_pack *pack = &stream.packet[j];
                            outPack[j].data = pack->data;
                            outPack[j].length = pack->length;
                            outPack[j].naluCnt = pack->packNum;
                            if (series == 0xEF) {
                                signed char n = 0;
                                switch (i3_state[i].payload) {
                                    case HAL_VIDCODEC_H264:
                                        for (unsigned int p = 0; p < pack->length - 4; p++) {
                                            if (outPack[j].data[p] || outPack[j].data[p + 1] ||
                                                outPack[j].data[p + 2] || outPack[j].data[p + 3] != 1) continue;
                                            outPack[0].nalu[n].type = outPack[j].data[p + 4] & 0x1F;
                                            outPack[0].nalu[n++].offset = p;
                                            if (n == (outPack[j].naluCnt)) break;
                                        }
                                        break;
                                    case HAL_VIDCODEC_H265:
                                        for (unsigned int p = 0; p < pack->length - 4; p++) {
                                            if (outPack[j].data[p] || outPack[j].data[p + 1] ||
                                                outPack[j].data[p + 2] || outPack[j].data[p + 3] != 1) continue;
                                            outPack[0].nalu[n].type = (outPack[j].data[p + 4] & 0x7E) >> 1;
                                            outPack[0].nalu[n++].offset = p;
                                            if (n == (outPack[j].naluCnt)) break;
                                        }
                                        break;
                                }

                                outPack[0].naluCnt = n;
                                outPack[0].nalu[n].offset = pack->length;
                                for (n = 0; n < outPack[0].naluCnt; n++)
                                    outPack[0].nalu[n].length = 
                                        outPack[0].nalu[n + 1].offset -
                                        outPack[0].nalu[n].offset;
                            } else switch (i3_state[i].payload) {
                                case HAL_VIDCODEC_H264:
                                    for (char k = 0; k < outPack[j].naluCnt; k++) {
                                        outPack[j].nalu[k].length =
                                            pack->packetInfo[k].length;
                                        outPack[j].nalu[k].offset =
                                            pack->packetInfo[k].offset;
                                        outPack[j].nalu[k].type =
                                            pack->packetInfo[k].packType.h264Nalu;
                                    }
                                    break;
                                case HAL_VIDCODEC_H265:
                                    for (char k = 0; k < outPack[j].naluCnt; k++) {
                                        outPack[j].nalu[k].length =
                                            pack->packetInfo[k].length;
                                        outPack[j].nalu[k].offset =
                                            pack->packetInfo[k].offset;
                                        outPack[j].nalu[k].type =
                                            pack->packetInfo[k].packType.h265Nalu;
                                    }
                                    break;
                            }
                            outPack[j].offset = pack->offset;
                            outPack[j].timestamp = pack->timestamp;
                        }
                        outStrm.pack = outPack;
                        (*i3_vid_cb)(i, &outStrm);
                    }

                    if (ret = i3_venc.fnFreeStream(i, &stream)) {
                        HAL_DANGER("i3_venc", "Releasing the stream on "
                            "channel %d failed with %#x!\n", i, ret);
                    }
                    free(stream.packet);
                    stream.packet = NULL;
                }
            }
        }
    }

    HAL_INFO("i3_venc", "Shutting down encoding thread...\n");
}*/

void i3_system_deinit(void)
{
    i3_sys.fnExit();
}

int i3_system_init(void)
{
    int ret;

    {
        i3_sys_ver version;
        if (ret = i3_sys.fnGetVersion(&version))
            return ret;
        printf("App built with headers v%s\n", I3_SYS_API);
        puts(version.version);
    }

    if (ret = i3_sys.fnInit())
        return ret;

    return EXIT_SUCCESS;
}

#endif