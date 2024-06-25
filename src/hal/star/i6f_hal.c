#ifdef __arm__

#include "i6f_hal.h"

i6f_aud_impl  i6f_aud;
i6f_isp_impl  i6f_isp;
i6f_rgn_impl  i6f_rgn;
i6f_scl_impl  i6f_scl;
i6f_snr_impl  i6f_snr;
i6f_sys_impl  i6f_sys;
i6f_venc_impl i6f_venc;
i6f_vif_impl  i6f_vif;

hal_chnstate i6f_state[I6F_VENC_CHN_NUM] = {0};
int (*i6f_aud_cb)(hal_audframe*);
int (*i6f_vid_cb)(char, hal_vidstream*);

i6f_snr_pad _i6f_snr_pad;
i6f_snr_plane _i6f_snr_plane;
char _i6f_snr_framerate, _i6f_snr_hdr, _i6f_snr_index, _i6f_snr_profile;

char _i6f_aud_chn = 0;
char _i6f_aud_dev = 0;
char _i6f_isp_chn = 0;
char _i6f_isp_dev = 0;
char _i6f_isp_port = 0;
char _i6f_scl_chn = 0;
char _i6f_scl_dev = 0;
char _i6f_venc_dev[I6F_VENC_CHN_NUM] = { 255 };
char _i6f_venc_port = 0;
char _i6f_vif_chn = 0;
char _i6f_vif_dev = 0;
char _i6f_vif_grp = 0;

void i6f_hal_deinit(void)
{
    i6f_vif_unload(&i6f_vif);
    i6f_venc_unload(&i6f_venc);
    i6f_snr_unload(&i6f_snr);
    i6f_scl_unload(&i6f_scl);
    i6f_rgn_unload(&i6f_rgn);
    i6f_isp_unload(&i6f_isp);
    i6f_aud_unload(&i6f_aud);
    i6f_sys_unload(&i6f_sys);
}

int i6f_hal_init(void)
{
    int ret;

    if (ret = i6f_sys_load(&i6f_sys))
        return ret;
    if (ret = i6f_aud_load(&i6f_aud))
        return ret;
    if (ret = i6f_isp_load(&i6f_isp))
        return ret;
    if (ret = i6f_rgn_load(&i6f_rgn))
        return ret;
    if (ret = i6f_scl_load(&i6f_scl))
        return ret;
    if (ret = i6f_snr_load(&i6f_snr))
        return ret;
    if (ret = i6f_venc_load(&i6f_venc))
        return ret;
    if (ret = i6f_vif_load(&i6f_vif))
        return ret;

    return EXIT_SUCCESS;
}

void i6f_audio_deinit(void)
{
    i6f_aud.fnDisableChannel(_i6f_aud_dev, _i6f_aud_chn);

    i6f_aud.fnDisableDevice(_i6f_aud_dev);
}


int i6f_audio_init(short samplerate)
{
    int ret;

    {
        i6f_aud_cnf config;
        config.rate = samplerate;
        config.bit = I6F_AUD_BIT_16;
        config.intf = I6F_AUD_INTF_I2S_SLAVE;
        config.sound = I6F_AUD_SND_MONO;
        config.frmNum = 0;
        config.packNumPerFrm = 640;
        config.codecChnNum = 0;
        config.chnNum = 1;
        config.i2s.clock = I6F_AUD_CLK_OFF;
        config.i2s.leftJustOn = 0;
        config.i2s.syncRxClkOn = 0;
        config.i2s.tdmSlotNum = 0;
        config.i2s.bit = I6F_AUD_BIT_16;
        if (ret = i6f_aud.fnSetDeviceConfig(_i6f_aud_dev, &config))
            return ret;
    }
    if (ret = i6f_aud.fnEnableDevice(_i6f_aud_dev))
        return ret;
    
    if (ret = i6f_aud.fnEnableChannel(_i6f_aud_dev, _i6f_aud_chn))
        return ret;
    if (ret = i6f_aud.fnSetVolume(_i6f_aud_dev, _i6f_aud_chn, 13))
        return ret;

    {
        i6f_sys_bind bind = { .module = I6F_SYS_MOD_AI, 
            .device = _i6f_aud_dev, .channel = _i6f_aud_chn };
        if (ret = i6f_sys.fnSetOutputDepth(0, &bind, 2, 4))
            return ret;
    }

    return EXIT_SUCCESS;
}

void *i6f_audio_thread(void)
{
    int ret;

    i6f_aud_frm frame;
    memset(&frame, 0, sizeof(frame));

    while (keepRunning) {
        if (ret = i6f_aud.fnGetFrame(_i6f_aud_dev, _i6f_aud_chn, 
            &frame, NULL, 128)) {
            fprintf(stderr, "[i6f_aud] Getting the frame failed "
                "with %#x!\n", ret);
            continue;
        }

        if (i6f_aud_cb) {
            hal_audframe outFrame;
            outFrame.channelCnt = 1;
            outFrame.data[0] = frame.addr[0];
            outFrame.length[0] = frame.length[0];
            outFrame.seq = frame.sequence;
            outFrame.timestamp = frame.timestamp;
            (i6f_aud_cb)(&outFrame);
        }

        if (ret = i6f_aud.fnFreeFrame(_i6f_aud_dev, _i6f_aud_chn,
            &frame, NULL)) {
            fprintf(stderr, "[i6f_aud] Releasing the frame failed"
                " with %#x!\n", ret);
        }
    }
    fprintf(stderr, "[i6f_aud] Shutting down encoding thread...\n");
}

int i6f_channel_bind(char index, char framerate)
{
    int ret;

    if (ret = i6f_scl.fnEnablePort(_i6f_scl_dev, _i6f_scl_chn, index))
        return ret;

    {
        i6f_sys_bind source = { .module = I6F_SYS_MOD_SCL, 
            .device = _i6f_scl_dev, .channel = _i6f_scl_chn, .port = index };
        i6f_sys_bind dest = { .module = I6F_SYS_MOD_VENC,
            .device = _i6f_venc_dev[index] ? I6F_VENC_DEV_MJPG_0 : I6F_VENC_DEV_H26X_0, 
            .channel = index, .port = _i6f_venc_port };
        if (ret = i6f_sys.fnBindExt(0, &source, &dest, framerate, framerate,
            _i6f_venc_dev[index] >= I6F_VENC_DEV_MJPG_0 ? 
                I6F_SYS_LINK_FRAMEBASE : I6F_SYS_LINK_REALTIME, 0))
            return ret;
    }

    return EXIT_SUCCESS;
}

int i6f_channel_create(char index, short width, short height, char mirror, char flip, char jpeg)
{
    i6f_scl_port port;
    port.crop.x = 0;
    port.crop.y = 0;
    port.crop.width = 0;
    port.crop.height = 0;
    port.output.width = width;
    port.output.height = height;
    port.mirror = mirror;
    port.flip = flip;
    port.compress = I6F_COMPR_NONE;
    port.pixFmt = jpeg ? I6F_PIXFMT_YUV422_YUYV : I6F_PIXFMT_YUV420SP;

    return i6f_scl.fnSetPortConfig(_i6f_scl_dev, _i6f_scl_chn, index, &port);
}

int i6f_channel_grayscale(char enable)
{
    return i6f_isp.fnSetColorToGray(_i6f_isp_dev, 0, &enable);
}

int i6f_channel_unbind(char index)
{
    int ret;

    if (ret = i6f_scl.fnDisablePort(_i6f_scl_dev, _i6f_scl_chn, index))
        return ret;

    {
        i6f_sys_bind source = { .module = I6F_SYS_MOD_SCL, 
            .device = _i6f_scl_dev, .channel = _i6f_scl_chn, .port = index };
        i6f_sys_bind dest = { .module = I6F_SYS_MOD_VENC,
            .device = _i6f_venc_dev[index], .channel = index, .port = _i6f_venc_port };
        if (ret = i6f_sys.fnUnbind(0, &source, &dest))
            return ret;
    }

    return EXIT_SUCCESS;    
}

int i6f_config_load(char *path)
{
    return i6f_isp.fnLoadChannelConfig(_i6f_isp_dev, _i6f_isp_chn, path, 1234);
}

int i6f_pipeline_create(char sensor, short width, short height, char framerate)
{
    int ret;

    _i6f_snr_index = sensor;
    _i6f_snr_profile = -1;

    {
        unsigned int count;
        i6f_snr_res resolution;
        if (ret = i6f_snr.fnSetPlaneMode(_i6f_snr_index, 0))
            return ret;

        if (ret = i6f_snr.fnGetResolutionCount(_i6f_snr_index, &count))
            return ret;
        for (char i = 0; i < count; i++) {
            if (ret = i6f_snr.fnGetResolution(_i6f_snr_index, i, &resolution))
                return ret;

            if (width > resolution.crop.width ||
                height > resolution.crop.height ||
                framerate > resolution.maxFps)
                continue;
        
            _i6f_snr_profile = i;
            if (ret = i6f_snr.fnSetResolution(_i6f_snr_index, _i6f_snr_profile))
                return ret;
            _i6f_snr_framerate = framerate;
            if (ret = i6f_snr.fnSetFramerate(_i6f_snr_index, _i6f_snr_framerate))
                return ret;
            break;
        }
        if (_i6f_snr_profile < 0)
            return EXIT_FAILURE;
    }

    if (ret = i6f_snr.fnGetPadInfo(_i6f_snr_index, &_i6f_snr_pad))
        return ret;
    if (ret = i6f_snr.fnGetPlaneInfo(_i6f_snr_index, 0, &_i6f_snr_plane))
        return ret;
    if (ret = i6f_snr.fnEnable(_i6f_snr_index))
        return ret;

    {
        i6f_vif_grp group;
        group.intf = _i6f_snr_pad.intf;
        group.work = I6F_VIF_WORK_1MULTIPLEX;
        group.hdr = I6F_HDR_OFF;
        group.edge = group.intf == I6F_INTF_BT656 ?
            _i6f_snr_pad.intfAttr.bt656.edge : I6F_EDGE_DOUBLE;
        group.interlaceOn = 0;
        group.grpStitch = (1 << _i6f_vif_grp);
        if (ret = i6f_vif.fnCreateGroup(_i6f_vif_grp, &group))
            return ret;
    }
    
    {
        i6f_vif_dev device;
        device.pixFmt = (i6f_common_pixfmt)(_i6f_snr_plane.bayer > I6F_BAYER_END ? 
            _i6f_snr_plane.pixFmt : (I6F_PIXFMT_RGB_BAYER + _i6f_snr_plane.precision * I6F_BAYER_END + _i6f_snr_plane.bayer));
        device.crop = _i6f_snr_plane.capt;
        device.field = 0;
        device.halfHScan = 0;
        if (ret = i6f_vif.fnSetDeviceConfig(_i6f_vif_dev, &device))
            return ret;
    }
    if (ret = i6f_vif.fnEnableDevice(_i6f_vif_dev))
        return ret;

    {
        i6f_vif_port port;
        port.capt = _i6f_snr_plane.capt;
        port.dest.height = _i6f_snr_plane.capt.height;
        port.dest.width = _i6f_snr_plane.capt.width;
        port.pixFmt = (i6f_common_pixfmt)(_i6f_snr_plane.bayer > I6F_BAYER_END ? 
            _i6f_snr_plane.pixFmt : (I6F_PIXFMT_RGB_BAYER + _i6f_snr_plane.precision * I6F_BAYER_END + _i6f_snr_plane.bayer));
        port.frate = I6F_VIF_FRATE_FULL;
        if (ret = i6f_vif.fnSetPortConfig(_i6f_vif_dev, _i6f_vif_chn, &port))
            return ret;
    }
    if (ret = i6f_vif.fnEnablePort(_i6f_vif_dev, _i6f_vif_chn))
        return ret;

    {
        unsigned int combo = 1;
        if (ret = i6f_isp.fnCreateDevice(_i6f_isp_dev, &combo))
            return ret;
    }

    {
        i6f_isp_chn channel;
        memset(&channel, 0, sizeof(channel));
        channel.sensorId = (1 << _i6f_snr_index);
        if (ret = i6f_isp.fnCreateChannel(_i6f_isp_dev, _i6f_isp_chn, &channel))
            return ret;
    }

    {
        i6f_isp_para param;
        param.hdr = I6F_HDR_OFF;
        param.level3DNR = 1;
        param.mirror = 0;
        param.flip = 0;
        param.rotate = 0;
        if (ret = i6f_isp.fnSetChannelParam(_i6f_isp_dev, _i6f_isp_chn, &param))
            return ret;
    }
    if (ret = i6f_isp.fnStartChannel(_i6f_isp_dev, _i6f_isp_chn))
        return ret;

    {
        i6f_isp_port port;
        memset(&port, 0, sizeof(port));
        port.pixFmt = I6F_PIXFMT_YUV422_YUYV;
        if (ret = i6f_isp.fnSetPortConfig(_i6f_isp_dev, _i6f_isp_chn, _i6f_isp_port, &port))
            return ret;
    }
    if (ret = i6f_isp.fnEnablePort(_i6f_isp_dev, _i6f_isp_chn, _i6f_isp_port))
        return ret;

    {
        unsigned int binds = (1 << 0) | (1 << 1) | (1 << 2) | (1 << 3);
        if (ret = i6f_scl.fnCreateDevice(_i6f_scl_dev, &binds))
            return ret;
    }

    {
        unsigned int reserved = 0;
        if (ret = i6f_scl.fnCreateChannel(_i6f_scl_dev, _i6f_scl_chn, &reserved))
            return ret;
    }
    {
        int rotate = 0;
        if (ret = i6f_scl.fnAdjustChannelRotation(_i6f_scl_dev, _i6f_scl_chn, &rotate))
            return ret;
    }
    if (ret = i6f_scl.fnStartChannel(_i6f_scl_dev, _i6f_scl_chn))
        return ret;

    {
        i6f_sys_bind source = { .module = I6F_SYS_MOD_VIF, 
            .device = _i6f_vif_dev, .channel = _i6f_vif_chn, .port = 0 };
        i6f_sys_bind dest = { .module = I6F_SYS_MOD_ISP,
            .device = _i6f_isp_dev, .channel = _i6f_isp_chn, .port = _i6f_isp_port };
        if (ret = i6f_sys.fnBindExt(0, &source, &dest, _i6f_snr_framerate, _i6f_snr_framerate,
            I6F_SYS_LINK_REALTIME, 0))
            return ret;
    }

    {
        i6f_sys_bind source = { .module = I6F_SYS_MOD_ISP, 
            .device = _i6f_isp_dev, .channel = _i6f_isp_chn, .port = _i6f_isp_port };
        i6f_sys_bind dest = { .module = I6F_SYS_MOD_SCL,
            .device = _i6f_scl_dev, .channel = _i6f_scl_chn, .port = 0 };
        return i6f_sys.fnBindExt(0, &source, &dest, _i6f_snr_framerate, _i6f_snr_framerate,
            I6F_SYS_LINK_REALTIME, 0);
    }

    return EXIT_SUCCESS;
}

void i6f_pipeline_destroy(void)
{
    for (char i = 0; i < 4; i++)
        i6f_scl.fnDisablePort(_i6f_scl_dev, _i6f_scl_chn, i);

    {
        i6f_sys_bind source = { .module = I6F_SYS_MOD_ISP, 
            .device = _i6f_isp_dev, .channel = _i6f_isp_chn, .port = _i6f_isp_port };
        i6f_sys_bind dest = { .module = I6F_SYS_MOD_SCL,
            .device = _i6f_scl_dev, .channel = _i6f_scl_chn, .port = 0 };
        i6f_sys.fnUnbind(0, &source, &dest);
    }

    i6f_scl.fnStopChannel(_i6f_scl_dev, _i6f_scl_chn);
    i6f_scl.fnDestroyChannel(_i6f_scl_dev, _i6f_scl_chn);

    i6f_scl.fnDestroyDevice(_i6f_scl_dev);

    i6f_isp.fnStopChannel(_i6f_isp_dev, _i6f_isp_chn);
    i6f_isp.fnDestroyChannel(_i6f_isp_dev, _i6f_isp_chn);

    i6f_isp.fnDestroyDevice(_i6f_isp_dev);

    {   
        i6f_sys_bind source = { .module = I6F_SYS_MOD_VIF, 
            .device = _i6f_vif_dev, .channel = _i6f_vif_chn, .port = 0 };
        i6f_sys_bind dest = { .module = I6F_SYS_MOD_ISP,
            .device = _i6f_isp_dev, .channel = _i6f_isp_chn, .port = _i6f_isp_port };
        i6f_sys.fnUnbind(0, &source, &dest);
    }

    i6f_vif.fnDisablePort(_i6f_vif_dev, 0);

    i6f_vif.fnDisableDevice(_i6f_vif_dev);

    i6f_snr.fnDisable(_i6f_snr_index);
}

int i6f_region_create(char handle, hal_rect rect, short opacity)
{
    int ret;

    i6f_sys_bind channel = { .module = I6F_SYS_MOD_SCL,
        .device = _i6f_scl_dev, .channel = _i6f_scl_chn };
    i6f_rgn_cnf region, regionCurr;
    i6f_rgn_chn attrib, attribCurr;

    region.type = I6F_RGN_TYPE_OSD;
    region.pixFmt = I6F_RGN_PIXFMT_ARGB1555;
    region.size.width = rect.width;
    region.size.height = rect.height;

    if (i6f_rgn.fnGetRegionConfig(0, handle, &regionCurr)) {
        fprintf(stderr, "[i6f_rgn] Creating region %d...\n", handle);
        if (ret = i6f_rgn.fnCreateRegion(0, handle, &region))
            return ret;
    } else if (regionCurr.size.height != region.size.height || 
        regionCurr.size.width != region.size.width) {
        fprintf(stderr, "[i6f_rgn] Parameters are different, recreating "
            "region %d...\n", handle);
        for (char i = 0; i < I6F_VENC_CHN_NUM; i++) {
            if (!i6f_state[i].enable) continue;
            channel.port = i;
            i6f_rgn.fnDetachChannel(0, handle, &channel);
        }
        i6f_rgn.fnDestroyRegion(0, handle);
        if (ret = i6f_rgn.fnCreateRegion(0, handle, &region))
            return ret;
    }

    if (i6f_rgn.fnGetChannelConfig(0, handle, &channel, &attribCurr))
        fprintf(stderr, "[i6f_rgn] Attaching region %d...\n", handle);
    else if (attribCurr.point.x != rect.x || attribCurr.point.x != rect.y ||
        attribCurr.osd.bgFgAlpha[1] != opacity) {
        fprintf(stderr, "[i6f_rgn] Parameters are different, reattaching "
            "region %d...\n", handle);
        for (char i = 0; i < I6F_VENC_CHN_NUM; i++) {
            if (!i6f_state[i].enable) continue;
            channel.port = i;
            i6f_rgn.fnDetachChannel(0, handle, &channel);
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

    for (char i = 0; i < I6F_VENC_CHN_NUM; i++) {
        if (!i6f_state[i].enable) continue;
        channel.port = i;
        i6f_rgn.fnAttachChannel(0, handle, &channel, &attrib);
    }

    return EXIT_SUCCESS;
}

void i6f_region_deinit(void)
{
    i6f_rgn.fnDeinit(0);
}

void i6f_region_destroy(char handle)
{
    i6f_sys_bind channel = { .module = I6F_SYS_MOD_SCL,
        .device = _i6f_scl_dev, .channel = _i6f_scl_chn };
    
    channel.port = 1;
    i6f_rgn.fnDetachChannel(0, handle, &channel);
    channel.port = 0;
    i6f_rgn.fnDetachChannel(0, handle, &channel);
    i6f_rgn.fnDestroyRegion(0, handle);
}

void i6f_region_init(void)
{
    i6f_rgn_pal palette = {{{0, 0, 0, 0}}};
    i6f_rgn.fnInit(0, &palette);
}

int i6f_region_setbitmap(int handle, hal_bitmap *bitmap)
{
    i6f_rgn_bmp nativeBmp = { .data = bitmap->data, .pixFmt = I6F_RGN_PIXFMT_ARGB1555,
        .size.height = bitmap->dim.height, .size.width = bitmap->dim.width };

    return i6f_rgn.fnSetBitmap(0, handle, &nativeBmp);
}

int i6f_video_create(char index, hal_vidconfig *config)
{
    int ret;
    i6f_venc_chn channel;
    i6f_venc_attr_h26x *attrib;
    
    if (config->codec == HAL_VIDCODEC_JPG || config->codec == HAL_VIDCODEC_MJPG) {
        _i6f_venc_dev[index] = I6F_VENC_DEV_MJPG_0;
        channel.attrib.codec = I6F_VENC_CODEC_MJPG;
        switch (config->mode) {
            case HAL_VIDMODE_CBR:
                channel.rate.mode = I6F_VENC_RATEMODE_MJPGCBR;
                channel.rate.mjpgCbr.bitrate = config->bitrate << 10;
                channel.rate.mjpgCbr.fpsNum = 
                    config->codec == HAL_VIDCODEC_JPG ? 1 : config->framerate;
                channel.rate.mjpgCbr.fpsDen = 1;
                break;
            case HAL_VIDMODE_QP:
                channel.rate.mode = I6F_VENC_RATEMODE_MJPGQP;
                channel.rate.mjpgQp.fpsNum = config->framerate;
                channel.rate.mjpgQp.fpsDen = 
                    config->codec == HAL_VIDCODEC_JPG ? 1 : config->framerate;
                channel.rate.mjpgQp.quality = MAX(config->minQual, config->maxQual);
                break;
            default:
                I6F_ERROR("MJPEG encoder can only support CBR or fixed QP modes!");
        }

        channel.attrib.mjpg.maxHeight = ALIGN_UP(config->height, 2);
        channel.attrib.mjpg.maxWidth = ALIGN_UP(config->width, 8);
        channel.attrib.mjpg.bufSize = ALIGN_UP(config->width, 8) * ALIGN_UP(config->height, 2);
        channel.attrib.mjpg.byFrame = 1;
        channel.attrib.mjpg.height = ALIGN_UP(config->height, 2);
        channel.attrib.mjpg.width = ALIGN_UP(config->width, 8);
        channel.attrib.mjpg.dcfThumbs = 0;
        channel.attrib.mjpg.markPerRow = 0;

        goto attach;
    } else if (config->codec == HAL_VIDCODEC_H265) {
        channel.attrib.codec = I6F_VENC_CODEC_H265;
        attrib = &channel.attrib.h265;
        switch (config->mode) {
            case HAL_VIDMODE_CBR:
                channel.rate.mode = I6F_VENC_RATEMODE_H265CBR;
                channel.rate.h265Cbr = (i6f_venc_rate_h26xcbr){ .gop = config->gop,
                    .statTime = 1, .fpsNum = config->framerate, .fpsDen = 1, .bitrate = 
                    (unsigned int)(config->bitrate) << 10, .avgLvl = 1 }; break;
            case HAL_VIDMODE_VBR:
                channel.rate.mode = I6F_VENC_RATEMODE_H265VBR;
                channel.rate.h265Vbr = (i6f_venc_rate_h26xvbr){ .gop = config->gop,
                    .statTime = 1, .fpsNum = config->framerate, .fpsDen = 1, .maxBitrate = 
                    (unsigned int)(MAX(config->bitrate, config->maxBitrate)) << 10,
                    .maxQual = config->maxQual, .minQual = config->minQual }; break;
            case HAL_VIDMODE_QP:
                channel.rate.mode = I6F_VENC_RATEMODE_H265QP;
                channel.rate.h265Qp = (i6f_venc_rate_h26xqp){ .gop = config->gop,
                    .fpsNum =  config->framerate, .fpsDen = 1, .interQual = config->maxQual,
                    .predQual = config->minQual }; break;
            case HAL_VIDMODE_ABR:
                I6F_ERROR("H.265 encoder does not support ABR mode!");
            case HAL_VIDMODE_AVBR:
                channel.rate.mode = I6F_VENC_RATEMODE_H265AVBR;
                channel.rate.h265Avbr = (i6f_venc_rate_h26xvbr){ .gop = config->gop,
                    .statTime = 1, .fpsNum = config->framerate, .fpsDen = 1, .maxBitrate = 
                    (unsigned int)(MAX(config->bitrate, config->maxBitrate)) << 10,
                    .maxQual = config->maxQual, .minQual = config->minQual }; break;
            default:
                I6F_ERROR("H.265 encoder does not support this mode!");
        }  
    } else if (config->codec == HAL_VIDCODEC_H264) {
        channel.attrib.codec = I6F_VENC_CODEC_H264;
        attrib = &channel.attrib.h264;
        switch (config->mode) {
            case HAL_VIDMODE_CBR:
                channel.rate.mode = I6F_VENC_RATEMODE_H264CBR;
                channel.rate.h264Cbr = (i6f_venc_rate_h26xcbr){ .gop = config->gop,
                    .statTime = 1, .fpsNum = config->framerate, .fpsDen = 1, .bitrate = 
                    (unsigned int)(config->bitrate) << 10, .avgLvl = 1 }; break;
            case HAL_VIDMODE_VBR:
                channel.rate.mode = I6F_VENC_RATEMODE_H264VBR;
                channel.rate.h264Vbr = (i6f_venc_rate_h26xvbr){ .gop = config->gop,
                    .statTime = 1, .fpsNum = config->framerate, .fpsDen = 1, .maxBitrate = 
                    (unsigned int)(MAX(config->bitrate, config->maxBitrate)) << 10,
                    .maxQual = config->maxQual, .minQual = config->minQual }; break;
            case HAL_VIDMODE_QP:
                channel.rate.mode = I6F_VENC_RATEMODE_H264QP;
                channel.rate.h264Qp = (i6f_venc_rate_h26xqp){ .gop = config->gop,
                    .fpsNum = config->framerate, .fpsDen = 1, .interQual = config->maxQual,
                    .predQual = config->minQual }; break;
            case HAL_VIDMODE_ABR:
                channel.rate.mode = I6F_VENC_RATEMODE_H264ABR;
                channel.rate.h264Abr = (i6f_venc_rate_h26xabr){ .gop = config->gop,
                    .statTime = 1, .fpsNum = config->framerate, .fpsDen = 1,
                    .avgBitrate = (unsigned int)(config->bitrate) << 10,
                    .maxBitrate = (unsigned int)(config->maxBitrate) << 10 }; break;
            case HAL_VIDMODE_AVBR:
                channel.rate.mode = I6F_VENC_RATEMODE_H265AVBR;
                channel.rate.h265Avbr = (i6f_venc_rate_h26xvbr){ .gop = config->gop,
                    .statTime = 1, .fpsNum = config->framerate, .fpsDen = 1, .maxBitrate = 
                    (unsigned int)(MAX(config->bitrate, config->maxBitrate)) << 10,
                    .maxQual = config->maxQual, .minQual = config->minQual }; break;
            default:
                I6F_ERROR("H.264 encoder does not support this mode!");
        }
    } else I6F_ERROR("This codec is not supported by the hardware!");
    _i6f_venc_dev[index] = I6F_VENC_DEV_H26X_0;
    attrib->maxHeight = config->height;
    attrib->maxWidth = config->width;
    attrib->bufSize = config->height * config->width;
    attrib->profile = config->profile;
    attrib->byFrame = 1;
    attrib->height = config->height;
    attrib->width = config->width;
    attrib->bFrameNum = 0;
    attrib->refNum = 1;
attach:
    if (ret = i6f_venc.fnCreateChannel(_i6f_venc_dev[index], index, &channel))
        return ret;

    if (config->codec != HAL_VIDCODEC_JPG && 
        (ret = i6f_venc.fnStartReceiving(_i6f_venc_dev[index], index)))
        return ret;

    i6f_state[index].payload = config->codec;

    return EXIT_SUCCESS;
}

int i6f_video_destroy(char index)
{
    int ret;

    i6f_state[index].enable = 0;
    i6f_state[index].payload = HAL_VIDCODEC_UNSPEC;

    i6f_venc.fnStopReceiving(_i6f_venc_dev[index], index);

    {
        i6f_sys_bind source = { .module = I6F_SYS_MOD_SCL, 
            .device = _i6f_scl_dev, .channel = _i6f_scl_chn, .port = index };
        i6f_sys_bind dest = { .module = I6F_SYS_MOD_VENC,
            .device = _i6f_venc_dev[index], .channel = index, .port = _i6f_venc_port };
        if (ret = i6f_sys.fnUnbind(0, &source, &dest))
            return ret;
    }

    if (ret = i6f_venc.fnDestroyChannel(_i6f_venc_dev[index], index))
        return ret;
    
    if (ret = i6f_scl.fnDisablePort(_i6f_scl_dev, _i6f_scl_chn, index))
        return ret;

    _i6f_venc_dev[index] = 255;
    
    return EXIT_SUCCESS;
}

int i6f_video_destroy_all(void)
{
    int ret;

    for (char i = 0; i < I6F_VENC_CHN_NUM; i++)
        if (i6f_state[i].enable)
            if (ret = i6f_video_destroy(i))
                return ret;
    
    return EXIT_SUCCESS;
}

void i6f_video_request_idr(char index)
{
    if (i6f_state[index].payload == HAL_VIDCODEC_JPG ||
        i6f_state[index].payload == HAL_VIDCODEC_MJPG) return;

    i6f_venc.fnRequestIdr(I6F_VENC_DEV_H26X_0, index, 1);
}

int i6f_video_snapshot_grab(char index, char quality, hal_jpegdata *jpeg)
{
    int ret;

    if (ret = i6f_channel_bind(index, 1)) {
        fprintf(stderr, "[i6f_venc] Binding the encoder channel "
            "%d failed with %#x!\n", index, ret);
        goto abort;
    }
    return ret;

    i6f_venc_jpg param;
    memset(&param, 0, sizeof(param));
    if (ret = i6f_venc.fnGetJpegParam(_i6f_venc_dev[index], index, &param)) {
        fprintf(stderr, "[i6f_venc] Reading the JPEG settings "
            "%d failed with %#x!\n", index, ret);
        goto abort;
    }
    return ret;
        return ret;
    param.quality = quality;
    if (ret = i6f_venc.fnSetJpegParam(_i6f_venc_dev[index], index, &param)) {
        fprintf(stderr, "[i6f_venc] Writing the JPEG settings "
            "%d failed with %#x!\n", index, ret);
        goto abort;
    }

    unsigned int count = 1;
    if (i6f_venc.fnStartReceivingEx(_i6f_venc_dev[index], index, &count)) {
        fprintf(stderr, "[i6f_venc] Requesting one frame "
            "%d failed with %#x!\n", index, ret);
        goto abort;
    }

    int fd = i6f_venc.fnGetDescriptor(_i6f_venc_dev[index], index);

    struct timeval timeout = { .tv_sec = 2, .tv_usec = 0 };
    fd_set readFds;
    FD_ZERO(&readFds);
    FD_SET(fd, &readFds);
    ret = select(fd + 1, &readFds, NULL, NULL, &timeout);
    if (ret < 0) {
        fprintf(stderr, "[i6f_venc] Select operation failed!\n");
        goto abort;
    } else if (ret == 0) {
        fprintf(stderr, "[i6f_venc] Capture stream timed out!\n");
        goto abort;
    }

    if (FD_ISSET(fd, &readFds)) {
        i6f_venc_stat stat;
        if (i6f_venc.fnQuery(_i6f_venc_dev[index], index, &stat)) {
            fprintf(stderr, "[i6f_venc] Querying the encoder channel "
                "%d failed with %#x!\n", index, ret);
            goto abort;
        }

        if (!stat.curPacks) {
            fprintf(stderr, "[i6f_venc] Current frame is empty, skipping it!\n");
            goto abort;
        }

        i6f_venc_strm strm;
        memset(&strm, 0, sizeof(strm));
        strm.packet = (i6f_venc_pack*)malloc(sizeof(i6f_venc_pack) * stat.curPacks);
        if (!strm.packet) {
            fprintf(stderr, "[i6f_venc] Memory allocation on channel %d failed!\n", index);
            goto abort;
        }
        strm.count = stat.curPacks;

        if (ret = i6f_venc.fnGetStream(_i6f_venc_dev[index], index, &strm, stat.curPacks)) {
            fprintf(stderr, "[i6f_venc] Getting the stream on "
                "channel %d failed with %#x!\n", index, ret);
            free(strm.packet);
            strm.packet = NULL;
            goto abort;
        }

        {
            jpeg->jpegSize = 0;
            for (unsigned int i = 0; i < strm.count; i++) {
                i6f_venc_pack *pack = &strm.packet[i];
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
        i6f_venc.fnFreeStream(_i6f_venc_dev[index], index, &strm);
    }

    i6f_venc.fnFreeDescriptor(_i6f_venc_dev[index], index);

    i6f_venc.fnStopReceiving(_i6f_venc_dev[index], index);

    i6f_channel_unbind(index);

    return ret;    
}

void *i6f_video_thread(void)
{
    int ret;
    int maxFd = 0;

    for (int i = 0; i < I6F_VENC_CHN_NUM; i++) {
        if (!i6f_state[i].enable) continue;
        if (!i6f_state[i].mainLoop) continue;

        ret = i6f_venc.fnGetDescriptor(_i6f_venc_dev[i], i);
        if (ret < 0) {
            fprintf(stderr, "[i6f_venc] Getting the encoder descriptor failed with %#x!\n", ret);
            return NULL;
        }
        i6f_state[i].fileDesc = ret;

        if (maxFd <= i6f_state[i].fileDesc)
            maxFd = i6f_state[i].fileDesc;
    }

    i6f_venc_stat stat;
    i6f_venc_strm stream;
    struct timeval timeout;
    fd_set readFds;

    while (keepRunning) {
        FD_ZERO(&readFds);
        for(int i = 0; i < I6F_VENC_CHN_NUM; i++) {
            if (!i6f_state[i].enable) continue;
            if (!i6f_state[i].mainLoop) continue;
            FD_SET(i6f_state[i].fileDesc, &readFds);
        }

        timeout.tv_sec = 2;
        timeout.tv_usec = 0;
        ret = select(maxFd + 1, &readFds, NULL, NULL, &timeout);
        if (ret < 0) {
            fprintf(stderr, "[i6f_venc] Select operation failed!\n");
            break;
        } else if (ret == 0) {
            fprintf(stderr, "[i6f_venc] Main stream loop timed out!\n");
            continue;
        } else {
            for (int i = 0; i < I6F_VENC_CHN_NUM; i++) {
                if (!i6f_state[i].enable) continue;
                if (!i6f_state[i].mainLoop) continue;
                if (FD_ISSET(i6f_state[i].fileDesc, &readFds)) {

                    memset(&stream, 0, sizeof(stream));
                    
                    if (ret = i6f_venc.fnQuery(_i6f_venc_dev[i], i, &stat)) {
                        fprintf(stderr, "[i6f_venc] Querying the encoder channel "
                            "%d failed with %#x!\n", i, ret);
                        break;
                    }

                    if (!stat.curPacks) {
                        fprintf(stderr, "[i6f_venc] Current frame is empty, skipping it!\n");
                        continue;
                    }

                    stream.packet = (i6f_venc_pack*)malloc(
                        sizeof(i6f_venc_pack) * stat.curPacks);
                    if (!stream.packet) {
                        fprintf(stderr, "[i6f_venc] Memory allocation on channel %d failed!\n", i);
                        break;
                    }
                    stream.count = stat.curPacks;

                    if (ret = i6f_venc.fnGetStream(_i6f_venc_dev[i], i, &stream, 40)) {
                        fprintf(stderr, "[i6f_venc] Getting the stream on "
                            "channel %d failed with %#x!\n", i, ret);
                        break;
                    }

                    if (i6f_vid_cb) {
                        hal_vidstream outStrm;
                        hal_vidpack outPack[stream.count];
                        outStrm.count = stream.count;
                        outStrm.seq = stream.sequence;
                        for (int j = 0; j < stream.count; j++) {
                            i6f_venc_pack *pack = &stream.packet[j];
                            outPack[j].data = pack->data;
                            outPack[j].length = pack->length;
                            outPack[j].naluCnt = pack->packNum;
                            switch (i6f_state[i].payload) {
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
                        (*i6f_vid_cb)(i, &outStrm);
                    }

                    if (ret = i6f_venc.fnFreeStream(_i6f_venc_dev[i], i, &stream)) {
                        fprintf(stderr, "[i6f_venc] Releasing the stream on "
                            "channel %d failed with %#x!\n", i, ret);
                    }
                    free(stream.packet);
                    stream.packet = NULL;
                }
            }
        }
    }
    fprintf(stderr, "[i6f_venc] Shutting down encoding thread...\n");
}

void i6f_system_deinit(void)
{
    i6f_sys.fnExit(0);
}

int i6f_system_init(void)
{
    int ret;

    if (ret = i6f_sys.fnInit(0))
        return ret;
    {
        i6f_sys_ver version;
        if (ret = i6f_sys.fnGetVersion(0, &version))
            return ret;
        printf("App built with headers v%s\n", I6F_SYS_API);
        puts(version.version);
    }

    return EXIT_SUCCESS;
}

#endif