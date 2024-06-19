#ifdef __arm__

#include "i6c_hal.h"

i6c_aud_impl  i6c_aud;
i6c_isp_impl  i6c_isp;
i6c_rgn_impl  i6c_rgn;
i6c_scl_impl  i6c_scl;
i6c_snr_impl  i6c_snr;
i6c_sys_impl  i6c_sys;
i6c_venc_impl i6c_venc;
i6c_vif_impl  i6c_vif;

hal_chnstate i6c_state[I6C_VENC_CHN_NUM] = {0};
int (*i6c_aud_cb)(hal_audframe*);
int (*i6c_vid_cb)(char, hal_vidstream*);

i6c_snr_pad _i6c_snr_pad;
i6c_snr_plane _i6c_snr_plane;
char _i6c_snr_framerate, _i6c_snr_hdr, _i6c_snr_index, _i6c_snr_profile;

char _i6c_aud_chn = 0;
char _i6c_aud_dev = 0;
char _i6c_isp_chn = 0;
char _i6c_isp_dev = 0;
char _i6c_isp_port = 0;
char _i6c_scl_chn = 0;
char _i6c_scl_dev = 0;
char _i6c_venc_dev[I6C_VENC_CHN_NUM] = { 255 };
char _i6c_venc_port = 0;
char _i6c_vif_chn = 0;
char _i6c_vif_dev = 0;
char _i6c_vif_grp = 0;

void i6c_hal_deinit(void)
{
    i6c_vif_unload(&i6c_vif);
    i6c_venc_unload(&i6c_venc);
    i6c_snr_unload(&i6c_snr);
    i6c_scl_unload(&i6c_scl);
    i6c_rgn_unload(&i6c_rgn);
    i6c_isp_unload(&i6c_isp);
    i6c_aud_unload(&i6c_aud);
    i6c_sys_unload(&i6c_sys);
}

int i6c_hal_init(void)
{
    int ret;

    if (ret = i6c_sys_load(&i6c_sys))
        return ret;
    if (ret = i6c_aud_load(&i6c_aud))
        return ret;
    if (ret = i6c_isp_load(&i6c_isp))
        return ret;
    if (ret = i6c_rgn_load(&i6c_rgn))
        return ret;
    if (ret = i6c_scl_load(&i6c_scl))
        return ret;
    if (ret = i6c_snr_load(&i6c_snr))
        return ret;
    if (ret = i6c_venc_load(&i6c_venc))
        return ret;
    if (ret = i6c_vif_load(&i6c_vif))
        return ret;

    return EXIT_SUCCESS;
}

void i6c_audio_deinit(void)
{
    i6c_aud.fnDisableGroup(_i6c_aud_dev, _i6c_aud_chn);

    i6c_aud.fnDisableDevice(_i6c_aud_dev);
}

int i6c_audio_init(void)
{
    int ret;

    {
        i6c_aud_cnf config;
        config.reserved = 0;
        config.sound = I6C_AUD_SND_MONO;
        config.rate = 48000;
        config.periodSize = 0x600;
        config.interleavedOn = 0;
        if (ret = i6c_aud.fnEnableDevice(_i6c_aud_dev, &config))
            return ret;
    }
    {
        i6c_aud_input input[] = { I6C_AUD_INPUT_I2S_A_01 };
        i6c_aud_i2s config;
        config.intf = I6C_AUD_INTF_I2S_SLAVE;
        config.bit = I6C_AUD_BIT_16;
        config.leftJustOn = 0;
        config.rate = 48000;
        config.clock = I6C_AUD_CLK_OFF;
        config.syncRxClkOn = 1;
        config.tdmSlotNum = 0;
        if (ret = i6c_aud.fnSetI2SConfig(input[0], &config))
            return ret;
        if (ret = i6c_aud.fnAttachToDevice(_i6c_aud_dev, input, 1))
            return ret;
    }

    {
        char gain[1] = { 0xF6 };
        if (ret = i6c_aud.fnSetGain(_i6c_aud_dev, _i6c_aud_chn, gain, 1))
            return ret;
    }
    if (ret = i6c_aud.fnEnableGroup(_i6c_aud_dev, _i6c_aud_chn))
        return ret;

    return EXIT_SUCCESS;
}

void *i6c_audio_thread(void)
{
    int ret;

    i6c_aud_frm frame, echoFrame;
    memset(&frame, 0, sizeof(frame));
    memset(&echoFrame, 0, sizeof(echoFrame));

    while (keepRunning) {
        if (ret = i6c_aud.fnGetFrame(_i6c_aud_dev, _i6c_aud_chn, 
            &frame, &echoFrame, 100)) {
            fprintf(stderr, "[i6c_aud] Getting the frame failed "
                "with %#x!\n", ret);
            continue;
        }

        if (i6c_aud_cb) {
            hal_audframe outFrame;
            (i6c_aud_cb)(&outFrame);
        }

        if (ret = i6c_aud.fnFreeFrame(_i6c_aud_dev, _i6c_aud_chn,
            &frame, &echoFrame)) {
            fprintf(stderr, "[i6c_aud] Releasing the frame failed"
                " with %#x!\n", ret);
        }
    }
    fprintf(stderr, "[i6c_aud] Shutting down encoding thread...\n");
}

int i6c_channel_bind(char index, char framerate)
{
    int ret;

    if (ret = i6c_scl.fnEnablePort(_i6c_scl_dev, _i6c_scl_chn, index))
        return ret;

    {
        i6c_sys_bind source = { .module = I6C_SYS_MOD_SCL, 
            .device = _i6c_scl_dev, .channel = _i6c_scl_chn, .port = index };
        i6c_sys_bind dest = { .module = I6C_SYS_MOD_VENC,
            .device = _i6c_venc_dev[index],
            .channel = index, .port = _i6c_venc_port };
        if (ret = i6c_sys.fnBindExt(0, &source, &dest, framerate, framerate,
            _i6c_venc_dev[index] >= I6C_VENC_DEV_MJPG_0 ?
                I6C_SYS_LINK_FRAMEBASE : I6C_SYS_LINK_RING, 0))
            return ret;
    }

    return EXIT_SUCCESS;
}

int i6c_channel_create(char index, short width, short height, char mirror, char flip, char jpeg)
{
    i6c_scl_port port;
    port.crop.x = 0;
    port.crop.y = 0;
    port.crop.width = 0;
    port.crop.height = 0;
    port.output.width = width;
    port.output.height = height;
    port.mirror = mirror;
    port.flip = flip;
    port.compress = jpeg ? I6C_COMPR_NONE : I6C_COMPR_IFC;
    port.pixFmt = jpeg ? I6C_PIXFMT_YUV422_YUYV : I6C_PIXFMT_YUV420SP;

    return i6c_scl.fnSetPortConfig(_i6c_scl_dev, _i6c_scl_chn, index, &port);
}

int i6c_channel_grayscale(char enable)
{
    return i6c_isp.fnSetColorToGray(_i6c_isp_dev, 0, &enable);
}

int i6c_channel_unbind(char index)
{
    int ret;

    if (ret = i6c_scl.fnDisablePort(_i6c_scl_dev, _i6c_scl_chn, index))
        return ret;

    {
        i6c_sys_bind source = { .module = I6C_SYS_MOD_SCL, 
            .device = _i6c_scl_dev, .channel = _i6c_scl_chn, .port = index };
        i6c_sys_bind dest = { .module = I6C_SYS_MOD_VENC,
            .device = _i6c_venc_dev[index], .channel = index, .port = _i6c_venc_port };
        if (ret = i6c_sys.fnUnbind(0, &source, &dest))
            return ret;
    }

    return EXIT_SUCCESS;
}

int i6c_config_load(char *path)
{
    return i6c_isp.fnLoadChannelConfig(_i6c_isp_dev, _i6c_isp_chn, path, 1234);
}

int i6c_pipeline_create(char sensor, short width, short height, char framerate)
{
    int ret;

    _i6c_snr_index = sensor;
    _i6c_snr_profile = -1;

    {
        unsigned int count;
        i6c_snr_res resolution;
        if (ret = i6c_snr.fnSetPlaneMode(_i6c_snr_index, 0))
            return ret;

        if (ret = i6c_snr.fnGetResolutionCount(_i6c_snr_index, &count))
            return ret;
        for (char i = 0; i < count; i++) {
            if (ret = i6c_snr.fnGetResolution(_i6c_snr_index, i, &resolution))
                return ret;

            if (width > resolution.crop.width ||
                height > resolution.crop.height ||
                framerate > resolution.maxFps)
                continue;
        
            _i6c_snr_profile = i;
            if (ret = i6c_snr.fnSetResolution(_i6c_snr_index, _i6c_snr_profile))
                return ret;
            _i6c_snr_framerate = framerate;
            if (ret = i6c_snr.fnSetFramerate(_i6c_snr_index, _i6c_snr_framerate))
                return ret;
            break;
        }
        if (_i6c_snr_profile < 0)
            return EXIT_FAILURE;
    }

    if (ret = i6c_snr.fnGetPadInfo(_i6c_snr_index, &_i6c_snr_pad))
        return ret;
    if (ret = i6c_snr.fnGetPlaneInfo(_i6c_snr_index, 0, &_i6c_snr_plane))
        return ret;

    i6c_sys_pool pool;
    memset(&pool, 0, sizeof(pool));
    pool.type = I6C_SYS_POOL_DEVICE_RING;
    pool.create = 1;
    pool.config.ring.module = I6C_SYS_MOD_SCL;
    pool.config.ring.device = _i6c_scl_dev;
    pool.config.ring.maxHeight = _i6c_snr_plane.capt.height;
    pool.config.ring.maxWidth = _i6c_snr_plane.capt.width;
    pool.config.ring.ringLine = _i6c_snr_plane.capt.height / 4;
    if (ret = i6c_sys.fnConfigPool(0, &pool))
        return ret;

    if (ret = i6c_snr.fnEnable(_i6c_snr_index))
        return ret;

    {
        i6c_vif_grp group;
        group.intf = _i6c_snr_pad.intf;
        group.work = I6C_VIF_WORK_1MULTIPLEX;
        group.hdr = I6C_HDR_OFF;
        group.edge = group.intf == I6C_INTF_BT656 ?
            _i6c_snr_pad.intfAttr.bt656.edge : I6C_EDGE_DOUBLE;
        group.interlaceOn = 0;
        group.grpStitch = (1 << _i6c_vif_grp);
        if (ret = i6c_vif.fnCreateGroup(_i6c_vif_grp, &group))
            return ret;
    }
    
    {
        i6c_vif_dev device;
        device.pixFmt = (i6c_common_pixfmt)(_i6c_snr_plane.bayer > I6C_BAYER_END ? 
            _i6c_snr_plane.pixFmt : (I6C_PIXFMT_RGB_BAYER + _i6c_snr_plane.precision * I6C_BAYER_END + _i6c_snr_plane.bayer));
        device.crop = _i6c_snr_plane.capt;
        device.field = 0;
        device.halfHScan = 0;
        if (ret = i6c_vif.fnSetDeviceConfig(_i6c_vif_dev, &device))
            return ret;
    }
    if (ret = i6c_vif.fnEnableDevice(_i6c_vif_dev))
        return ret;
    
    {
        i6c_vif_port port;
        port.capt = _i6c_snr_plane.capt;
        port.dest.height = _i6c_snr_plane.capt.height;
        port.dest.width = _i6c_snr_plane.capt.width;
        port.pixFmt = (i6c_common_pixfmt)(_i6c_snr_plane.bayer > I6C_BAYER_END ? 
            _i6c_snr_plane.pixFmt : (I6C_PIXFMT_RGB_BAYER + _i6c_snr_plane.precision * I6C_BAYER_END + _i6c_snr_plane.bayer));
        port.frate = I6C_VIF_FRATE_FULL;
        port.compress = I6C_COMPR_NONE;
        if (ret = i6c_vif.fnSetPortConfig(_i6c_vif_dev, _i6c_vif_chn, &port))
            return ret;
    }
    if (ret = i6c_vif.fnEnablePort(_i6c_vif_dev, _i6c_vif_chn))
        return ret;

    {
        unsigned int combo = 1;
        if (ret = i6c_isp.fnCreateDevice(_i6c_isp_dev, &combo))
            return ret;
    }


    {
        i6c_isp_chn channel;
        memset(&channel, 0, sizeof(channel));
        channel.sensorId = (1 << _i6c_snr_index);
        if (ret = i6c_isp.fnCreateChannel(_i6c_isp_dev, _i6c_isp_chn, &channel))
            return ret;
    }

    {
        i6c_isp_para param;
        param.hdr = I6C_HDR_OFF;
        param.level3DNR = 1;
        param.mirror = 0;
        param.flip = 0;
        param.rotate = 0;
        param.yuv2BayerOn = _i6c_snr_plane.bayer > I6C_BAYER_END;
        if (ret = i6c_isp.fnSetChannelParam(_i6c_isp_dev, _i6c_isp_chn, &param))
            return ret;
    }
    if (ret = i6c_isp.fnStartChannel(_i6c_isp_dev, _i6c_isp_chn))
        return ret;

    {
        i6c_isp_port port;
        memset(&port, 0, sizeof(port));
        port.pixFmt = I6C_PIXFMT_YUV422_YUYV;
        if (ret = i6c_isp.fnSetPortConfig(_i6c_isp_dev, _i6c_isp_chn, _i6c_isp_port, &port))
            return ret;
    }
    if (ret = i6c_isp.fnEnablePort(_i6c_isp_dev, _i6c_isp_chn, _i6c_isp_port))
        return ret;

    {
        unsigned int binds = (1 << 0) | (1 << 1) | (1 << 2) | (1 << 3);
        if (ret = i6c_scl.fnCreateDevice(_i6c_scl_dev, &binds))
            return ret;
    }

    {
        unsigned int reserved = 0;
        if (ret = i6c_scl.fnCreateChannel(_i6c_scl_dev, _i6c_scl_chn, &reserved))
            return ret;
    }
    {
        int rotate = 0;
        if (ret = i6c_scl.fnAdjustChannelRotation(_i6c_scl_dev, _i6c_scl_chn, &rotate))
            return ret;
    }
    if (ret = i6c_scl.fnStartChannel(_i6c_scl_dev, _i6c_scl_chn))
        return ret;

    {
        i6c_sys_bind source = { .module = I6C_SYS_MOD_VIF, 
            .device = _i6c_vif_dev, .channel = _i6c_vif_chn, .port = 0 };
        i6c_sys_bind dest = { .module = I6C_SYS_MOD_ISP,
            .device = _i6c_isp_dev, .channel = _i6c_isp_chn, .port = _i6c_isp_port };
        if (ret = i6c_sys.fnBindExt(0, &source, &dest, _i6c_snr_framerate, _i6c_snr_framerate,
            I6C_SYS_LINK_REALTIME, 0))
            return ret;
    }

    {
        i6c_sys_bind source = { .module = I6C_SYS_MOD_ISP, 
            .device = _i6c_isp_dev, .channel = _i6c_isp_chn, .port = _i6c_isp_port };
        i6c_sys_bind dest = { .module = I6C_SYS_MOD_SCL,
            .device = _i6c_scl_dev, .channel = _i6c_scl_chn, .port = 0 };
        return i6c_sys.fnBindExt(0, &source, &dest, _i6c_snr_framerate, _i6c_snr_framerate,
            I6C_SYS_LINK_REALTIME, 0);
    }

    return EXIT_SUCCESS;
}

void i6c_pipeline_destroy(void)
{
    for (char i = 0; i < 4; i++)
        i6c_scl.fnDisablePort(_i6c_scl_dev, _i6c_scl_chn, i);

    {
        i6c_sys_bind source = { .module = I6C_SYS_MOD_ISP, 
            .device = _i6c_isp_dev, .channel = _i6c_isp_chn, .port = _i6c_isp_port };
        i6c_sys_bind dest = { .module = I6C_SYS_MOD_SCL,
            .device = _i6c_scl_dev, .channel = _i6c_scl_chn, .port = 0 };
        i6c_sys.fnUnbind(0, &source, &dest);
    }

    i6c_scl.fnStopChannel(_i6c_scl_dev, _i6c_scl_chn);
    i6c_scl.fnDestroyChannel(_i6c_scl_dev, _i6c_scl_chn);

    i6c_scl.fnDestroyDevice(_i6c_scl_dev);

    i6c_isp.fnStopChannel(_i6c_isp_dev, _i6c_isp_chn);
    i6c_isp.fnDestroyChannel(_i6c_isp_dev, _i6c_isp_chn);

    i6c_isp.fnDestroyDevice(_i6c_isp_dev);

    {   
        i6c_sys_bind source = { .module = I6C_SYS_MOD_VIF, 
            .device = _i6c_vif_dev, .channel = _i6c_vif_chn, .port = 0 };
        i6c_sys_bind dest = { .module = I6C_SYS_MOD_ISP,
            .device = _i6c_isp_dev, .channel = _i6c_isp_chn, .port = _i6c_isp_port };
        i6c_sys.fnUnbind(0, &source, &dest);
    }

    i6c_vif.fnDisablePort(_i6c_vif_dev, 0);

    i6c_vif.fnDisableDevice(_i6c_vif_dev);

    i6c_snr.fnDisable(_i6c_snr_index);
}

int i6c_region_create(char handle, hal_rect rect, short opacity)
{
    int ret = EXIT_SUCCESS;

    i6c_sys_bind channel = { .module = I6C_SYS_MOD_SCL,
        .device = _i6c_scl_dev, .channel = _i6c_scl_chn };
    i6c_rgn_cnf region, regionCurr;
    i6c_rgn_chn attrib, attribCurr;

    region.type = I6C_RGN_TYPE_OSD;
    region.pixFmt = I6C_RGN_PIXFMT_ARGB1555;
    region.size.width = rect.width;
    region.size.height = rect.height;

    if (i6c_rgn.fnGetRegionConfig(0, handle, &regionCurr)) {
        fprintf(stderr, "[i6c_rgn] Creating region %d...\n", handle);
        if (ret = i6c_rgn.fnCreateRegion(0, handle, &region))
            return ret;
    } else if (regionCurr.size.height != region.size.height || 
        regionCurr.size.width != region.size.width) {
        fprintf(stderr, "[i6c_rgn] Parameters are different, recreating "
            "region %d...\n", handle);
        for (char i = 0; i < I6C_VENC_CHN_NUM; i++) {
            if (!i6c_state[i].enable) continue;
            channel.port = i;
            i6c_rgn.fnDetachChannel(0, handle, &channel);
        }
        i6c_rgn.fnDestroyRegion(0, handle);
        if (ret = i6c_rgn.fnCreateRegion(0, handle, &region))
            return ret;
    }

    if (i6c_rgn.fnGetChannelConfig(0, handle, &channel, &attribCurr))
        fprintf(stderr, "[i6c_rgn] Attaching region %d...\n", handle);
    else if (attribCurr.point.x != rect.x || attribCurr.point.x != rect.y ||
        attribCurr.osd.bgFgAlpha[1] != opacity) {
        fprintf(stderr, "[i6c_rgn] Parameters are different, reattaching "
            "region %d...\n", handle);
        for (char i = 0; i < I6C_VENC_CHN_NUM; i++) {
            if (!i6c_state[i].enable) continue;
            channel.port = i;
            i6c_rgn.fnDetachChannel(0, handle, &channel);
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

    for (char i = 0; i < I6C_VENC_CHN_NUM; i++) {
        if (!i6c_state[i].enable) continue;
        channel.port = i;
        i6c_rgn.fnAttachChannel(0, handle, &channel, &attrib);
    }

    return EXIT_SUCCESS;
}

void i6c_region_deinit(void)
{
    i6c_rgn.fnDeinit(0);
}

void i6c_region_destroy(char handle)
{
    i6c_sys_bind channel = { .module = I6C_SYS_MOD_SCL,
        .device = _i6c_scl_dev, .channel = _i6c_scl_chn };
    
    channel.port = 1;
    i6c_rgn.fnDetachChannel(0, handle, &channel);
    channel.port = 0;
    i6c_rgn.fnDetachChannel(0, handle, &channel);
    i6c_rgn.fnDestroyRegion(0, handle);
}

void i6c_region_init(void)
{
    i6c_rgn_pal palette = {{{0, 0, 0, 0}}};
    i6c_rgn.fnInit(0, &palette);
}

int i6c_region_setbitmap(int handle, hal_bitmap *bitmap)
{
    i6c_rgn_bmp nativeBmp = { .data = bitmap->data, .pixFmt = I6C_RGN_PIXFMT_ARGB1555,
        .size.height = bitmap->dim.height, .size.width = bitmap->dim.width };

    return i6c_rgn.fnSetBitmap(0, handle, &nativeBmp);
}

int i6c_video_create(char index, hal_vidconfig *config)
{
    int ret;
    i6c_venc_chn channel;
    i6c_venc_attr_h26x *attrib;
    
    if (config->codec == HAL_VIDCODEC_JPG || config->codec == HAL_VIDCODEC_MJPG) {
        _i6c_venc_dev[index] = I6C_VENC_DEV_MJPG_0;
        channel.attrib.codec = I6C_VENC_CODEC_MJPG;
        switch (config->mode) {
            case HAL_VIDMODE_CBR:
                channel.rate.mode = I6C_VENC_RATEMODE_MJPGCBR;
                channel.rate.mjpgCbr.bitrate = config->bitrate << 10;
                channel.rate.mjpgCbr.fpsNum = 
                    config->codec == HAL_VIDCODEC_JPG ? 1 : config->framerate;
                channel.rate.mjpgCbr.fpsDen = 1;
                break;
            case HAL_VIDMODE_QP:
                channel.rate.mode = I6C_VENC_RATEMODE_MJPGQP;
                channel.rate.mjpgQp.fpsNum = config->framerate;
                channel.rate.mjpgQp.fpsDen = 
                    config->codec == HAL_VIDCODEC_JPG ? 1 : config->framerate;
                channel.rate.mjpgQp.quality = MAX(config->minQual, config->maxQual);
                break;
            default:
                I6C_ERROR("MJPEG encoder can only support CBR or fixed QP modes!");
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
        channel.attrib.codec = I6C_VENC_CODEC_H265;
        attrib = &channel.attrib.h265;
        switch (config->mode) {
            case HAL_VIDMODE_CBR:
                channel.rate.mode = I6C_VENC_RATEMODE_H265CBR;
                channel.rate.h265Cbr = (i6c_venc_rate_h26xcbr){ .gop = config->gop,
                    .statTime = 1, .fpsNum = config->framerate, .fpsDen = 1, .bitrate = 
                    (unsigned int)(config->bitrate) << 10, .avgLvl = 1 }; break;
            case HAL_VIDMODE_VBR:
                channel.rate.mode = I6C_VENC_RATEMODE_H265VBR;
                channel.rate.h265Vbr = (i6c_venc_rate_h26xvbr){ .gop = config->gop,
                    .statTime = 1, .fpsNum = config->framerate, .fpsDen = 1, .maxBitrate = 
                    (unsigned int)(MAX(config->bitrate, config->maxBitrate)) << 10,
                    .maxQual = config->maxQual, .minQual = config->minQual }; break;
            case HAL_VIDMODE_QP:
                channel.rate.mode = I6C_VENC_RATEMODE_H265QP;
                channel.rate.h265Qp = (i6c_venc_rate_h26xqp){ .gop = config->gop,
                    .fpsNum = config->framerate, .fpsDen = 1, .interQual = config->maxQual,
                    .predQual = config->minQual }; break;
            case HAL_VIDMODE_ABR:
                I6C_ERROR("H.265 encoder does not support ABR mode!");
            case HAL_VIDMODE_AVBR:
                channel.rate.mode = I6C_VENC_RATEMODE_H265AVBR;
                channel.rate.h265Avbr = (i6c_venc_rate_h26xvbr){ .gop = config->gop,
                    .statTime = 1, .fpsNum = config->framerate, .fpsDen = 1, .maxBitrate = 
                    (unsigned int)(MAX(config->bitrate, config->maxBitrate)) << 10,
                    .maxQual = config->maxQual, .minQual = config->minQual }; break;
            default:
                I6C_ERROR("H.265 encoder does not support this mode!");
        }  
    } else if (config->codec == HAL_VIDCODEC_H264) {
        channel.attrib.codec = I6C_VENC_CODEC_H264;
        attrib = &channel.attrib.h264;
        switch (config->mode) {
            case HAL_VIDMODE_CBR:
                channel.rate.mode =  I6C_VENC_RATEMODE_H264CBR;
                channel.rate.h264Cbr = (i6c_venc_rate_h26xcbr){ .gop = config->gop,
                    .statTime = 1, .fpsNum = config->framerate, .fpsDen = 1, .bitrate = 
                    (unsigned int)(config->bitrate) << 10, .avgLvl = 1 }; break;
            case HAL_VIDMODE_VBR:
                channel.rate.mode = I6C_VENC_RATEMODE_H264VBR;
                channel.rate.h264Vbr = (i6c_venc_rate_h26xvbr){ .gop = config->gop,
                    .statTime = 1, .fpsNum = config->framerate, .fpsDen = 1, .maxBitrate = 
                    (unsigned int)(MAX(config->bitrate, config->maxBitrate)) << 10,
                    .maxQual = config->maxQual, .minQual = config->minQual }; break;
            case HAL_VIDMODE_QP:
                channel.rate.mode = I6C_VENC_RATEMODE_H264QP;
                channel.rate.h264Qp = (i6c_venc_rate_h26xqp){ .gop = config->gop,
                    .fpsNum = config->framerate, .fpsDen = 1, .interQual = config->maxQual,
                    .predQual = config->minQual }; break;
            case HAL_VIDMODE_ABR:
                channel.rate.mode = I6C_VENC_RATEMODE_H264ABR;
                channel.rate.h264Abr = (i6c_venc_rate_h26xabr){ .gop = config->gop,
                    .statTime = 1, .fpsNum = config->framerate, .fpsDen = 1,
                    .avgBitrate = (unsigned int)(config->bitrate) << 10,
                    .maxBitrate = (unsigned int)(config->maxBitrate) << 10 }; break;
            case HAL_VIDMODE_AVBR:
                channel.rate.mode = I6C_VENC_RATEMODE_H264AVBR;
                channel.rate.h264Avbr = (i6c_venc_rate_h26xvbr){ .gop = config->gop,
                    .statTime = 1, .fpsNum = config->framerate, .fpsDen = 1, .maxBitrate = 
                    (unsigned int)(MAX(config->bitrate, config->maxBitrate)) << 10,
                    .maxQual = config->maxQual, .minQual = config->minQual }; break;
            default:
                I6C_ERROR("H.264 encoder does not support this mode!");
        }
    } else I6C_ERROR("This codec is not supported by the hardware!");
    _i6c_venc_dev[index] = I6C_VENC_DEV_H26X_0;
    attrib->maxHeight = config->height;
    attrib->maxWidth = config->width;
    attrib->bufSize = config->height * config->width;
    attrib->profile = config->profile;
    attrib->byFrame = 1;
    attrib->height = config->height;
    attrib->width = config->width;
    attrib->bFrameNum = 0;
    attrib->refNum = 1;

    i6c_sys_pool pool;
    memset(&pool, 0, sizeof(pool));
    pool.type = I6C_SYS_POOL_DEVICE_RING;
    pool.create = 1;
    pool.config.ring.module = I6C_SYS_MOD_VENC;
    pool.config.ring.device = _i6c_venc_dev[index];
    pool.config.ring.maxHeight = config->height;
    pool.config.ring.maxWidth = config->width;
    pool.config.ring.ringLine = config->height;
    if (ret = i6c_sys.fnConfigPool(0, &pool))
        return ret;
attach:

    if (ret = i6c_venc.fnCreateChannel(_i6c_venc_dev[index], index, &channel))
        return ret;

    if (_i6c_venc_dev[index] == I6C_VENC_DEV_H26X_0) {
        i6c_venc_src_conf config = I6C_VENC_SRC_CONF_RING_DMA;
        if (ret = i6c_venc.fnSetSourceConfig(_i6c_venc_dev[index], index, &config))
            return ret;
    }

    if (config->codec != HAL_VIDCODEC_JPG && 
        (ret = i6c_venc.fnStartReceiving(_i6c_venc_dev[index], index)))
        return ret;

    i6c_state[index].payload = config->codec;

    return EXIT_SUCCESS;
}

int i6c_video_destroy(char index)
{
    int ret;

    i6c_state[index].enable = 0;
    i6c_state[index].payload = HAL_VIDCODEC_UNSPEC;

    i6c_venc.fnStopReceiving(_i6c_venc_dev[index], index);

    {
        i6c_sys_bind source = { .module = I6C_SYS_MOD_SCL, 
            .device = _i6c_scl_dev, .channel = _i6c_scl_chn, .port = index };
        i6c_sys_bind dest = { .module = I6C_SYS_MOD_VENC,
            .device = _i6c_venc_dev[index], .channel = index, .port = _i6c_venc_port };
        if (ret = i6c_sys.fnUnbind(0, &source, &dest))
            return ret;
    }

    if (ret = i6c_venc.fnDestroyChannel(_i6c_venc_dev[index], index))
        return ret;
    
    if (ret = i6c_scl.fnDisablePort(_i6c_scl_dev, _i6c_scl_chn, index))
        return ret;

    _i6c_venc_dev[index] = 255;
    
    return EXIT_SUCCESS;
}

int i6c_video_destroy_all(void)
{
    int ret;

    for (char i = 0; i < I6C_VENC_CHN_NUM; i++)
        if (i6c_state[i].enable)
            if (ret = i6c_video_destroy(i))
                return ret;

    return EXIT_SUCCESS;
}

void i6c_video_request_idr(char index)
{
    if (i6c_state[index].payload == HAL_VIDCODEC_JPG ||
        i6c_state[index].payload == HAL_VIDCODEC_MJPG) return;

    i6c_venc.fnRequestIdr(I6C_VENC_DEV_H26X_0, index, 1);
}

int i6c_video_snapshot_grab(char index, char quality, hal_jpegdata *jpeg)
{
    int ret;

    if (ret = i6c_channel_bind(index, 1)) {
        fprintf(stderr, "[i6c_venc] Binding the encoder channel "
            "%d failed with %#x!\n", index, ret);
        goto abort;
    }

    i6c_venc_jpg param;
    memset(&param, 0, sizeof(param));
    if (ret = i6c_venc.fnGetJpegParam(_i6c_venc_dev[index], index, &param)) {
        fprintf(stderr, "[i6c_venc] Reading the JPEG settings "
            "%d failed with %#x!\n", index, ret);
        goto abort;
    }

    param.quality = quality;
    if (ret = i6c_venc.fnSetJpegParam(_i6c_venc_dev[index], index, &param)) {
        fprintf(stderr, "[i6c_venc] Writing the JPEG settings "
            "%d failed with %#x!\n", index, ret);
        goto abort;
    }

    unsigned int count = 1;
    if (i6c_venc.fnStartReceivingEx(_i6c_venc_dev[index], index, &count)) {
        fprintf(stderr, "[i6c_venc] Requesting one frame "
            "%d failed with %#x!\n", index, ret);
        goto abort;
    }

    int fd = i6c_venc.fnGetDescriptor(_i6c_venc_dev[index], index);

    struct timeval timeout = { .tv_sec = 2, .tv_usec = 0 };
    fd_set readFds;
    FD_ZERO(&readFds);
    FD_SET(fd, &readFds);
    ret = select(fd + 1, &readFds, NULL, NULL, &timeout);
    if (ret < 0) {
        fprintf(stderr, "[i6c_venc] Select operation failed!\n");
        goto abort;
    } else if (ret == 0) {
        fprintf(stderr, "[i6c_venc] Capture stream timed out!\n");
        goto abort;
    }

    if (FD_ISSET(fd, &readFds)) {
        i6c_venc_stat stat;
        if (i6c_venc.fnQuery(_i6c_venc_dev[index], index, &stat)) {
            fprintf(stderr, "[i6c_venc] Querying the encoder channel "
                "%d failed with %#x!\n", index, ret);
            goto abort;
        }

        if (!stat.curPacks) {
            fprintf(stderr, "[i6c_venc] Current frame is empty, skipping it!\n");
            goto abort;
        }

        i6c_venc_strm strm;
        memset(&strm, 0, sizeof(strm));
        strm.packet = (i6c_venc_pack*)malloc(sizeof(i6c_venc_pack) * stat.curPacks);
        if (!strm.packet) {
            fprintf(stderr, "[i6c_venc] Memory allocation on channel %d failed!\n", index);
            goto abort;
        }
        strm.count = stat.curPacks;

        if (ret = i6c_venc.fnGetStream(_i6c_venc_dev[index], index, &strm, stat.curPacks)) {
            fprintf(stderr, "[i6c_venc] Getting the stream on "
                "channel %d failed with %#x!\n", index, ret);
            free(strm.packet);
            strm.packet = NULL;
            goto abort;
        }

        {
            jpeg->jpegSize = 0;
            for (unsigned int i = 0; i < strm.count; i++) {
                i6c_venc_pack *pack = &strm.packet[i];
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
        i6c_venc.fnFreeStream(_i6c_venc_dev[index], index, &strm);
    }

    i6c_venc.fnFreeDescriptor(_i6c_venc_dev[index], index);

    i6c_venc.fnStopReceiving(_i6c_venc_dev[index], index);

    i6c_channel_unbind(index);

    return ret;
}

void *i6c_video_thread(void)
{
    int ret;
    int maxFd = 0;

    for (int i = 0; i < I6C_VENC_CHN_NUM; i++) {
        if (!i6c_state[i].enable) continue;
        if (!i6c_state[i].mainLoop) continue;

        ret = i6c_venc.fnGetDescriptor(_i6c_venc_dev[i], i);
        if (ret < 0) {
            fprintf(stderr, "[i6c_venc] Getting the encoder descriptor failed with %#x!\n", ret);
            return NULL;
        }
        i6c_state[i].fileDesc = ret;

        if (maxFd <= i6c_state[i].fileDesc)
            maxFd = i6c_state[i].fileDesc;
    }

    i6c_venc_stat stat;
    i6c_venc_strm stream;
    struct timeval timeout;
    fd_set readFds;

    while (keepRunning) {
        FD_ZERO(&readFds);
        for(int i = 0; i < I6C_VENC_CHN_NUM; i++) {
            if (!i6c_state[i].enable) continue;
            if (!i6c_state[i].mainLoop) continue;
            FD_SET(i6c_state[i].fileDesc, &readFds);
        }

        timeout.tv_sec = 2;
        timeout.tv_usec = 0;
        ret = select(maxFd + 1, &readFds, NULL, NULL, &timeout);
        if (ret < 0) {
            fprintf(stderr, "[i6c_venc] Select operation failed!\n");
            break;
        } else if (ret == 0) {
            fprintf(stderr, "[i6c_venc] Main stream loop timed out!\n");
            continue;
        } else {
            for (int i = 0; i < I6C_VENC_CHN_NUM; i++) {
                if (!i6c_state[i].enable) continue;
                if (!i6c_state[i].mainLoop) continue;
                if (FD_ISSET(i6c_state[i].fileDesc, &readFds)) {
                    memset(&stream, 0, sizeof(stream));
                    
                    if (ret = i6c_venc.fnQuery(_i6c_venc_dev[i], i, &stat)) {
                        fprintf(stderr, "[i6c_venc] Querying the encoder channel "
                            "%d failed with %#x!\n", i, ret);
                        break;
                    }

                    if (!stat.curPacks) {
                        fprintf(stderr, "[i6c_venc] Current frame is empty, skipping it!\n");
                        continue;
                    }

                    stream.packet = (i6c_venc_pack*)malloc(
                        sizeof(i6c_venc_pack) * stat.curPacks);
                    if (!stream.packet) {
                        fprintf(stderr, "[i6c_venc] Memory allocation on channel %d failed!\n", i);
                        break;
                    }
                    stream.count = stat.curPacks;

                    if (ret = i6c_venc.fnGetStream(_i6c_venc_dev[i], i, &stream, 40)) {
                        fprintf(stderr, "[i6c_venc] Getting the stream on "
                            "channel %d failed with %#x!\n", i, ret);
                        break;
                    }

                    if (i6c_vid_cb) {
                        hal_vidstream outStrm;
                        hal_vidpack outPack[stream.count];
                        outStrm.count = stream.count;
                        outStrm.seq = stream.sequence;
                        for (int j = 0; j < stream.count; j++) {
                            i6c_venc_pack *pack = &stream.packet[j];
                            outPack[j].data = pack->data;
                            outPack[j].length = pack->length;
                            outPack[j].naluCnt = pack->packNum;
                            switch (i6c_state[i].payload) {
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
                        (*i6c_vid_cb)(i, &outStrm);
                    }

                    if (ret = i6c_venc.fnFreeStream(_i6c_venc_dev[i], i, &stream)) {
                        fprintf(stderr, "[i6c_venc] Releasing the stream on "
                            "channel %d failed with %#x!\n", i, ret);
                    }
                    free(stream.packet);
                    stream.packet = NULL;
                }
            }
        }
    }
    fprintf(stderr, "[i6c_venc] Shutting down encoding thread...\n");
}

void i6c_system_deinit(void)
{
    i6c_sys.fnExit(0);
}

int i6c_system_init(void)
{
    int ret;

    if (ret = i6c_sys.fnInit(0))
        return ret;
    {
        i6c_sys_ver version;
        if (ret = i6c_sys.fnGetVersion(0, &version))
            return ret;
        printf("App built with headers v%s\n", I6C_SYS_API);
        puts(version.version);
    }

    return EXIT_SUCCESS;
}

#endif