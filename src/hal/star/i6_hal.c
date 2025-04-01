#ifdef __arm__

#include "i6_hal.h"
#include <time.h>

i6_aud_impl  i6_aud;
i6_isp_impl  i6_isp;
i6_rgn_impl  i6_rgn;
i6_snr_impl  i6_snr;
i6_sys_impl  i6_sys;
i6_venc_impl i6_venc;
i6_vif_impl  i6_vif;
i6_vpe_impl  i6_vpe;

hal_chnstate i6_state[I6_VENC_CHN_NUM] = {0};
int (*i6_aud_cb)(hal_audframe*);
int (*i6_vid_cb)(char, hal_vidstream*);

i6_snr_pad _i6_snr_pad;
i6_snr_plane _i6_snr_plane;
char _i6_snr_framerate, _i6_snr_hdr, _i6_snr_index, _i6_snr_profile;

char _i6_aud_chn = 0;
char _i6_aud_dev = 0;
char _i6_isp_chn = 0;
char _i6_venc_port = 0;
char _i6_vif_chn = 0;
char _i6_vif_dev = 0;
char _i6_vif_port = 0;
char _i6_vpe_chn = 0;
char _i6_vpe_dev = 0;
char _i6_vpe_port = 0;

void i6_hal_deinit(void)
{
    i6_vpe_unload(&i6_vpe);
    i6_vif_unload(&i6_vif);
    i6_venc_unload(&i6_venc);
    i6_snr_unload(&i6_snr);
    i6_rgn_unload(&i6_rgn);
    i6_isp_unload(&i6_isp);
    i6_aud_unload(&i6_aud);
    i6_sys_unload(&i6_sys);
}

int i6_hal_init(void)
{
    int ret;

    if (ret = i6_sys_load(&i6_sys))
        return ret;
    if (ret = i6_aud_load(&i6_aud))
        return ret;
    if (ret = i6_isp_load(&i6_isp))
        return ret;
    if (ret = i6_rgn_load(&i6_rgn))
        return ret;
    if (ret = i6_snr_load(&i6_snr))
        return ret;
    if (ret = i6_venc_load(&i6_venc))
        return ret;
    if (ret = i6_vif_load(&i6_vif))
        return ret;
    if (ret = i6_vpe_load(&i6_vpe))
        return ret;

    return EXIT_SUCCESS;
}

void i6_audio_deinit(void)
{
    i6_aud.fnDisableChannel(_i6_aud_dev, _i6_aud_chn);

    i6_aud.fnDisableDevice(_i6_aud_dev);
}

int i6_audio_init(int samplerate, int gain)
{
    int ret;

    {
        i6_aud_cnf config;
        config.rate = samplerate;
        config.bit24On = 0;
        config.intf = I6_AUD_INTF_I2S_SLAVE;
        config.sound = I6_AUD_SND_MONO;
        config.frmNum = 0;
        config.packNumPerFrm = 640;
        config.codecChnNum = 0;
        config.chnNum = 1;
        config.i2s.leftJustOn = 0;
        config.i2s.clock = I6_AUD_CLK_OFF;
        config.i2s.syncRxClkOn = 0;
        config.i2s.tdmSlotNum = 0;
        config.i2s.bit24On = 0;
        if (ret = i6_aud.fnSetDeviceConfig(_i6_aud_dev, &config))
            return ret;
    }
    if (ret = i6_aud.fnEnableDevice(_i6_aud_dev))
        return ret;
    
    if (ret = i6_aud.fnEnableChannel(_i6_aud_dev, _i6_aud_chn))
        return ret;
    if (ret = i6_aud.fnSetVolume(_i6_aud_dev, _i6_aud_chn, gain))
        return ret;

    {
        i6_sys_bind bind = { .module = I6_SYS_MOD_AI, 
            .device = _i6_aud_dev, .channel = _i6_aud_chn };
        if (ret = i6_sys.fnSetOutputDepth(&bind, 2, 4))
            return ret;
    }

    return EXIT_SUCCESS;
}

void *i6_audio_thread(void)
{
    int ret;

    i6_aud_frm frame;
    memset(&frame, 0, sizeof(frame));

    while (keepRunning) {
        if (ret = i6_aud.fnGetFrame(_i6_aud_dev, _i6_aud_chn, 
            &frame, NULL, 128)) {
            HAL_WARNING("i6_aud", "Getting the frame failed "
                "with %#x!\n", ret);
            continue;
        }

        if (i6_aud_cb) {
            hal_audframe outFrame;
            outFrame.channelCnt = 1;
            outFrame.data[0] = frame.addr[0];
            outFrame.length[0] = frame.length;
            outFrame.seq = frame.sequence;
            outFrame.timestamp = frame.timestamp;
            (i6_aud_cb)(&outFrame);
        }

        if (ret = i6_aud.fnFreeFrame(_i6_aud_dev, _i6_aud_chn,
            &frame, NULL)) {
            HAL_WARNING("i6_aud", "Releasing the frame failed"
                " with %#x!\n", ret);
        }
    }
    HAL_INFO("i6_aud", "Shutting down capture thread...\n");
}

int i6_channel_bind(char index, char framerate)
{
    int ret;

    HAL_DEBUG("HAL",  "fnEnablePort %d, %d\n", _i6_vpe_chn, index);
    if (ret = i6_vpe.fnEnablePort(_i6_vpe_chn, index))
        return ret;

    {
        unsigned int device;
        if (ret = i6_venc.fnGetChannelDeviceId(index, &device))
            return ret;
        i6_sys_bind source = { .module = I6_SYS_MOD_VPE, 
            .device = _i6_vpe_dev, .channel = _i6_vpe_chn, .port = index };
        i6_sys_bind dest = { .module = I6_SYS_MOD_VENC,
            .device = device, .channel = index, .port = _i6_venc_port };


        HAL_DEBUG("HAL",  "MI_SYS_BindChnPort2 interceptée avec les arguments:\n");
        HAL_DEBUG("HAL",  "Source Channel Port: Module=%d, DevId=%d, ChnId=%d, PortId=%d, fps=%d\n", source.module, source.device, source.channel, source.port, framerate);
        HAL_DEBUG("HAL",  "Dest   Channel Port: Module=%d, DevId=%d, ChnId=%d, PortId=%d, fps=%d\n", dest.module, dest.device, dest.channel, dest.port, framerate);
        HAL_DEBUG("HAL",  "Link: %d, LinkParam: %d\n", I6_SYS_LINK_FRAMEBASE, 1080);
        if (ret = i6_sys.fnBindExt(&source, &dest, framerate, framerate,
            I6_SYS_LINK_FRAMEBASE, 1080))
            return ret;
    }

    return EXIT_SUCCESS;
}


int i6_channel_create(char index, short width, short height, char mirror, char flip, char jpeg)
{
    i6_vpe_port port;
    port.output.width = width;
    port.output.height = height;
    /* use sensor mirror/flip instead of VPE one */
    port.mirror = 0;
    port.flip = 0;
    port.compress = I6_COMPR_NONE;
    port.pixFmt = jpeg ? I6_PIXFMT_YUV422_YUYV : I6_PIXFMT_YUV420SP;

    HAL_DEBUG("HAL",  "output.width: %u\n", port.output.width);
    HAL_DEBUG("HAL",  "output.height: %u\n", port.output.height);
    HAL_DEBUG("HAL",  "mirror: %d\n", port.mirror);
    HAL_DEBUG("HAL",  "flip: %d\n", port.flip);
    HAL_DEBUG("HAL",  "pixFmt: %d\n", port.pixFmt);
    HAL_DEBUG("HAL",  "compress: %d\n", port.compress);

    return i6_vpe.fnSetPortConfig(_i6_vpe_chn, index, &port);
}

int i6_channel_grayscale(char enable)
{
    return i6_isp.fnSetColorToGray(0, &enable);
}

int i6_channel_sensorexposure(unsigned int timeus)
{
    int ret;
    MI_ISP_AE_EXPO_LIMIT_TYPE_t data;
    if (ret = i6_isp.getExposureLimit(0, &data))
        return ret;
    HAL_DEBUG("HAL",  "Set Exposure limit to: %u us\n", timeus);
    data.u32MaxShutterUS = timeus;
    ret = i6_isp.setExposureLimit(0, &data);
    return ret;
}

int i6_channel_unbind(char index)
{
    int ret;

    if (ret = i6_vpe.fnDisablePort(_i6_vpe_chn, index))
        return ret;

    {
        unsigned int device;
        if (ret = i6_venc.fnGetChannelDeviceId(index, &device))
            return ret;
        i6_sys_bind source = { .module = I6_SYS_MOD_VPE, 
            .device = _i6_vpe_dev, .channel = _i6_vpe_chn, .port = index };
        i6_sys_bind dest = { .module = I6_SYS_MOD_VENC,
            .device = device, .channel = index, .port = _i6_venc_port };
        if (ret = i6_sys.fnUnbind(&source, &dest))
            return ret;
    }

    return EXIT_SUCCESS; 
}

int i6_config_load(char *path)
{
    return i6_isp.fnLoadChannelConfig(_i6_isp_chn, path, 1234);
}

void i6_sensor_config(char framerate)
{
    i6_channel_grayscale(0);
    unsigned int timeus = 1000000 / framerate;
    HAL_DEBUG("HAL",  "Set sensor exposure to: %u us, framerate %i\n", timeus, framerate); 
    i6_channel_sensorexposure(timeus);
}

int i6_pipeline_create(char sensor, short width, short height, char framerate, char mirror, char flip, unsigned int noiselevel)
{
    int ret;

    _i6_snr_index = sensor;
    _i6_snr_profile = -1;

    {
        unsigned int count;
        i6_snr_res resolution;
        if (ret = i6_snr.fnSetPlaneMode(_i6_snr_index, 0))
            return ret;

        if (ret = i6_snr.fnGetResolutionCount(_i6_snr_index, &count))
            return ret;

        for (char i = 0; i < count; i++) {
            if (ret = i6_snr.fnGetResolution(_i6_snr_index, i, &resolution))
                return ret;
                HAL_DEBUG("HAL",  "Profile %d: %dx%d @ %d fps\n", i, resolution.crop.width,
                resolution.crop.height, resolution.maxFps);
        }

        for (char i = 0; i < count; i++) {
            if (ret = i6_snr.fnGetResolution(_i6_snr_index, i, &resolution))
                return ret;

            if (width > resolution.crop.width ||
                height > resolution.crop.height ||
                framerate > resolution.maxFps)
                continue;
            
            HAL_DEBUG("HAL",  "Set profile %i, fps %i\n", i, framerate);

            _i6_snr_profile = i;
            if (ret = i6_snr.fnSetResolution(_i6_snr_index, _i6_snr_profile))
                return ret;
            _i6_snr_framerate = framerate;
            if (ret = i6_snr.fnSetFramerate(_i6_snr_index, _i6_snr_framerate))
                return ret;
            break;
        }
        if (_i6_snr_profile < 0)
            return EXIT_FAILURE;
    }

    if (ret = i6_snr.fnSetOrien(_i6_snr_index, mirror, flip))
        return ret;

    if (ret = i6_snr.fnGetPadInfo(_i6_snr_index, &_i6_snr_pad))
        return ret;
    if (ret = i6_snr.fnGetPlaneInfo(_i6_snr_index, 0, &_i6_snr_plane))
        return ret;
    if (ret = i6_snr.fnEnable(_i6_snr_index))
        return ret;

    {
        i6_vif_dev device;
        memset(&device, 0, sizeof(device));
        device.intf = _i6_snr_pad.intf;
        device.work = device.intf == I6_INTF_BT656 ? 
            I6_VIF_WORK_1MULTIPLEX : I6_VIF_WORK_RGB_REALTIME;
        device.hdr = I6_HDR_OFF;
        if (device.intf == I6_INTF_MIPI) {
            device.edge = I6_EDGE_DOUBLE;
            device.input = _i6_snr_pad.intfAttr.mipi.input;
        } else if (device.intf == I6_INTF_BT656) {
            device.edge = _i6_snr_pad.intfAttr.bt656.edge;
            device.sync = _i6_snr_pad.intfAttr.bt656.sync;
        }


        HAL_DEBUG("HAL",  "intf: %u\n", device.intf);
        HAL_DEBUG("HAL",  "work: %u\n", device.work);
        HAL_DEBUG("HAL",  "hdr: %u\n", device.hdr);
        HAL_DEBUG("HAL",  "edge: %u\n", device.edge);
        HAL_DEBUG("HAL",  "input: %u\n", device.input);
        HAL_DEBUG("HAL",  "bitswap: %d\n", device.bitswap);
        HAL_DEBUG("HAL",  "vsyncInv: %d\n", device.sync.vsyncInv);
        HAL_DEBUG("HAL",  "hsyncInv: %d\n", device.sync.hsyncInv);
        HAL_DEBUG("HAL",  "pixclkInv: %d\n", device.sync.pixclkInv);
        HAL_DEBUG("HAL",  "vsyncDelay: %u\n", device.sync.vsyncDelay);
        HAL_DEBUG("HAL",  "hsyncDelay: %u\n", device.sync.hsyncDelay);
        HAL_DEBUG("HAL",  "pixclkDelay: %u\n", device.sync.pixclkDelay);

        if (ret = i6_vif.fnSetDeviceConfig(_i6_vif_dev, &device))
            return ret;
    }
    if (ret = i6_vif.fnEnableDevice(_i6_vif_dev))
        return ret;

    {
        i6_vif_port port;
        port.capt = _i6_snr_plane.capt;
        port.dest.height = _i6_snr_plane.capt.height;
        port.dest.width = _i6_snr_plane.capt.width;
        port.field = 0;
        port.interlaceOn = 0;
        port.pixFmt = (i6_common_pixfmt)(_i6_snr_plane.bayer > I6_BAYER_END ? 
            _i6_snr_plane.pixFmt : (I6_PIXFMT_RGB_BAYER + _i6_snr_plane.precision * I6_BAYER_END + _i6_snr_plane.bayer));
        port.frate = I6_VIF_FRATE_FULL;
        port.frameLineCnt = 0;

        HAL_DEBUG("HAL",  "capt.x: %u\n", port.capt.x);
        HAL_DEBUG("HAL",  "capt.y: %u\n", port.capt.y);
        HAL_DEBUG("HAL",  "capt.width: %u\n", port.capt.width);
        HAL_DEBUG("HAL",  "capt.height: %u\n", port.capt.height);
        HAL_DEBUG("HAL",  "dest.width: %u\n", port.dest.width);
        HAL_DEBUG("HAL",  "dest.height: %u\n", port.dest.height);
        HAL_DEBUG("HAL",  "field: %d\n", port.field);
        HAL_DEBUG("HAL",  "interlaceOn: %d\n", port.interlaceOn);
        HAL_DEBUG("HAL",  "pixFmt: %d\n", port.pixFmt);
        HAL_DEBUG("HAL",  "frate: %u\n", port.frate);
        HAL_DEBUG("HAL",  "frameLineCnt: %u\n", port.frameLineCnt);

        if (ret = i6_vif.fnSetPortConfig(_i6_vif_chn, _i6_vif_port, &port))
            return ret;
    }
    if (ret = i6_vif.fnEnablePort(_i6_vif_chn, _i6_vif_port))
        return ret;

    if (series == 0xF1) {
        i6e_vpe_chn channel;
        memset(&channel, 0, sizeof(channel));
        channel.capt.height = _i6_snr_plane.capt.height;
        channel.capt.width = _i6_snr_plane.capt.width;
        channel.pixFmt = (i6_common_pixfmt)(_i6_snr_plane.bayer > I6_BAYER_END ? 
            _i6_snr_plane.pixFmt : (I6_PIXFMT_RGB_BAYER + _i6_snr_plane.precision * I6_BAYER_END + _i6_snr_plane.bayer));
        channel.hdr = I6_HDR_OFF;
        channel.sensor = (i6_vpe_sens)(_i6_snr_index + 1);
        channel.mode = I6_VPE_MODE_REALTIME;
        if (ret = i6_vpe.fnCreateChannel(_i6_vpe_chn, (i6_vpe_chn*)&channel))
            return ret;

        i6e_vpe_para param;
        memset(&param, 0, sizeof(param));
        param.hdr = I6_HDR_OFF;
        param.level3DNR = noiselevel;
        param.mirror = 0;
        param.flip = 0;
        param.lensAdjOn = 0;

        HAL_DEBUG("HAL",  "hdr: %d\n", param.hdr);
        HAL_DEBUG("HAL",  "level3DNR: %d\n", param.level3DNR);
        HAL_DEBUG("HAL",  "mirror: %d\n", param.mirror);
        HAL_DEBUG("HAL",  "flip: %d\n", param.flip);
        HAL_DEBUG("HAL",  "lensAdjOn: %d\n", param.lensAdjOn);

        if (ret = i6_vpe.fnSetChannelParam(_i6_vpe_chn, (i6_vpe_para*)&param))
            return ret;
    } else {
        i6_vpe_chn channel;
        memset(&channel, 0, sizeof(channel));
        channel.capt.height = _i6_snr_plane.capt.height;
        channel.capt.width = _i6_snr_plane.capt.width;
        channel.pixFmt = (i6_common_pixfmt)(_i6_snr_plane.bayer > I6_BAYER_END ? 
            _i6_snr_plane.pixFmt : (I6_PIXFMT_RGB_BAYER + _i6_snr_plane.precision * I6_BAYER_END + _i6_snr_plane.bayer));
        channel.hdr = I6_HDR_OFF;
        channel.sensor = (i6_vpe_sens)(_i6_snr_index + 1);
        channel.mode = I6_VPE_MODE_REALTIME;
        if (ret = i6_vpe.fnCreateChannel(_i6_vpe_chn, &channel))
            return ret;

        i6_vpe_para param;
        memset(&param, 0, sizeof(param));
        param.hdr = I6_HDR_OFF;
        param.level3DNR = noiselevel;
        param.mirror = 0;
        param.flip = 0;
        param.lensAdjOn = 0;
        if (ret = i6_vpe.fnSetChannelParam(_i6_vpe_chn, &param))
            return ret;
    }
    if (ret = i6_vpe.fnStartChannel(_i6_vpe_chn))
        return ret;

    {
        i6_sys_bind source = { .module = I6_SYS_MOD_VIF, 
            .device = _i6_vif_dev, .channel = _i6_vif_chn, .port = _i6_vif_port };
        i6_sys_bind dest = { .module = I6_SYS_MOD_VPE,
            .device = _i6_vpe_dev, .channel = _i6_vpe_chn, .port = _i6_vpe_port };

        HAL_DEBUG("HAL",  "MI_SYS_BindChnPort2 interceptée avec les arguments:\n");
        HAL_DEBUG("HAL",  "Source Channel Port: Module=%d, DevId=%d, ChnId=%d, PortId=%d, fps=%d\n", source.module, source.device, source.channel, source.port, framerate);
        HAL_DEBUG("HAL",  "Dest   Channel Port: Module=%d, DevId=%d, ChnId=%d, PortId=%d, fps=%d\n", dest.module, dest.device, dest.channel, dest.port, framerate);
        HAL_DEBUG("HAL",  "Link: %d, LinkParam: %d\n", I6_SYS_LINK_REALTIME, 0);
        return i6_sys.fnBindExt(&source, &dest, _i6_snr_framerate, _i6_snr_framerate,
            I6_SYS_LINK_REALTIME, 0);
    }

    return EXIT_SUCCESS;
}

void i6_pipeline_destroy(void)
{
    for (char i = 0; i < 4; i++)
        i6_vpe.fnDisablePort(_i6_vpe_chn, i);

    {
        i6_sys_bind source = { .module = I6_SYS_MOD_VIF, 
            .device = _i6_vif_dev, .channel = _i6_vif_chn, .port = _i6_vif_port };
        i6_sys_bind dest = { .module = I6_SYS_MOD_VPE,
            .device = _i6_vif_dev, .channel = _i6_vpe_chn, .port = _i6_vpe_port };
        i6_sys.fnUnbind(&source, &dest);
    }

    i6_vpe.fnStopChannel(_i6_vpe_chn);
    i6_vpe.fnDestroyChannel(_i6_vpe_chn);

    i6_vif.fnDisablePort(_i6_vif_chn, 0);
    i6_vif.fnDisableDevice(_i6_vif_dev);

    i6_snr.fnDisable(_i6_snr_index);
}

int i6_region_create(char handle, hal_rect rect, short opacity)
{
    int ret;

    i6_sys_bind dest = { .module = 0,
        .device = _i6_vpe_dev, .channel = _i6_vpe_chn };
    i6_rgn_cnf region, regionCurr;
    i6_rgn_chn attrib, attribCurr;

    region.type = I6_RGN_TYPE_OSD;
    region.pixFmt = I6_RGN_PIXFMT_ARGB1555;
    region.size.width = rect.width;
    region.size.height = rect.height;

    if (i6_rgn.fnGetRegionConfig(handle, &regionCurr)) {
        HAL_INFO("i6_rgn", "Creating region %d...\n", handle);
        if (ret = i6_rgn.fnCreateRegion(handle, &region))
            return ret;
    } else if (regionCurr.type != region.type ||
        regionCurr.size.height != region.size.height || 
        regionCurr.size.width != region.size.width) {
        HAL_INFO("i6_rgn", "Parameters are different, recreating "
            "region %d...\n", handle);
        for (char i = 0; i < I6_VENC_CHN_NUM; i++) {
            if (!i6_state[i].enable) continue;
            dest.port = i;
            i6_rgn.fnDetachChannel(handle, &dest);
        }
        i6_rgn.fnDestroyRegion(handle);
        if (ret = i6_rgn.fnCreateRegion(handle, &region))
            return ret;
    }

    if (i6_rgn.fnGetChannelConfig(handle, &dest, &attribCurr))
        HAL_INFO("i6_rgn", "Attaching region %d...\n", handle);
    else if (attribCurr.point.x != rect.x || attribCurr.point.x != rect.y ||
        attribCurr.osd.bgFgAlpha[1] != opacity) {
        HAL_INFO("i6_rgn", "Parameters are different, reattaching "
            "region %d...\n", handle);
        for (char i = 0; i < I6_VENC_CHN_NUM; i++) {
            if (!i6_state[i].enable) continue;
            dest.port = i;
            i6_rgn.fnDetachChannel(handle, &dest);
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

    for (char i = 0; i < I6_VENC_CHN_NUM; i++) {
        if (!i6_state[i].enable) continue;
        dest.port = i;
        i6_rgn.fnAttachChannel(handle, &dest, &attrib);
    }

    return EXIT_SUCCESS;
}

void i6_region_deinit(void)
{
    i6_rgn.fnDeinit();
}

void i6_region_destroy(char handle)
{
    i6_sys_bind dest = { .module = 0,
        .device = _i6_vpe_dev, .channel = _i6_vpe_chn };
    
    for (char i = 0; i < I6_VENC_CHN_NUM; i++) {
        if (!i6_state[i].enable) continue;
        dest.port = i;
        i6_rgn.fnDetachChannel(handle, &dest);
    }
    i6_rgn.fnDestroyRegion(handle);
}

void i6_region_init(void)
{
    i6_rgn_pal palette = {{{0, 0, 0, 0}}};
    i6_rgn.fnInit(&palette);
}

int i6_region_setbitmap(int handle, hal_bitmap *bitmap)
{
    i6_rgn_bmp nativeBmp = { .data = bitmap->data, .pixFmt = I6_RGN_PIXFMT_ARGB1555,
        .size.height = bitmap->dim.height, .size.width = bitmap->dim.width };

    return i6_rgn.fnSetBitmap(handle, &nativeBmp);
}

int i6_video_create(char index, hal_vidconfig *config)
{
    int ret;
    i6_venc_chn channel;
    i6_venc_attr_h26x *attrib;
    
    if (config->codec == HAL_VIDCODEC_JPG || config->codec == HAL_VIDCODEC_MJPG) {
        channel.attrib.codec = I6_VENC_CODEC_MJPG;
        switch (config->mode) {
            case HAL_VIDMODE_CBR:
                channel.rate.mode = series == 0xEF ? I6OG_VENC_RATEMODE_MJPGCBR : I6_VENC_RATEMODE_MJPGCBR;
                channel.rate.mjpgCbr.bitrate = config->bitrate << 10;
                channel.rate.mjpgCbr.fpsNum = 
                    config->codec == HAL_VIDCODEC_JPG ? 1 : config->framerate;
                channel.rate.mjpgCbr.fpsDen = 1;
                break;
            case HAL_VIDMODE_QP:
                channel.rate.mode = series == 0xEF ? I6OG_VENC_RATEMODE_MJPGQP : I6_VENC_RATEMODE_MJPGQP;
                channel.rate.mjpgQp.fpsNum = 
                    config->codec == HAL_VIDCODEC_JPG ? 1 : config->framerate;
                channel.rate.mjpgCbr.fpsDen = 1;
                channel.rate.mjpgQp.quality = MAX(config->minQual, config->maxQual);
                break;
            default:
                HAL_ERROR("i6_venc", "MJPEG encoder can only support CBR or fixed QP modes!");
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
        channel.attrib.codec = I6_VENC_CODEC_H265;
        attrib = &channel.attrib.h265;
        switch (config->mode) {
            case HAL_VIDMODE_CBR:
                HAL_DEBUG("HAL",  "H265: Mode CBR\n");
                channel.rate.mode = series == 0xEF ? I6OG_VENC_RATEMODE_H265CBR :
                    I6_VENC_RATEMODE_H265CBR;
                channel.rate.h265Cbr = (i6_venc_rate_h26xcbr){ .gop = config->gop,
                    .statTime = 1, .fpsNum = config->framerate, .fpsDen = 1, .bitrate = 
                    (unsigned int)(config->bitrate) << 10, .avgLvl = 1 }; break;
            case HAL_VIDMODE_VBR:
                HAL_DEBUG("HAL",  "H265: Mode VBR\n");
                channel.rate.mode = series == 0xEF ? I6OG_VENC_RATEMODE_H265VBR :
                    I6_VENC_RATEMODE_H265VBR;
                channel.rate.h265Vbr = (i6_venc_rate_h26xvbr){ .gop = config->gop,
                    .statTime = 1, .fpsNum = config->framerate, .fpsDen = 1, .maxBitrate = 
                    (unsigned int)(MAX(config->bitrate, config->maxBitrate)) << 10,
                    .maxQual = config->maxQual, .minQual = config->minQual }; break;
            case HAL_VIDMODE_QP:
                HAL_DEBUG("HAL",  "H265: Mode QP\n");
                channel.rate.mode = series == 0xEF ? I6OG_VENC_RATEMODE_H265QP :
                    I6_VENC_RATEMODE_H265QP;
                channel.rate.h265Qp = (i6_venc_rate_h26xqp){ .gop = config->gop,
                    .fpsNum = config->framerate, .fpsDen = 1, .interQual = config->maxQual,
                    .predQual = config->minQual }; break;
            case HAL_VIDMODE_ABR:
                HAL_ERROR("i6_venc", "H.265 encoder does not support ABR mode!");
            case HAL_VIDMODE_AVBR:
                HAL_DEBUG("HAL",  "H265: Mode AVBR\n");
                channel.rate.mode = series == 0xEF ? I6OG_VENC_RATEMODE_H265AVBR :
                    I6_VENC_RATEMODE_H265AVBR;
                channel.rate.h265Avbr = (i6_venc_rate_h26xvbr){ .gop = config->gop,
                    .statTime = 1, .fpsNum = config->framerate, .fpsDen = 1, .maxBitrate = 
                    (unsigned int)(MAX(config->bitrate, config->maxBitrate)) << 10,
                    .maxQual = config->maxQual, .minQual = config->minQual }; break;
            default:
                HAL_ERROR("i6_venc", "H.265 encoder does not support this mode!");
        }  
    } else if (config->codec == HAL_VIDCODEC_H264) {
        channel.attrib.codec = I6_VENC_CODEC_H264;
        attrib = &channel.attrib.h264;
        if (series == 0xEF && config->mode == HAL_VIDMODE_ABR)
            config->mode = -1;
        switch (config->mode) {
            case HAL_VIDMODE_CBR:
                HAL_DEBUG("HAL",  "H264: Mode CBR\n");
                channel.rate.mode = series == 0xEF ? I6OG_VENC_RATEMODE_H264CBR :
                    I6_VENC_RATEMODE_H264CBR;
                channel.rate.h264Cbr = (i6_venc_rate_h26xcbr){ .gop = config->gop,
                    .statTime = 1, .fpsNum = config->framerate, .fpsDen = 1, .bitrate = 
                    (unsigned int)(config->bitrate) << 10, .avgLvl = 1 }; break;
            case HAL_VIDMODE_VBR:
                HAL_DEBUG("HAL",  "H264: Mode VBR\n");
                channel.rate.mode = series == 0xEF ? I6OG_VENC_RATEMODE_H264VBR :
                    I6_VENC_RATEMODE_H264VBR;
                channel.rate.h264Vbr = (i6_venc_rate_h26xvbr){ .gop = config->gop,
                    .statTime = 1, .fpsNum = config->framerate, .fpsDen = 1, .maxBitrate = 
                    (unsigned int)(MAX(config->bitrate, config->maxBitrate)) << 10,
                    .maxQual = config->maxQual, .minQual = config->minQual }; break;
            case HAL_VIDMODE_QP:
                HAL_DEBUG("HAL",  "H264: Mode QP\n");
                channel.rate.mode = series == 0xEF ? I6OG_VENC_RATEMODE_H264QP :
                    I6_VENC_RATEMODE_H264QP;
                channel.rate.h264Qp = (i6_venc_rate_h26xqp){ .gop = config->gop,
                    .fpsNum = config->framerate, .fpsDen = 1, .interQual = config->maxQual,
                    .predQual = config->minQual }; break;
            case HAL_VIDMODE_ABR:
                HAL_DEBUG("HAL",  "H264: Mode ABR\n");
                channel.rate.mode = I6_VENC_RATEMODE_H264ABR;
                channel.rate.h264Abr = (i6_venc_rate_h26xabr){ .gop = config->gop,
                    .statTime = 1, .fpsNum = config->framerate, .fpsDen = 1,
                    .avgBitrate = (unsigned int)(config->bitrate) << 10,
                    .maxBitrate = (unsigned int)(config->maxBitrate) << 10 }; break;
            case HAL_VIDMODE_AVBR:
                HAL_DEBUG("HAL",  "H264: Mode AVBR\n");
                channel.rate.mode = series == 0xEF ? I6OG_VENC_RATEMODE_H264AVBR :
                    I6_VENC_RATEMODE_H264AVBR;
                channel.rate.h264Avbr = (i6_venc_rate_h26xvbr){ .gop = config->gop, .statTime = 1,
                    .fpsNum = config->framerate, .fpsDen = 1, .maxBitrate = 
                    (unsigned int)(MAX(config->bitrate, config->maxBitrate)) << 10,
                    .maxQual = config->maxQual, .minQual = config->minQual }; break;
            default:
                HAL_ERROR("i6_venc", "H.264 encoder does not support this mode!");
        }
    } else HAL_ERROR("i6_venc", "This codec is not supported by the hardware!");
    attrib->maxHeight = config->height;
    attrib->maxWidth = config->width;
    //attrib->bufSize = config->height * config->width;
    attrib->bufSize = 0; // Set as majestic
    attrib->profile = MIN((series == 0xEF || config->codec == HAL_VIDCODEC_H265) ? 1 : 2,
        config->profile);
    attrib->byFrame = 1;
    attrib->height = config->height;
    attrib->width = config->width;
    attrib->bFrameNum = 0;
    //attrib->refNum = 1;
    attrib->refNum = 0;
    // Reference frame does not seems to be supported in h264/h265, set to 0 (as majestic) 

    channel.rate.h265Cbr.statTime = 0;
    channel.rate.h265Cbr.avgLvl = 0;
attach:

    HAL_DEBUG("HAL",  "codec: %u\n", channel.attrib.codec);
    HAL_DEBUG("HAL",  "maxWidth: %u\n", channel.attrib.h265.maxWidth);
    HAL_DEBUG("HAL",  "maxHeight: %u\n", channel.attrib.h265.maxHeight);
    HAL_DEBUG("HAL",  "bufSize: %u\n", channel.attrib.h265.bufSize);
    HAL_DEBUG("HAL",  "profile: %u\n", channel.attrib.h265.profile);
    HAL_DEBUG("HAL",  "byFrame: 0x%02x\n", channel.attrib.h265.byFrame);
    HAL_DEBUG("HAL",  "width: %u\n", channel.attrib.h265.width);
    HAL_DEBUG("HAL",  "height: %u\n", channel.attrib.h265.height);
    HAL_DEBUG("HAL",  "bFrameNum: %u\n", channel.attrib.h265.bFrameNum);
    HAL_DEBUG("HAL",  "refNum: %u\n", channel.attrib.h265.refNum);
    HAL_DEBUG("HAL",  "mode: %u\n", channel.rate.mode);
    HAL_DEBUG("HAL",  "gop: %u\n", channel.rate.h265Cbr.gop);
    HAL_DEBUG("HAL",  "statTime: %u\n", channel.rate.h265Cbr.statTime);
    HAL_DEBUG("HAL",  "fpsNum: %u\n", channel.rate.h265Cbr.fpsNum);
    HAL_DEBUG("HAL",  "fpsDen: %u\n", channel.rate.h265Cbr.fpsDen);
    HAL_DEBUG("HAL",  "bitrate: %u\n", channel.rate.h265Cbr.bitrate);
    HAL_DEBUG("HAL",  "avgLvl: %u\n", channel.rate.h265Cbr.avgLvl);


    if (ret = i6_venc.fnCreateChannel(index, &channel))
        return ret;

    MI_VENC_RcParam_t pstRcParam;
    //if (ret = i6_venc.fnGetRCParam(index, &pstRcParam))
    //    return ret;
    
    pstRcParam.u32ThrdI[0] = 0;
    pstRcParam.u32ThrdP[0] = 0;
    pstRcParam.u32RowQpDelta = 0;
    pstRcParam.stParamH265Cbr.u32MaxQp = config->maxQp;
    pstRcParam.stParamH265Cbr.u32MinQp = config->minQp;
    pstRcParam.stParamH265Cbr.s32IPQPDelta = config->IPQPDelta;
    pstRcParam.stParamH265Cbr.u32MaxIQp = config->maxIQp;
    pstRcParam.stParamH265Cbr.u32MinIQp = config->minIQp;
    pstRcParam.stParamH265Cbr.u32MaxIPProp = config->maxIPProp;
    pstRcParam.stParamH265Cbr.u32MaxISize = config->maxISize;
    pstRcParam.stParamH265Cbr.u32MaxPSize = config->maxPSize;

    HAL_DEBUG("HAL",  "u32ThrdI[0]: %u\n", pstRcParam.u32ThrdI[0]);
    HAL_DEBUG("HAL",  "u32ThrdP[0]: %u\n", pstRcParam.u32ThrdP[0]);
    HAL_DEBUG("HAL",  "u32RowQpDelta: %u\n", pstRcParam.u32RowQpDelta);
    HAL_DEBUG("HAL",  "u32MaxQp: %u\n", pstRcParam.stParamH265Cbr.u32MaxQp);
    HAL_DEBUG("HAL",  "u32MinQp: %u\n", pstRcParam.stParamH265Cbr.u32MinQp);
    HAL_DEBUG("HAL",  "s32IPQPDelta: %d\n", pstRcParam.stParamH265Cbr.s32IPQPDelta);
    HAL_DEBUG("HAL",  "u32MaxIQp: %u\n", pstRcParam.stParamH265Cbr.u32MaxIQp);
    HAL_DEBUG("HAL",  "u32MinIQp: %u\n", pstRcParam.stParamH265Cbr.u32MinIQp);
    HAL_DEBUG("HAL",  "u32MaxIPProp: %u\n", pstRcParam.stParamH265Cbr.u32MaxIPProp);
    HAL_DEBUG("HAL",  "u32MaxISize: %u\n", pstRcParam.stParamH265Cbr.u32MaxISize);
    HAL_DEBUG("HAL",  "u32MaxPSize: %u\n", pstRcParam.stParamH265Cbr.u32MaxPSize);

    if (ret = i6_venc.fnSetRCParam(index, &pstRcParam))
       return ret;

    HAL_DEBUG("HAL",  "RC Set\n");

    if (config->cus3A)
    {
        // some reverse engineering with ltrace and LD_PRELOAD to see what's majestic is doing
        HAL_DEBUG("HAL",  "disable 3A and reactivate it\n");
        if (ret = i6_isp.fnDisableUserspace3A(0))
            return ret;
        HAL_DEBUG("HAL",  "after 3A deactivation\n");
        cus3AEnable_t data;
        data.args[0] = 0x4000000;
        data.args[1] = 0;
        data.args[2] = 0xb;
        data.args[3] = 0;
        data.args[4] = 0;
        data.args[5] = 0;
        data.args[6] = 0;
        data.args[7] = 0;
        data.args[8] = 0;
        data.args[9] = 0;
        data.args[10] = 0;
        data.args[11] = 0;
        data.args[12] = 0;

        if (ret = i6_isp.fnCUS3AEnable(0, &data))
            return ret;
        HAL_DEBUG("HAL",  "after 3A reactivation\n");
    }
    else
    {
        HAL_DEBUG("HAL",  "Keep default 3A\n");
    }


    if (config->intraQp)
    {
        MI_VENC_IntraRefresh_t stIntraRefresh;
        stIntraRefresh.bEnable = 1;
        stIntraRefresh.u32RefreshLineNum = config->intraLine;
        stIntraRefresh.u32ReqIQp = 1;
        HAL_DEBUG("HAL",  "call setIntraRefresh, enable %i, RefreshLine : %i, u32ReqIQp: %i\n",
            stIntraRefresh.bEnable, stIntraRefresh.u32RefreshLineNum, stIntraRefresh.u32ReqIQp);
        i6_venc.fnSetIntraRefresh(0, &stIntraRefresh);
        HAL_DEBUG("HAL",  "after call setIntraRefresh\n");
    }
    else
    {
        HAL_DEBUG("HAL",  "IntraRefresh deactivated\n");
    }

    if (config->codec != HAL_VIDCODEC_JPG && 
        (ret = i6_venc.fnStartReceiving(index)))
        return ret;

    i6_state[index].payload = config->codec;

    HAL_DEBUG("HAL",  "VENC channel started\n");

    return EXIT_SUCCESS;
}

int i6_video_destroy(char index)
{
    int ret;

    i6_state[index].enable = 0;
    i6_state[index].payload = HAL_VIDCODEC_UNSPEC;

    i6_venc.fnStopReceiving(index);

    {
        unsigned int device;
        if (ret = i6_venc.fnGetChannelDeviceId(index, &device))
            return ret;
        i6_sys_bind source = { .module = I6_SYS_MOD_VPE, 
            .device = _i6_vpe_dev, .channel = _i6_vpe_chn, .port = index };
        i6_sys_bind dest = { .module = I6_SYS_MOD_VENC,
            .device = device, .channel = index, .port = _i6_venc_port };
        if (ret = i6_sys.fnUnbind(&source, &dest))
            return ret;
    }

    if (ret = i6_venc.fnDestroyChannel(index))
        return ret;
    
    if (ret = i6_vpe.fnDisablePort(_i6_vpe_chn, index))
        return ret;

    return EXIT_SUCCESS;
}

int i6_video_destroy_all(void)
{
    int ret;

    for (char i = 0; i < I6_VENC_CHN_NUM; i++)
        if (i6_state[i].enable)
            if (ret = i6_video_destroy(i))
                return ret;

    return EXIT_SUCCESS;
}

void i6_video_request_idr(char index)
{
    i6_venc.fnRequestIdr(index, 1);
}

int i6_video_snapshot_grab(char index, char quality, hal_jpegdata *jpeg)
{
    int ret;

    if (ret = i6_channel_bind(index, 1)) {
        HAL_DANGER("i6_venc", "Binding the encoder channel "
            "%d failed with %#x!\n", index, ret);
        goto abort;
    }

    i6_venc_jpg param;
    memset(&param, 0, sizeof(param));
    if (ret = i6_venc.fnGetJpegParam(index, &param)) {
        HAL_DANGER("i6_venc", "Reading the JPEG settings "
            "%d failed with %#x!\n", index, ret);
        goto abort;
    }

    param.quality = quality;
    if (ret = i6_venc.fnSetJpegParam(index, &param)) {
        HAL_DANGER("i6_venc", "Writing the JPEG settings "
            "%d failed with %#x!\n", index, ret);
        goto abort;
    }

    unsigned int count = 1;
    if (ret = i6_venc.fnStartReceivingEx(index, &count)) {
        HAL_DANGER("i6_venc", "Requesting one frame "
            "%d failed with %#x!\n", index, ret);
        goto abort;
    }

    int fd = i6_venc.fnGetDescriptor(index);

    struct timeval timeout = { .tv_sec = 2, .tv_usec = 0 };
    fd_set readFds;
    FD_ZERO(&readFds);
    FD_SET(fd, &readFds);
    ret = select(fd + 1, &readFds, NULL, NULL, &timeout);
    if (ret < 0) {
        HAL_DANGER("i6_venc", "Select operation failed!\n");
        goto abort;
    } else if (ret == 0) {
        HAL_DANGER("i6_venc", "Capture stream timed out!\n");
        goto abort;
    }

    if (FD_ISSET(fd, &readFds)) {
        i6_venc_stat stat;
        if (ret = i6_venc.fnQuery(index, &stat)) {
            HAL_DANGER("i6_venc", "Querying the encoder channel "
                "%d failed with %#x!\n", index, ret);
            goto abort;
        }

        if (!stat.curPacks) {
            HAL_DANGER("i6_venc", "Current frame is empty, skipping it!\n");
            goto abort;
        }

        i6_venc_strm strm;
        memset(&strm, 0, sizeof(strm));
        strm.packet = (i6_venc_pack*)malloc(sizeof(i6_venc_pack) * stat.curPacks);
        if (!strm.packet) {
            HAL_DANGER("i6_venc", "Memory allocation on channel %d failed!\n", index);
            goto abort;
        }
        strm.count = stat.curPacks;

        if (ret = i6_venc.fnGetStream(index, &strm, stat.curPacks)) {
            HAL_DANGER("i6_venc", "Getting the stream on "
                "channel %d failed with %#x!\n", index, ret);
            free(strm.packet);
            strm.packet = NULL;
            goto abort;
        }

        {
            jpeg->jpegSize = 0;
            for (unsigned int i = 0; i < strm.count; i++) {
                i6_venc_pack *pack = &strm.packet[i];
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
        i6_venc.fnFreeStream(index, &strm);
    }

    i6_venc.fnFreeDescriptor(index);

    i6_venc.fnStopReceiving(index);

    i6_channel_unbind(index);

    return ret;
}

#define DEBUG

extern unsigned long long current_time_microseconds(void);


#define PROC_FILENAME "/proc/sstarts"

typedef struct {
    unsigned long      frameNb;
    unsigned long long vsync_timestamp;
    unsigned long long framestart_timestamp;
    unsigned long long frameend_timestamp;
    unsigned long long ispframedone_timestamp;
    unsigned long long vencdone_timestamp;
    unsigned long long one_way_delay_ns;
} air_timestamp_buffer_t;
static air_timestamp_buffer_t timestamps;



static int is_module_loaded(const char *module_name) {
    FILE *fp = fopen("/proc/modules", "r");
    if (!fp) {
        perror("Failed to open /proc/modules");
        return -1;
    }

    char line[256];
    while (fgets(line, sizeof(line), fp)) {
        if (strncmp(line, module_name, strlen(module_name)) == 0) {
            fclose(fp);
            return 1; // Module is loaded
        }
    }

    fclose(fp);
    return 0; // Module is not loaded
}

int load_sstarts(void) {
    const char *module_name = "sstarts";
    const char *path1 = "/root/sstarts.ko";
    const char *path2 = "/lib/modules/4.9.84/extra/sstarts.ko";
    const char *module_path = NULL;

    // Check if the module is already loaded
    int loaded = is_module_loaded(module_name);
    if (loaded < 0) {
        return 1; // Error occurred while checking
    } else if (loaded == 1) {
        printf("Module '%s' is already loaded. Attempting to remove it...\n", module_name);
        if (system("rmmod sstarts") != 0) {
            perror("Failed to remove kernel module");
            return 1;
        }
        printf("Module '%s' removed successfully.\n", module_name);
    }

    // Check if the module exists at the first path
    if (access(path1, F_OK) == 0) {
        module_path = path1;
    }
    // Check if the module exists at the second path
    else if (access(path2, F_OK) == 0) {
        module_path = path2;
    } else {
        HAL_INFO("HAL", "%s kernel module not found at either path %s or %s\n", module_name, path1, path2);
        return 1;
    }

    // Construct the insmod command
    char command[256];
    snprintf(command, sizeof(command), "insmod %s", module_path);

    // Execute the insmod command
    int ret = system(command);

    // Check the result
    if (ret == 0) {
        printf("Kernel module inserted successfully from %s.\n", module_path);
    } else {
        perror("Failed to insert kernel module sstarts");
    }

    return ret;
}








static int proc_fd = -1;



void venc_finished() {
    char buffer[256];
    ssize_t bytes_read;
    unsigned long long current_time_ns;
    unsigned long long acquisition_time_ns;
    unsigned long long isp_processing_time_ns;
    unsigned long long vpe_venc_time_ns;
    unsigned long ispframedone_nb;

    struct timespec ts;
    loff_t pos = 0;

    // Obtenir le temps courant
    clock_gettime(CLOCK_MONOTONIC, &ts);
    current_time_ns = ts.tv_sec * 1000000000ULL + ts.tv_nsec;
    timestamps.vencdone_timestamp = current_time_ns;

    // Ouvrir le fichier /proc/sstarts si nécessaire
    if (proc_fd < 0) {
        proc_fd = open(PROC_FILENAME, O_RDONLY);
        if (proc_fd < 0) {
            perror("Failed to open /proc/sstarts");
            return;
        }
    }

    lseek(proc_fd, 0, SEEK_SET);
    bytes_read = read(proc_fd, buffer, sizeof(buffer) - 1);
    if (bytes_read < 0) {
        perror("Failed to read /proc/sstarts");
        close(proc_fd);
        proc_fd = -1;
        return;
    }
    buffer[bytes_read] = '\0';

    // Parse the timestamps
    sscanf(buffer, "Frame: %u, VSync: %llu ns, FrameStart: %llu ns, FrameEnd: %llu ns, ISPFrameDone: %llu ns\n",
           &ispframedone_nb,
           &timestamps.vsync_timestamp,
           &timestamps.framestart_timestamp,
           &timestamps.frameend_timestamp,
           &timestamps.ispframedone_timestamp);


    // Calculer les temps
    acquisition_time_ns = timestamps.frameend_timestamp - timestamps.vsync_timestamp;
    isp_processing_time_ns = timestamps.ispframedone_timestamp - timestamps.frameend_timestamp;
    vpe_venc_time_ns = timestamps.vencdone_timestamp - timestamps.ispframedone_timestamp;

    // Publier les résultats
    //printf("Acquisition Time: %llu ns\n", acquisition_time_ns);
    //printf("ISP Processing Time: %llu ns\n", isp_processing_time_ns);
    //printf("VPE + VENC Time: %llu ns\n", vpe_venc_time_ns);
}


#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <time.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <pthread.h>


#define GROUND_IP "192.168.1.14" // Adresse IP du système "ground"
#define GROUND_PORT 12345       // Port du système "ground"
#define AIR_PORT 12346          // Port du système "air"
#define TIMEOUT_US 500          // Timeout en microsecondes

static int sockfd = -1;
static struct sockaddr_in ground_addr;

int timestamp_init(void)
{
    // Créer un socket UDP
    if ((sockfd = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
        perror("Failed to create socket");
        return 1;
    }

    struct sockaddr_in air_addr;
    memset(&air_addr, 0, sizeof(air_addr));
    air_addr.sin_family = AF_INET;
    air_addr.sin_addr.s_addr = INADDR_ANY;
    air_addr.sin_port = htons(AIR_PORT);

    if (bind(sockfd, (struct sockaddr *)&air_addr, sizeof(air_addr)) < 0) {
        perror("Failed to bind socket");
        close(sockfd);
        return 1;
    }

    memset(&ground_addr, 0, sizeof(ground_addr));
    ground_addr.sin_family = AF_INET;
    ground_addr.sin_port = htons(GROUND_PORT);
    if (inet_pton(AF_INET, GROUND_IP, &ground_addr.sin_addr) <= 0) {
        perror("Invalid address/Address not supported");
        close(sockfd);
        return 1;
    }
}

// Define htonll and ntohll functions
uint64_t htonll(uint64_t value) {
    if (htonl(1) != 1) {
        return ((uint64_t)htonl(value & 0xFFFFFFFF) << 32) | htonl(value >> 32);
    } else {
        return value;
    }
}

uint64_t ntohll(uint64_t value) {
    if (ntohl(1) != 1) {
        return ((uint64_t)ntohl(value & 0xFFFFFFFF) << 32) | ntohl(value >> 32);
    } else {
        return value;
    }
}

void timestamp_send_finished(unsigned long frameNb)
{
    struct timespec ts;
    unsigned long long air_time_ns, air_time_ns_network, rtt_ns, ground_time_ns;
    socklen_t addr_len = sizeof(ground_addr);
    char buffer[256];

    // Capturer le temps "air"
    clock_gettime(CLOCK_MONOTONIC, &ts);
    air_time_ns = ts.tv_sec * 1000000000ULL + ts.tv_nsec;

    // Envoyer le temps "air" au système "ground"
    air_time_ns_network = htonll(air_time_ns);
    if (sendto(sockfd, &air_time_ns_network, sizeof(air_time_ns_network), 0, (struct sockaddr *)&ground_addr, sizeof(ground_addr)) < 0) {
        perror("Failed to send data");
        return;
    }

    // Recevoir la réponse du système "ground" avec timeout
    struct timeval timeout;
    timeout.tv_sec = 0;
    timeout.tv_usec = TIMEOUT_US;
    setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));

    if (recvfrom(sockfd, &ground_time_ns, sizeof(ground_time_ns), 0, (struct sockaddr *)&ground_addr, &addr_len) < 0) {
        if (errno == EWOULDBLOCK || errno == EAGAIN) {
            printf("Receive timeout\n");
        } else {
            perror("Failed to receive data");
        }
        return;
    }

    // Capturer le temps de réception de la réponse
    clock_gettime(CLOCK_MONOTONIC, &ts);
    rtt_ns = ts.tv_sec * 1000000000ULL + ts.tv_nsec - air_time_ns;

    // convertir le temps de réception du système "ground" en réseau en local
    ground_time_ns = ntohll(ground_time_ns);

    // Calculer le temps de traversée unidirectionnel
    timestamps.one_way_delay_ns = rtt_ns / 2;

#ifdef DEBUG
    //printf("Packet sent %i =>\n", frameNb);
    //printf("Sent air time:        %llu ns\n", air_time_ns);
    //printf("Received ground time: %llu ns\n", ground_time_ns);
    //printf("RTT:                  %llu ns\n", rtt_ns);
    //printf("One-way delay:        %llu ns\n", timestamps.one_way_delay_ns);

      
    //HAL_DEBUG("HAL",  "Packet sent %lu =>\n", frameNb);
	//HAL_DEBUG("HAL",  "AirTime:                %llu us\n", air_time_ns / 1000);
	//HAL_DEBUG("HAL",  "GroundTime:             %llu us\n", ground_time_ns / 1000);
    //HAL_DEBUG("HAL",  "Vsync:                  %llu us\n", timestamps.vsync_timestamp / 1000);
    //HAL_DEBUG("HAL",  "ISP Done:               %llu us\n", timestamps.ispframedone_timestamp / 1000);
	//HAL_DEBUG("HAL",  "Venc Done:              %llu us\n", timestamps.vencdone_timestamp / 1000);
	//HAL_DEBUG("HAL",  "Air One Way Delay:      %llu us\n", timestamps.one_way_delay_ns / 1000);

#endif
    timestamps.frameNb = htonl(frameNb);
    timestamps.vsync_timestamp = htonll(timestamps.vsync_timestamp);
    timestamps.framestart_timestamp = htonll(timestamps.framestart_timestamp);
    timestamps.frameend_timestamp = htonll(timestamps.frameend_timestamp);
    timestamps.ispframedone_timestamp = htonll(timestamps.ispframedone_timestamp);
    timestamps.vencdone_timestamp = htonll(timestamps.vencdone_timestamp);
    timestamps.one_way_delay_ns = htonll(timestamps.one_way_delay_ns);

    if (sendto(sockfd, &timestamps, sizeof(timestamps), 0, (struct sockaddr *)&ground_addr, sizeof(ground_addr)) < 0) {
        perror("Failed to send data");
        return;
    }
}

void *i6_video_thread(void)
{
    int ret, maxFd = 0;
    #if 0
    unsigned long long lastms = 0;
    unsigned long long curms = 0;
    unsigned long long timediff;
    #endif

    for (int i = 0; i < I6_VENC_CHN_NUM; i++) {
        if (!i6_state[i].enable) continue;
        if (!i6_state[i].mainLoop) continue;

        ret = i6_venc.fnGetDescriptor(i);
        if (ret < 0) {
            HAL_DANGER("i6_venc", "Getting the encoder descriptor failed with %#x!\n", ret);
            return (void*)0;
        }
        i6_state[i].fileDesc = ret;

        if (maxFd <= i6_state[i].fileDesc)
            maxFd = i6_state[i].fileDesc;
    }

    i6_venc_stat stat;
    i6_venc_strm stream;
    struct timeval timeout;
    fd_set readFds;


    unsigned long long lastTs = 0;
    timestamp_init();

    load_sstarts();
    
    while (keepRunning) {
        FD_ZERO(&readFds);
        for(int i = 0; i < I6_VENC_CHN_NUM; i++) {
            if (!i6_state[i].enable) continue;
            if (!i6_state[i].mainLoop) continue;
            FD_SET(i6_state[i].fileDesc, &readFds);
        }

        timeout.tv_sec = 2;
        timeout.tv_usec = 0;
        ret = select(maxFd + 1, &readFds, NULL, NULL, &timeout);
        if (ret < 0) {
            HAL_DANGER("i6_venc", "Select operation failed!\n");
            break;
        } else if (ret == 0) {
            HAL_WARNING("i6_venc", "Main stream loop timed out!\n");
            continue;
        } else {
            #if 0
            curms = current_time_microseconds() / 1000;
            if (lastms)
            {
                timediff = curms - lastms;
                //if (timediff < 10 || timediff > 20)
                //    HAL_DEBUG("HAL",  "Time diff %llu ms\n", curms - lastms);
                //HAL_DEBUG("HAL",  "Time diff %llu ms\n", curms - lastms);
            }
            lastms = curms;
            #endif

            for (int i = 0; i < I6_VENC_CHN_NUM; i++) {
                if (!i6_state[i].enable) continue;
                if (!i6_state[i].mainLoop) continue;
                if (FD_ISSET(i6_state[i].fileDesc, &readFds)) {
                    memset(&stream, 0, sizeof(stream));
                    
                    if (ret = i6_venc.fnQuery(i, &stat)) {
                        HAL_DANGER("i6_venc", "Querying the encoder channel "
                            "%d failed with %#x!\n", i, ret);
                        break;
                    }

                    if (!stat.curPacks) {
                        HAL_WARNING("i6_venc", "Current frame is empty, skipping it!\n");
                        continue;
                    }

                    stream.packet = (i6_venc_pack*)malloc(
                        sizeof(i6_venc_pack) * stat.curPacks);
                    if (!stream.packet) {
                        HAL_DANGER("i6_venc", "Memory allocation on channel %d failed!\n", i);
                        break;
                    }
                    stream.count = stat.curPacks;

                    if (ret = i6_venc.fnGetStream(i, &stream, 40)) {
                        HAL_DANGER("i6_venc", "Getting the stream on "
                            "channel %d failed with %#x!\n", i, ret);
                        break;
                    }

                    venc_finished();

                    if (i6_vid_cb) {
                        hal_vidstream outStrm;
                        hal_vidpack outPack[stream.count];
                        outStrm.count = stream.count;
                        outStrm.seq = stream.sequence;
                        for (int j = 0; j < stream.count; j++) {
                            
                            i6_venc_pack *pack = &stream.packet[j];
                            //HAL_DEBUG("HAL",  "Stream %d, packet %d, diff %llu\n", j, pack->packNum, pack->timestamp - lastTs);
                            lastTs = pack->timestamp;
                            outPack[j].data = pack->data;
                            outPack[j].length = pack->length;
                            outPack[j].naluCnt = pack->packNum;
                            if (series == 0xEF) {
                                signed char n = 0;
                                switch (i6_state[i].payload) {
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
                            } else switch (i6_state[i].payload) {
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
                        (*i6_vid_cb)(i, &outStrm);
                    }

                    if (ret = i6_venc.fnFreeStream(i, &stream)) {
                        HAL_DANGER("i6_venc", "Releasing the stream on "
                            "channel %d failed with %#x!\n", i, ret);
                    }
                    free(stream.packet);
                    stream.packet = NULL;

                    //timestamp_send_finished(0);
                }
            }
        }
    }

    // Fermer le fichier à la fin du programme
    if (proc_fd >= 0) {
        close(proc_fd);
    }

    HAL_INFO("i6_venc", "Shutting down encoding thread...\n");
}

void i6_system_deinit(void)
{
    i6_sys.fnExit();
}

int i6_system_init(void)
{
    int ret;

    printf("App built with headers v%s\n", I6_SYS_API);

    {
        i6_sys_ver version;
        if (ret = i6_sys.fnGetVersion(&version))
            return ret;
        puts(version.version);
    }

    if (ret = i6_sys.fnInit())
        return ret;

    return EXIT_SUCCESS;
}

#endif