#if defined(__ARM_PCS_VFP)

#include "m6_hal.h"

m6_aud_impl  m6_aud;
m6_isp_impl  m6_isp;
m6_rgn_impl  m6_rgn;
m6_scl_impl  m6_scl;
m6_snr_impl  m6_snr;
m6_sys_impl  m6_sys;
m6_venc_impl m6_venc;
m6_vif_impl  m6_vif;

hal_chnstate m6_state[M6_VENC_CHN_NUM] = {0};
int (*m6_aud_cb)(hal_audframe*);
int (*m6_vid_cb)(char, hal_vidstream*);

m6_snr_pad _m6_snr_pad;
m6_snr_plane _m6_snr_plane;
char _m6_snr_framerate, _m6_snr_hdr, _m6_snr_index, _m6_snr_profile;

char _m6_aud_chn = 0;
char _m6_aud_dev = 0;
char _m6_isp_chn = 0;
char _m6_isp_dev = 0;
char _m6_isp_port = 0;
char _m6_scl_chn = 0;
char _m6_scl_dev = 0;
char _m6_venc_dev[M6_VENC_CHN_NUM] = { 255 };
char _m6_venc_port = 0;
char _m6_vif_chn = 0;
char _m6_vif_dev = 0;
char _m6_vif_grp = 0;

void m6_hal_deinit(void)
{
    m6_vif_unload(&m6_vif);
    m6_venc_unload(&m6_venc);
    m6_snr_unload(&m6_snr);
    m6_scl_unload(&m6_scl);
    m6_rgn_unload(&m6_rgn);
    m6_isp_unload(&m6_isp);
    m6_aud_unload(&m6_aud);
    m6_sys_unload(&m6_sys);
}

int m6_hal_init(void)
{
    int ret;

    if (ret = m6_sys_load(&m6_sys))
        return ret;
    if (ret = m6_aud_load(&m6_aud))
        return ret;
    if (ret = m6_isp_load(&m6_isp))
        return ret;
    if (ret = m6_rgn_load(&m6_rgn))
        return ret;
    if (ret = m6_scl_load(&m6_scl))
        return ret;
    if (ret = m6_snr_load(&m6_snr))
        return ret;
    if (ret = m6_venc_load(&m6_venc))
        return ret;
    if (ret = m6_vif_load(&m6_vif))
        return ret;

    return EXIT_SUCCESS;
}

void m6_audio_deinit(void)
{
    m6_aud.fnDisableChannel(_m6_aud_dev, _m6_aud_chn);

    m6_aud.fnDisableDevice(_m6_aud_dev);
}


int m6_audio_init(int samplerate, int gain)
{
    int ret;

    {
        m6_aud_cnf config;
        config.rate = samplerate;
        config.bit = M6_AUD_BIT_16;
        config.intf = M6_AUD_INTF_I2S_SLAVE;
        config.sound = M6_AUD_SND_MONO;
        config.frmNum = 0;
        config.packNumPerFrm = 640;
        config.codecChnNum = 0;
        config.chnNum = 1;
        config.i2s.clock = M6_AUD_CLK_OFF;
        config.i2s.leftJustOn = 0;
        config.i2s.syncRxClkOn = 0;
        config.i2s.tdmSlotNum = 0;
        config.i2s.bit = M6_AUD_BIT_16;
        if (ret = m6_aud.fnSetDeviceConfig(_m6_aud_dev, &config))
            return ret;
    }
    if (ret = m6_aud.fnEnableDevice(_m6_aud_dev))
        return ret;
    
    if (ret = m6_aud.fnEnableChannel(_m6_aud_dev, _m6_aud_chn))
        return ret;
    if (ret = m6_aud.fnSetVolume(_m6_aud_dev, _m6_aud_chn, gain))
        return ret;

    {
        m6_sys_bind bind = { .module = M6_SYS_MOD_AI, 
            .device = _m6_aud_dev, .channel = _m6_aud_chn };
        if (ret = m6_sys.fnSetOutputDepth(0, &bind, 2, 4))
            return ret;
    }

    return EXIT_SUCCESS;
}

void *m6_audio_thread(void)
{
    int ret;

    m6_aud_frm frame;
    memset(&frame, 0, sizeof(frame));

    while (keepRunning && audioOn) {
        if (ret = m6_aud.fnGetFrame(_m6_aud_dev, _m6_aud_chn, 
            &frame, NULL, 128)) {
            HAL_WARNING("m6_aud", "Getting the frame failed "
                "with %#x!\n", ret);
            continue;
        }

        if (m6_aud_cb) {
            hal_audframe outFrame;
            outFrame.channelCnt = 1;
            outFrame.data[0] = frame.addr[0];
            outFrame.length[0] = frame.length[0];
            outFrame.seq = frame.sequence;
            outFrame.timestamp = frame.timestamp;
            (m6_aud_cb)(&outFrame);
        }

        if (ret = m6_aud.fnFreeFrame(_m6_aud_dev, _m6_aud_chn,
            &frame, NULL)) {
            HAL_WARNING("m6_aud", "Releasing the frame failed"
                " with %#x!\n", ret);
        }
    }
    HAL_INFO("m6_aud", "Shutting down capture thread...\n");
}

int m6_channel_bind(char index, char framerate)
{
    int ret;

    if (ret = m6_scl.fnEnablePort(_m6_scl_dev, _m6_scl_chn, index))
        return ret;

    {
        m6_sys_bind source = { .module = M6_SYS_MOD_SCL, 
            .device = _m6_scl_dev, .channel = _m6_scl_chn, .port = index };
        m6_sys_bind dest = { .module = M6_SYS_MOD_VENC,
            .device = _m6_venc_dev[index] ? M6_VENC_DEV_MJPG_0 : M6_VENC_DEV_H26X_0, 
            .channel = index, .port = _m6_venc_port };
        if (ret = m6_sys.fnBindExt(0, &source, &dest, framerate, framerate,
            _m6_venc_dev[index] >= M6_VENC_DEV_MJPG_0 ? 
                M6_SYS_LINK_FRAMEBASE : M6_SYS_LINK_REALTIME, 0))
            return ret;
    }

    return EXIT_SUCCESS;
}

int m6_channel_create(char index, short width, short height, char mirror, char flip, char jpeg)
{
    m6_scl_port port;
    port.crop.x = 0;
    port.crop.y = 0;
    port.crop.width = 0;
    port.crop.height = 0;
    port.output.width = width;
    port.output.height = height;
    port.mirror = mirror;
    port.flip = flip;
    port.compress = M6_COMPR_NONE;
    port.pixFmt = jpeg ? M6_PIXFMT_YUV422_YUYV : M6_PIXFMT_YUV420SP;

    return m6_scl.fnSetPortConfig(_m6_scl_dev, _m6_scl_chn, index, &port);
}

int m6_channel_grayscale(char enable)
{
    return m6_isp.fnSetColorToGray(_m6_isp_dev, 0, &enable);
}

int m6_channel_unbind(char index)
{
    int ret;

    if (ret = m6_scl.fnDisablePort(_m6_scl_dev, _m6_scl_chn, index))
        return ret;

    {
        m6_sys_bind source = { .module = M6_SYS_MOD_SCL, 
            .device = _m6_scl_dev, .channel = _m6_scl_chn, .port = index };
        m6_sys_bind dest = { .module = M6_SYS_MOD_VENC,
            .device = _m6_venc_dev[index], .channel = index, .port = _m6_venc_port };
        if (ret = m6_sys.fnUnbind(0, &source, &dest))
            return ret;
    }

    return EXIT_SUCCESS;    
}

int m6_config_load(char *path)
{
    return m6_isp.fnLoadChannelConfig(_m6_isp_dev, _m6_isp_chn, path, 1234);
}

int m6_pipeline_create(char sensor, short width, short height, char framerate)
{
    int ret;

    _m6_snr_index = sensor;
    _m6_snr_profile = -1;

    {
        unsigned int count;
        m6_snr_res resolution;
        if (ret = m6_snr.fnSetPlaneMode(_m6_snr_index, 0))
            return ret;

        if (ret = m6_snr.fnGetResolutionCount(_m6_snr_index, &count))
            return ret;
        for (char i = 0; i < count; i++) {
            if (ret = m6_snr.fnGetResolution(_m6_snr_index, i, &resolution))
                return ret;

            if (width > resolution.crop.width ||
                height > resolution.crop.height ||
                framerate > resolution.maxFps)
                continue;
        
            _m6_snr_profile = i;
            if (ret = m6_snr.fnSetResolution(_m6_snr_index, _m6_snr_profile))
                return ret;
            _m6_snr_framerate = framerate;
            if (ret = m6_snr.fnSetFramerate(_m6_snr_index, _m6_snr_framerate))
                return ret;
            break;
        }
        if (_m6_snr_profile < 0)
            return EXIT_FAILURE;
    }

    if (ret = m6_snr.fnGetPadInfo(_m6_snr_index, &_m6_snr_pad))
        return ret;
    if (ret = m6_snr.fnGetPlaneInfo(_m6_snr_index, 0, &_m6_snr_plane))
        return ret;
    if (ret = m6_snr.fnEnable(_m6_snr_index))
        return ret;

    {
        m6_vif_grp group;
        group.intf = _m6_snr_pad.intf;
        group.work = M6_VIF_WORK_1MULTIPLEX;
        group.hdr = M6_HDR_OFF;
        group.edge = group.intf == M6_INTF_BT656 ?
            _m6_snr_pad.intfAttr.bt656.edge : M6_EDGE_DOUBLE;
        group.interlaceOn = 0;
        group.grpStitch = (1 << _m6_vif_grp);
        if (ret = m6_vif.fnCreateGroup(_m6_vif_grp, &group))
            return ret;
    }
    
    {
        m6_vif_dev device;
        device.pixFmt = (m6_common_pixfmt)(_m6_snr_plane.bayer > M6_BAYER_END ? 
            _m6_snr_plane.pixFmt : (M6_PIXFMT_RGB_BAYER + _m6_snr_plane.precision * M6_BAYER_END + _m6_snr_plane.bayer));
        device.crop = _m6_snr_plane.capt;
        device.field = 0;
        device.halfHScan = 0;
        if (ret = m6_vif.fnSetDeviceConfig(_m6_vif_dev, &device))
            return ret;
    }
    if (ret = m6_vif.fnEnableDevice(_m6_vif_dev))
        return ret;

    {
        m6_vif_port port;
        port.capt = _m6_snr_plane.capt;
        port.dest.height = _m6_snr_plane.capt.height;
        port.dest.width = _m6_snr_plane.capt.width;
        port.pixFmt = (m6_common_pixfmt)(_m6_snr_plane.bayer > M6_BAYER_END ? 
            _m6_snr_plane.pixFmt : (M6_PIXFMT_RGB_BAYER + _m6_snr_plane.precision * M6_BAYER_END + _m6_snr_plane.bayer));
        port.frate = M6_VIF_FRATE_FULL;
        if (ret = m6_vif.fnSetPortConfig(_m6_vif_dev, _m6_vif_chn, &port))
            return ret;
    }
    if (ret = m6_vif.fnEnablePort(_m6_vif_dev, _m6_vif_chn))
        return ret;

    {
        unsigned int combo = 1;
        if (ret = m6_isp.fnCreateDevice(_m6_isp_dev, &combo))
            return ret;
    }

    {
        m6_isp_chn channel;
        memset(&channel, 0, sizeof(channel));
        channel.sensorId = (1 << _m6_snr_index);
        if (ret = m6_isp.fnCreateChannel(_m6_isp_dev, _m6_isp_chn, &channel))
            return ret;
    }

    {
        m6_isp_para param;
        param.hdr = M6_HDR_OFF;
        param.level3DNR = 1;
        param.mirror = 0;
        param.flip = 0;
        param.rotate = 0;
        if (ret = m6_isp.fnSetChannelParam(_m6_isp_dev, _m6_isp_chn, &param))
            return ret;
    }
    if (ret = m6_isp.fnStartChannel(_m6_isp_dev, _m6_isp_chn))
        return ret;

    {
        m6_isp_port port;
        memset(&port, 0, sizeof(port));
        port.pixFmt = M6_PIXFMT_YUV422_YUYV;
        if (ret = m6_isp.fnSetPortConfig(_m6_isp_dev, _m6_isp_chn, _m6_isp_port, &port))
            return ret;
    }
    if (ret = m6_isp.fnEnablePort(_m6_isp_dev, _m6_isp_chn, _m6_isp_port))
        return ret;

    {
        unsigned int binds = (1 << 0) | (1 << 1) | (1 << 2) | (1 << 3);
        if (ret = m6_scl.fnCreateDevice(_m6_scl_dev, &binds))
            return ret;
    }

    {
        unsigned int reserved = 0;
        if (ret = m6_scl.fnCreateChannel(_m6_scl_dev, _m6_scl_chn, &reserved))
            return ret;
    }
    {
        int rotate = 0;
        if (ret = m6_scl.fnAdjustChannelRotation(_m6_scl_dev, _m6_scl_chn, &rotate))
            return ret;
    }
    if (ret = m6_scl.fnStartChannel(_m6_scl_dev, _m6_scl_chn))
        return ret;

    {
        m6_sys_bind source = { .module = M6_SYS_MOD_VIF, 
            .device = _m6_vif_dev, .channel = _m6_vif_chn, .port = 0 };
        m6_sys_bind dest = { .module = M6_SYS_MOD_ISP,
            .device = _m6_isp_dev, .channel = _m6_isp_chn, .port = _m6_isp_port };
        if (ret = m6_sys.fnBindExt(0, &source, &dest, _m6_snr_framerate, _m6_snr_framerate,
            M6_SYS_LINK_REALTIME, 0))
            return ret;
    }

    {
        m6_sys_bind source = { .module = M6_SYS_MOD_ISP, 
            .device = _m6_isp_dev, .channel = _m6_isp_chn, .port = _m6_isp_port };
        m6_sys_bind dest = { .module = M6_SYS_MOD_SCL,
            .device = _m6_scl_dev, .channel = _m6_scl_chn, .port = 0 };
        return m6_sys.fnBindExt(0, &source, &dest, _m6_snr_framerate, _m6_snr_framerate,
            M6_SYS_LINK_REALTIME, 0);
    }

    return EXIT_SUCCESS;
}

void m6_pipeline_destroy(void)
{
    for (char i = 0; i < 4; i++)
        m6_scl.fnDisablePort(_m6_scl_dev, _m6_scl_chn, i);

    {
        m6_sys_bind source = { .module = M6_SYS_MOD_ISP, 
            .device = _m6_isp_dev, .channel = _m6_isp_chn, .port = _m6_isp_port };
        m6_sys_bind dest = { .module = M6_SYS_MOD_SCL,
            .device = _m6_scl_dev, .channel = _m6_scl_chn, .port = 0 };
        m6_sys.fnUnbind(0, &source, &dest);
    }

    m6_scl.fnStopChannel(_m6_scl_dev, _m6_scl_chn);
    m6_scl.fnDestroyChannel(_m6_scl_dev, _m6_scl_chn);

    m6_scl.fnDestroyDevice(_m6_scl_dev);

    m6_isp.fnStopChannel(_m6_isp_dev, _m6_isp_chn);
    m6_isp.fnDestroyChannel(_m6_isp_dev, _m6_isp_chn);

    m6_isp.fnDestroyDevice(_m6_isp_dev);

    {   
        m6_sys_bind source = { .module = M6_SYS_MOD_VIF, 
            .device = _m6_vif_dev, .channel = _m6_vif_chn, .port = 0 };
        m6_sys_bind dest = { .module = M6_SYS_MOD_ISP,
            .device = _m6_isp_dev, .channel = _m6_isp_chn, .port = _m6_isp_port };
        m6_sys.fnUnbind(0, &source, &dest);
    }

    m6_vif.fnDisablePort(_m6_vif_dev, 0);

    m6_vif.fnDisableDevice(_m6_vif_dev);

    m6_snr.fnDisable(_m6_snr_index);
}

int m6_region_create(char handle, hal_rect rect, short opacity)
{
    int ret;

    m6_sys_bind dest = { .module = M6_SYS_MOD_VENC, .port = _m6_venc_port };
    m6_rgn_cnf region, regionCurr;
    m6_rgn_chn attrib, attribCurr;

    region.type = M6_RGN_TYPE_OSD;
    region.pixFmt = M6_RGN_PIXFMT_ARGB1555;
    region.size.width = rect.width;
    region.size.height = rect.height;

    if (m6_rgn.fnGetRegionConfig(0, handle, &regionCurr)) {
        HAL_INFO("m6_rgn", "Creating region %d...\n", handle);
        if (ret = m6_rgn.fnCreateRegion(0, handle, &region))
            return ret;
    } else if (regionCurr.type != region.type ||
        regionCurr.size.height != region.size.height || 
        regionCurr.size.width != region.size.width) {
        HAL_INFO("m6_rgn", "Parameters are different, recreating "
            "region %d...\n", handle);
        for (char i = 0; i < M6_VENC_CHN_NUM; i++) {
            if (!m6_state[i].enable) continue;
            dest.device = _m6_venc_dev[i];
            dest.channel = i;
            m6_rgn.fnDetachChannel(0, handle, &dest);
        }
        m6_rgn.fnDestroyRegion(0, handle);
        if (ret = m6_rgn.fnCreateRegion(0, handle, &region))
            return ret;
    }

    if (m6_rgn.fnGetChannelConfig(0, handle, &dest, &attribCurr))
        HAL_INFO("m6_rgn", "Attaching region %d...\n", handle);
    else if (attribCurr.point.x != rect.x || attribCurr.point.x != rect.y ||
        attribCurr.osd.bgFgAlpha[1] != opacity) {
        HAL_INFO("m6_rgn", "Parameters are different, reattaching "
            "region %d...\n", handle);
        for (char i = 0; i < M6_VENC_CHN_NUM; i++) {
            if (!m6_state[i].enable) continue;
            dest.device = _m6_venc_dev[i];
            dest.channel = i;
            m6_rgn.fnDetachChannel(0, handle, &dest);
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

    for (char i = 0; i < M6_VENC_CHN_NUM; i++) {
        if (!m6_state[i].enable) continue;
        dest.device = _m6_venc_dev[i];
        dest.channel = i;
        m6_rgn.fnAttachChannel(0, handle, &dest, &attrib);
    }

    return EXIT_SUCCESS;
}

void m6_region_deinit(void)
{
    m6_rgn.fnDeinit(0);
}

void m6_region_destroy(char handle)
{
    m6_sys_bind dest = { .module = M6_SYS_MOD_VENC, .port = _m6_venc_port };
    
    for (char i = 0; i < M6_VENC_CHN_NUM; i++) {
        if (!m6_state[i].enable) continue;
        dest.device = _m6_venc_dev[i];
        dest.channel = i;
        m6_rgn.fnDetachChannel(0, handle, &dest);
    }
    m6_rgn.fnDestroyRegion(0, handle);
}

void m6_region_init(void)
{
    m6_rgn_pal palette = {{{0, 0, 0, 0}}};
    m6_rgn.fnInit(0, &palette);
}

int m6_region_setbitmap(int handle, hal_bitmap *bitmap)
{
    m6_rgn_bmp nativeBmp = { .data = bitmap->data, .pixFmt = M6_RGN_PIXFMT_ARGB1555,
        .size.height = bitmap->dim.height, .size.width = bitmap->dim.width };

    return m6_rgn.fnSetBitmap(0, handle, &nativeBmp);
}

int m6_video_create(char index, hal_vidconfig *config)
{
    int ret;
    m6_venc_chn channel;
    m6_venc_attr_h26x *attrib;
    
    if (config->codec == HAL_VIDCODEC_JPG || config->codec == HAL_VIDCODEC_MJPG) {
        _m6_venc_dev[index] = M6_VENC_DEV_MJPG_0;
        channel.attrib.codec = M6_VENC_CODEC_MJPG;
        switch (config->mode) {
            case HAL_VIDMODE_CBR:
                channel.rate.mode = M6_VENC_RATEMODE_MJPGCBR;
                channel.rate.mjpgCbr.bitrate = config->bitrate << 10;
                channel.rate.mjpgCbr.fpsNum = 
                    config->codec == HAL_VIDCODEC_JPG ? 1 : config->framerate;
                channel.rate.mjpgCbr.fpsDen = 1;
                break;
            case HAL_VIDMODE_QP:
                channel.rate.mode = M6_VENC_RATEMODE_MJPGQP;
                channel.rate.mjpgQp.fpsNum = config->framerate;
                channel.rate.mjpgQp.fpsDen = 
                    config->codec == HAL_VIDCODEC_JPG ? 1 : config->framerate;
                channel.rate.mjpgQp.quality = MAX(config->minQual, config->maxQual);
                break;
            default:
                HAL_ERROR("m6_venc", "MJPEG encoder can only support CBR or fixed QP modes!");
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
        channel.attrib.codec = M6_VENC_CODEC_H265;
        attrib = &channel.attrib.h265;
        switch (config->mode) {
            case HAL_VIDMODE_CBR:
                channel.rate.mode = M6_VENC_RATEMODE_H265CBR;
                channel.rate.h265Cbr = (m6_venc_rate_h26xcbr){ .gop = config->gop,
                    .statTime = 1, .fpsNum = config->framerate, .fpsDen = 1, .bitrate = 
                    (unsigned int)(config->bitrate) << 10, .avgLvl = 1 }; break;
            case HAL_VIDMODE_VBR:
                channel.rate.mode = M6_VENC_RATEMODE_H265VBR;
                channel.rate.h265Vbr = (m6_venc_rate_h26xvbr){ .gop = config->gop,
                    .statTime = 1, .fpsNum = config->framerate, .fpsDen = 1, .maxBitrate = 
                    (unsigned int)(MAX(config->bitrate, config->maxBitrate)) << 10,
                    .maxQual = config->maxQual, .minQual = config->minQual }; break;
            case HAL_VIDMODE_QP:
                channel.rate.mode = M6_VENC_RATEMODE_H265QP;
                channel.rate.h265Qp = (m6_venc_rate_h26xqp){ .gop = config->gop,
                    .fpsNum =  config->framerate, .fpsDen = 1, .interQual = config->maxQual,
                    .predQual = config->minQual }; break;
            case HAL_VIDMODE_ABR:
                HAL_ERROR("m6_venc", "H.265 encoder does not support ABR mode!");
            case HAL_VIDMODE_AVBR:
                channel.rate.mode = M6_VENC_RATEMODE_H265AVBR;
                channel.rate.h265Avbr = (m6_venc_rate_h26xvbr){ .gop = config->gop,
                    .statTime = 1, .fpsNum = config->framerate, .fpsDen = 1, .maxBitrate = 
                    (unsigned int)(MAX(config->bitrate, config->maxBitrate)) << 10,
                    .maxQual = config->maxQual, .minQual = config->minQual }; break;
            default:
                HAL_ERROR("m6_venc", "H.265 encoder does not support this mode!");
        }  
    } else if (config->codec == HAL_VIDCODEC_H264) {
        channel.attrib.codec = M6_VENC_CODEC_H264;
        attrib = &channel.attrib.h264;
        switch (config->mode) {
            case HAL_VIDMODE_CBR:
                channel.rate.mode = M6_VENC_RATEMODE_H264CBR;
                channel.rate.h264Cbr = (m6_venc_rate_h26xcbr){ .gop = config->gop,
                    .statTime = 1, .fpsNum = config->framerate, .fpsDen = 1, .bitrate = 
                    (unsigned int)(config->bitrate) << 10, .avgLvl = 1 }; break;
            case HAL_VIDMODE_VBR:
                channel.rate.mode = M6_VENC_RATEMODE_H264VBR;
                channel.rate.h264Vbr = (m6_venc_rate_h26xvbr){ .gop = config->gop,
                    .statTime = 1, .fpsNum = config->framerate, .fpsDen = 1, .maxBitrate = 
                    (unsigned int)(MAX(config->bitrate, config->maxBitrate)) << 10,
                    .maxQual = config->maxQual, .minQual = config->minQual }; break;
            case HAL_VIDMODE_QP:
                channel.rate.mode = M6_VENC_RATEMODE_H264QP;
                channel.rate.h264Qp = (m6_venc_rate_h26xqp){ .gop = config->gop,
                    .fpsNum = config->framerate, .fpsDen = 1, .interQual = config->maxQual,
                    .predQual = config->minQual }; break;
            case HAL_VIDMODE_ABR:
                channel.rate.mode = M6_VENC_RATEMODE_H264ABR;
                channel.rate.h264Abr = (m6_venc_rate_h26xabr){ .gop = config->gop,
                    .statTime = 1, .fpsNum = config->framerate, .fpsDen = 1,
                    .avgBitrate = (unsigned int)(config->bitrate) << 10,
                    .maxBitrate = (unsigned int)(config->maxBitrate) << 10 }; break;
            case HAL_VIDMODE_AVBR:
                channel.rate.mode = M6_VENC_RATEMODE_H265AVBR;
                channel.rate.h265Avbr = (m6_venc_rate_h26xvbr){ .gop = config->gop,
                    .statTime = 1, .fpsNum = config->framerate, .fpsDen = 1, .maxBitrate = 
                    (unsigned int)(MAX(config->bitrate, config->maxBitrate)) << 10,
                    .maxQual = config->maxQual, .minQual = config->minQual }; break;
            default:
                HAL_ERROR("m6_venc", "H.264 encoder does not support this mode!");
        }
    } else HAL_ERROR("m6_venc", "This codec is not supported by the hardware!");
    _m6_venc_dev[index] = M6_VENC_DEV_H26X_0;
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
    if (ret = m6_venc.fnCreateChannel(_m6_venc_dev[index], index, &channel))
        return ret;

    if (config->codec != HAL_VIDCODEC_JPG && 
        (ret = m6_venc.fnStartReceiving(_m6_venc_dev[index], index)))
        return ret;

    m6_state[index].payload = config->codec;

    return EXIT_SUCCESS;
}

int m6_video_destroy(char index)
{
    int ret;

    m6_state[index].enable = 0;
    m6_state[index].payload = HAL_VIDCODEC_UNSPEC;

    m6_venc.fnStopReceiving(_m6_venc_dev[index], index);

    {
        m6_sys_bind source = { .module = M6_SYS_MOD_SCL, 
            .device = _m6_scl_dev, .channel = _m6_scl_chn, .port = index };
        m6_sys_bind dest = { .module = M6_SYS_MOD_VENC,
            .device = _m6_venc_dev[index], .channel = index, .port = _m6_venc_port };
        if (ret = m6_sys.fnUnbind(0, &source, &dest))
            return ret;
    }

    if (ret = m6_venc.fnDestroyChannel(_m6_venc_dev[index], index))
        return ret;
    
    if (ret = m6_scl.fnDisablePort(_m6_scl_dev, _m6_scl_chn, index))
        return ret;

    _m6_venc_dev[index] = 255;
    
    return EXIT_SUCCESS;
}

int m6_video_destroy_all(void)
{
    int ret;

    for (char i = 0; i < M6_VENC_CHN_NUM; i++)
        if (m6_state[i].enable)
            if (ret = m6_video_destroy(i))
                return ret;
    
    return EXIT_SUCCESS;
}

void m6_video_request_idr(char index)
{
    if (m6_state[index].payload == HAL_VIDCODEC_JPG ||
        m6_state[index].payload == HAL_VIDCODEC_MJPG) return;

    m6_venc.fnRequestIdr(M6_VENC_DEV_H26X_0, index, 1);
}

int m6_video_snapshot_grab(char index, char quality, hal_jpegdata *jpeg)
{
    int ret;

    if (ret = m6_channel_bind(index, 1)) {
        HAL_DANGER("m6_venc", "Binding the encoder channel "
            "%d failed with %#x!\n", index, ret);
        goto abort;
    }
    return ret;

    m6_venc_jpg param;
    memset(&param, 0, sizeof(param));
    if (ret = m6_venc.fnGetJpegParam(_m6_venc_dev[index], index, &param)) {
        HAL_DANGER("m6_venc", "Reading the JPEG settings "
            "%d failed with %#x!\n", index, ret);
        goto abort;
    }
    return ret;
        return ret;
    param.quality = quality;
    if (ret = m6_venc.fnSetJpegParam(_m6_venc_dev[index], index, &param)) {
        HAL_DANGER("m6_venc", "Writing the JPEG settings "
            "%d failed with %#x!\n", index, ret);
        goto abort;
    }

    unsigned int count = 1;
    if (ret = m6_venc.fnStartReceivingEx(_m6_venc_dev[index], index, &count)) {
        HAL_DANGER("m6_venc", "Requesting one frame "
            "%d failed with %#x!\n", index, ret);
        goto abort;
    }

    int fd = m6_venc.fnGetDescriptor(_m6_venc_dev[index], index);

    struct timeval timeout = { .tv_sec = 2, .tv_usec = 0 };
    fd_set readFds;
    FD_ZERO(&readFds);
    FD_SET(fd, &readFds);
    ret = select(fd + 1, &readFds, NULL, NULL, &timeout);
    if (ret < 0) {
        HAL_DANGER("m6_venc", "Select operation failed!\n");
        goto abort;
    } else if (ret == 0) {
        HAL_DANGER("m6_venc", "Capture stream timed out!\n");
        goto abort;
    }

    if (FD_ISSET(fd, &readFds)) {
        m6_venc_stat stat;
        if (ret = m6_venc.fnQuery(_m6_venc_dev[index], index, &stat)) {
            HAL_DANGER("m6_venc", "Querying the encoder channel "
                "%d failed with %#x!\n", index, ret);
            goto abort;
        }

        if (!stat.curPacks) {
            HAL_DANGER("m6_venc", "Current frame is empty, skipping it!\n");
            goto abort;
        }

        m6_venc_strm strm;
        memset(&strm, 0, sizeof(strm));
        strm.packet = (m6_venc_pack*)malloc(sizeof(m6_venc_pack) * stat.curPacks);
        if (!strm.packet) {
            HAL_DANGER("m6_venc", "Memory allocation on channel %d failed!\n", index);
            goto abort;
        }
        strm.count = stat.curPacks;

        if (ret = m6_venc.fnGetStream(_m6_venc_dev[index], index, &strm, stat.curPacks)) {
            HAL_DANGER("m6_venc", "Getting the stream on "
                "channel %d failed with %#x!\n", index, ret);
            free(strm.packet);
            strm.packet = NULL;
            goto abort;
        }

        {
            jpeg->jpegSize = 0;
            for (unsigned int i = 0; i < strm.count; i++) {
                m6_venc_pack *pack = &strm.packet[i];
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
        m6_venc.fnFreeStream(_m6_venc_dev[index], index, &strm);
    }

    m6_venc.fnFreeDescriptor(_m6_venc_dev[index], index);

    m6_venc.fnStopReceiving(_m6_venc_dev[index], index);

    m6_channel_unbind(index);

    return ret;    
}

void *m6_video_thread(void)
{
    int ret, maxFd = 0;

    for (int i = 0; i < M6_VENC_CHN_NUM; i++) {
        if (!m6_state[i].enable) continue;
        if (!m6_state[i].mainLoop) continue;

        ret = m6_venc.fnGetDescriptor(_m6_venc_dev[i], i);
        if (ret < 0) {
            HAL_DANGER("m6_venc", "Getting the encoder descriptor failed with %#x!\n", ret);
            return NULL;
        }
        m6_state[i].fileDesc = ret;

        if (maxFd <= m6_state[i].fileDesc)
            maxFd = m6_state[i].fileDesc;
    }

    m6_venc_stat stat;
    m6_venc_strm stream;
    struct timeval timeout;
    fd_set readFds;

    while (keepRunning) {
        FD_ZERO(&readFds);
        for(int i = 0; i < M6_VENC_CHN_NUM; i++) {
            if (!m6_state[i].enable) continue;
            if (!m6_state[i].mainLoop) continue;
            FD_SET(m6_state[i].fileDesc, &readFds);
        }

        timeout.tv_sec = 2;
        timeout.tv_usec = 0;
        ret = select(maxFd + 1, &readFds, NULL, NULL, &timeout);
        if (ret < 0) {
            HAL_DANGER("m6_venc", "Select operation failed!\n");
            break;
        } else if (ret == 0) {
            HAL_WARNING("m6_venc", "Main stream loop timed out!\n");
            continue;
        } else {
            for (int i = 0; i < M6_VENC_CHN_NUM; i++) {
                if (!m6_state[i].enable) continue;
                if (!m6_state[i].mainLoop) continue;
                if (FD_ISSET(m6_state[i].fileDesc, &readFds)) {
                    memset(&stream, 0, sizeof(stream));
                    
                    if (ret = m6_venc.fnQuery(_m6_venc_dev[i], i, &stat)) {
                        HAL_DANGER("m6_venc", "Querying the encoder channel "
                            "%d failed with %#x!\n", i, ret);
                        break;
                    }

                    if (!stat.curPacks) {
                        HAL_WARNING("m6_venc", "Current frame is empty, skipping it!\n");
                        continue;
                    }

                    stream.packet = (m6_venc_pack*)malloc(
                        sizeof(m6_venc_pack) * stat.curPacks);
                    if (!stream.packet) {
                        HAL_DANGER("m6_venc", "Memory allocation on channel %d failed!\n", i);
                        break;
                    }
                    stream.count = stat.curPacks;

                    if (ret = m6_venc.fnGetStream(_m6_venc_dev[i], i, &stream, 40)) {
                        HAL_DANGER("m6_venc", "Getting the stream on "
                            "channel %d failed with %#x!\n", i, ret);
                        break;
                    }

                    if (m6_vid_cb) {
                        hal_vidstream outStrm;
                        hal_vidpack outPack[stream.count];
                        outStrm.count = stream.count;
                        outStrm.seq = stream.sequence;
                        for (int j = 0; j < stream.count; j++) {
                            m6_venc_pack *pack = &stream.packet[j];
                            outPack[j].data = pack->data;
                            outPack[j].length = pack->length;
                            outPack[j].naluCnt = pack->packNum;
                            switch (m6_state[i].payload) {
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
                        (*m6_vid_cb)(i, &outStrm);
                    }

                    if (ret = m6_venc.fnFreeStream(_m6_venc_dev[i], i, &stream))
                        HAL_WARNING("m6_venc", "Releasing the stream on "
                            "channel %d failed with %#x!\n", i, ret);
                    free(stream.packet);
                    stream.packet = NULL;
                }
            }
        }
    }

    HAL_INFO("m6_venc", "Shutting down encoding thread...\n");
}

void m6_system_deinit(void)
{
    m6_sys.fnExit(0);
}

int m6_system_init(void)
{
    int ret;

    printf("App built with headers v%s\n", M6_SYS_API);

    if (ret = m6_sys.fnInit(0))
        return ret;
    {
        m6_sys_ver version;
        if (ret = m6_sys.fnGetVersion(0, &version))
            return ret;
        puts(version.version);
    }

    return EXIT_SUCCESS;
}

#endif