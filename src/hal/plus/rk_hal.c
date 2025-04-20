#if defined(__ARM_PCS_VFP)

#include "rk_hal.h"

rk_aiq_impl     rk_aiq;
rk_aud_impl     rk_aud;
rk_mb_impl      rk_mb;
rk_rgn_impl     rk_rgn;
rk_sys_impl     rk_sys;
rk_venc_impl    rk_venc;
rk_vi_impl      rk_vi;
rk_vpss_impl    rk_vpss;

hal_chnstate rk_state[RK_VENC_CHN_NUM] = {0};
int (*rk_aud_cb)(hal_audframe*);
int (*rk_vid_cb)(char, hal_vidstream*);

void *_rk_aiq_ctx = NULL;
char _rk_aud_chn = 0;
char _rk_aud_dev = 0;
char _rk_isp_chn = 0;
char _rk_isp_dev = 0;
char _rk_venc_dev = 0;
char _rk_vi_chn = 0;
char _rk_vi_dev = 0;
char _rk_vi_pipe = 0;
char _rk_vpss_chn = 0;
char _rk_vpss_grp = 0;

void rk_hal_deinit(void)
{
    rk_vpss_unload(&rk_vpss);
    rk_vi_unload(&rk_vi);
    rk_venc_unload(&rk_venc);
    rk_rgn_unload(&rk_rgn);
    rk_mb_unload(&rk_mb);
    rk_aud_unload(&rk_aud);
    rk_aiq_unload(&rk_aiq);
    rk_sys_unload(&rk_sys);
}

int rk_hal_init(void)
{
    int ret;

    if (ret = rk_sys_load(&rk_sys))
        return ret;
    if (ret = rk_aiq_load(&rk_aiq))
        return ret;
    if (ret = rk_aud_load(&rk_aud))
        return ret;
    if (ret = rk_mb_load(&rk_mb))
        return ret;
    if (ret = rk_rgn_load(&rk_rgn))
        return ret;
    if (ret = rk_venc_load(&rk_venc))
        return ret;
    if (ret = rk_vi_load(&rk_vi))
        return ret;
    if (ret = rk_vpss_load(&rk_vpss))
        return ret;

    return EXIT_SUCCESS;
}

void rk_audio_deinit(void)
{
    rk_aud.fnDisableChannel(_rk_aud_dev, _rk_aud_chn);

    rk_aud.fnDisableDevice(_rk_aud_dev);
}

int rk_audio_init(int samplerate)
{
    int ret;

    {
        rk_aud_cnf config;
        config.cardChnNum = 1;
        config.cardRate = samplerate;
        config.cardBit = RK_AUD_BIT_16;
        config.rate = samplerate;
        config.bit = RK_AUD_BIT_16;
        config.mode = RK_AUD_MODE_MONO;
        config.expandOn = 0;
        config.frmNum = 30;
        config.packNumPerFrm = 320;
        config.chnNum = 1;
        config.cardName[0] = '\0';
        if (ret = rk_aud.fnSetDeviceConfig(_rk_aud_dev, &config))
            return ret;
    }
    if (ret = rk_aud.fnEnableDevice(_rk_aud_dev))
        return ret;
    
    if (ret = rk_aud.fnEnableChannel(_rk_aud_dev, _rk_aud_chn))
        return ret;

    return EXIT_SUCCESS;
}

void *rk_audio_thread(void)
{
    int ret;

    rk_aud_frm frame;
    rk_aud_efrm echoFrame;
    memset(&frame, 0, sizeof(frame));
    memset(&echoFrame, 0, sizeof(echoFrame));

    while (keepRunning) {
        if (ret = rk_aud.fnGetFrame(_rk_aud_dev, _rk_aud_chn, 
            &frame, &echoFrame, 128)) {
            HAL_WARNING("rk_aud", "Getting the frame failed "
                "with %#x!\n", ret);
            continue;
        }

        if (rk_aud_cb) {
            hal_audframe outFrame;
            outFrame.channelCnt = 1;
            outFrame.data[0] = rk_mb.fnGetData(frame.mbBlk);
            outFrame.length[0] = frame.length;
            outFrame.seq = frame.sequence;
            outFrame.timestamp = frame.timestamp;
            (rk_aud_cb)(&outFrame);
        }

        if (ret = rk_aud.fnFreeFrame(_rk_aud_dev, _rk_aud_chn,
            &frame, &echoFrame)) {
            HAL_DANGER("rk_aud", "Releasing the frame failed"
                " with %#x!\n", ret);
        }
    }
    HAL_INFO("rk_aud", "Shutting down encoding thread...\n");
}

int rk_channel_bind(char index)
{
    int ret;

    if (ret = rk_vpss.fnEnableChannel(_rk_vpss_grp, index))
        return ret;

    {
        rk_sys_bind source = { .module = RK_SYS_MOD_VPSS, 
            .device = _rk_vpss_grp, .channel = index };
        rk_sys_bind dest = { .module = RK_SYS_MOD_VENC,
            .device = _rk_venc_dev, .channel = index };
        if (ret = rk_sys.fnBind(&source, &dest))
            return ret;
    }

    return EXIT_SUCCESS;
}

int rk_channel_create(char index, short width, short height, char mirror, char flip)
{
    int ret;

    {
        rk_vpss_chn channel;
        memset(&channel, 0, sizeof(channel));
        channel.chnMode = RK_VPSS_CMODE_AUTO;
        channel.dest.width = width;
        channel.dest.height = height;
        channel.pixFmt = RK_PIXFMT_YUV420SP;
        channel.hdr = RK_HDR_SDR8;
        channel.srcFps = -1;
        channel.dstFps = -1;
        channel.mirror = mirror;
        channel.flip = flip;
        channel.depth = 0;
        channel.privFrmBufCnt = 1;
        if (ret = rk_vpss.fnSetChannelConfig(_rk_vpss_grp, index, &channel))
            return ret;
    }

    return EXIT_SUCCESS;
}

int rk_channel_grayscale(char enable)
{
    int ret;

    for (char i = 0; i < RK_VENC_CHN_NUM; i++) {
        rk_venc_para param;
        if (!rk_state[i].enable) continue;
        if (ret = rk_venc.fnGetChannelParam(i, &param))
            return ret;
        param.grayscaleOn = enable;
        if (ret = rk_venc.fnSetChannelParam(i, &param))
            return ret;
    }

    return EXIT_SUCCESS;
}

int rk_channel_unbind(char index)
{
    int ret;

    if (ret = rk_vpss.fnDisableChannel(_rk_vpss_grp, index))
        return ret;

    {
        rk_sys_bind source = { .module = RK_SYS_MOD_VPSS, 
            .device = _rk_vpss_grp, .channel = index };
        rk_sys_bind dest = { .module = RK_SYS_MOD_VENC,
            .device = _rk_venc_dev, .channel = index };
        if (ret = rk_sys.fnUnbind(&source, &dest))
            return ret;
    }

    return EXIT_SUCCESS;
}

int rk_pipeline_create(short width, short height)
{
    int ret, v4l2dev = rk_sensor_find_v4l2_endpoint();
    char endpoint[32];

    if (v4l2dev < 0)
        HAL_ERROR("rk_hal", "Failed to find sensor endpoint!\n");

    snprintf(endpoint, sizeof(endpoint), "/dev/video%d", v4l2dev);

    const char *snrEnt = rk_aiq.fnGetSensorFromV4l2(endpoint);
    if (!snrEnt)
        HAL_ERROR("rk_aiq", "Failed to get the sensor entity name!\n");

    if (ret = rk_aiq.fnPreInitBuf(snrEnt, "rkraw_rx", 2))
        HAL_ERROR("rk_aiq", "Failed to pre-initialize buffer!\n");

    if (ret = rk_aiq.fnPreInitScene(snrEnt, "normal", "day"))
        HAL_ERROR("rk_aiq", "Failed to pre-initialize scene!\n");

    if (!(_rk_aiq_ctx = rk_aiq.fnInit(snrEnt, "/oem/usr/share/iqfiles", NULL, NULL)))
        HAL_ERROR("rk_aiq", "Failed to initialize!\n");

    if (ret = rk_aiq.fnPrepare(_rk_aiq_ctx, width, height, RK_AIQ_WORK_NORMAL))
        HAL_ERROR("rk_aiq", "Failed to prepare device!\n");

    if (ret = rk_aiq.fnStart(_rk_aiq_ctx))
        HAL_ERROR("rk_aiq", "Failed to start device!\n");

    if (ret = rk_vi.fnEnableDevice(_rk_vi_dev))
        return ret;

    {
        rk_vi_bind bind;
        memset(&bind, 0, sizeof(bind));
        bind.num = 1;
        bind.pipeId[0] = _rk_vi_pipe;
        if (ret = rk_vi.fnBindPipe(_rk_vi_dev, &bind))
            return ret;
    }

    {
        rk_vi_chn channel;
        memset(&channel, 0, sizeof(channel));
        channel.size.width = width;
        channel.size.height = height;
        channel.pixFmt = RK_PIXFMT_YUV420SP;
        channel.compress = RK_COMPR_NONE;
        channel.depth = 0;
        channel.srcFps = -1;
        channel.dstFps = -1;
        channel.ispOpts.bufCount = 2;
        channel.ispOpts.bufSize = width * height * 3 / 2;
        channel.ispOpts.vidCap = RK_VI_VCAP_VIDEO_MPLANE;
        channel.ispOpts.vidMem = RK_VI_VMEM_DMABUF;
        strcpy(channel.ispOpts.entityName, "rkisp_mainpath");
        if (ret = rk_vi.fnSetChannelConfig(_rk_vi_pipe, _rk_vi_chn, &channel))
            return ret;
    }
    if (ret = rk_vi.fnEnableChannel(_rk_vi_pipe, _rk_vi_chn))
        return ret;
    
    {
        rk_vpss_grp group;
        group.dest.width = width;
        group.dest.height = height;
        group.pixFmt = RK_PIXFMT_YUV420SP;
        group.hdr = RK_HDR_SDR8;
        group.srcFps = -1;
        group.dstFps = -1;
        group.compress = RK_COMPR_NONE;
        if (ret = rk_vpss.fnCreateGroup(_rk_vpss_grp, &group))
            return ret;
        if (ret = rk_vpss.fnSetGroupDevice(_rk_vpss_grp, 1))
            return ret;
    }
    if (ret = rk_vpss.fnStartGroup(_rk_vpss_grp))
        return ret;

    {
        rk_sys_bind source = { .module = RK_SYS_MOD_VI, 
            .device = _rk_vi_dev, .channel = _rk_vi_chn };
        rk_sys_bind dest = { .module = RK_SYS_MOD_VPSS, 
            .device = _rk_vpss_grp, .channel = 0 };
        if (ret = rk_sys.fnBind(&source, &dest))
            return ret;
    }

    return EXIT_SUCCESS;
}

void rk_pipeline_destroy(void)
{
    for (char grp = 0; grp < RK_VPSS_GRP_NUM; grp++)
    {
        for (char chn = 0; chn < RK_VPSS_CHN_NUM; chn++)
            rk_vpss.fnDisableChannel(grp, chn);

        {
            rk_sys_bind source = { .module = RK_SYS_MOD_VI, 
                .device = _rk_vi_dev, .channel = _rk_vi_chn };
            rk_sys_bind dest = { .module = RK_SYS_MOD_VPSS,
                .device = grp, .channel = 0 };
            rk_sys.fnUnbind(&source, &dest);
        }

        rk_vpss.fnStopGroup(grp);
        rk_vpss.fnDestroyGroup(grp);
    }
    
    rk_vi.fnDisableChannel(_rk_vi_pipe, _rk_vi_chn);

    rk_vi.fnDisableDevice(_rk_vi_dev);

    rk_aiq.fnStop(_rk_aiq_ctx);

    rk_aiq.fnDeinit(_rk_aiq_ctx);
}

int rk_region_create(char handle, hal_rect rect, short opacity)
{
    int ret;

    rk_sys_bind channel = { .module = RK_SYS_MOD_VENC,
        .device = _rk_venc_dev, .channel = 0 };
    rk_rgn_cnf region, regionCurr;
    rk_rgn_chn attrib, attribCurr;

    memset(&region, 0, sizeof(region));
    region.type = RK_RGN_TYPE_OVERLAY;
    region.overlay.pixFmt = RK_PIXFMT_ARGB1555;
    region.overlay.size.width = rect.width;
    region.overlay.size.height = rect.height;
    region.overlay.canvas = handle + 1;

    if (rk_rgn.fnGetRegionConfig(handle, &regionCurr)) {
        HAL_INFO("rk_rgn", "Creating region %d...\n", handle);
        if (ret = rk_rgn.fnCreateRegion(handle, &region))
            return ret;
    } else if (regionCurr.overlay.size.height != region.overlay.size.height || 
        regionCurr.overlay.size.width != region.overlay.size.width) {
        HAL_INFO("rk_rgn", "Parameters are different, recreating "
            "region %d...\n", handle);
        rk_rgn.fnDetachChannel(handle, &channel);
        rk_rgn.fnDestroyRegion(handle);
        if (ret = rk_rgn.fnCreateRegion(handle, &region))
            return ret;
    }

    if (rk_rgn.fnGetChannelConfig(handle, &channel, &attribCurr))
        HAL_INFO("rk_rgn", "Attaching region %d...\n", handle);
    else if (attribCurr.overlay.point.x != rect.x || attribCurr.overlay.point.x != rect.y) {
        HAL_INFO("rk_rgn", "Position has changed, reattaching "
            "region %d...\n", handle);
        rk_rgn.fnDetachChannel(handle, &channel);
    }

    memset(&attrib, 0, sizeof(attrib));
    attrib.show = 1;
    attrib.type = RK_RGN_TYPE_OVERLAY;
    attrib.overlay.bgAlpha = 0;
    attrib.overlay.fgAlpha = opacity >> 1;
    attrib.overlay.point.x = rect.x;
    attrib.overlay.point.y = rect.y;
    attrib.overlay.layer = 7;

    rk_rgn.fnAttachChannel(handle, &channel, &attrib);

    return EXIT_SUCCESS;
}

void rk_region_destroy(char handle)
{
    rk_sys_bind channel = { .module = RK_SYS_MOD_VENC,
        .device = _rk_venc_dev, .channel = 0 };
    
    rk_rgn.fnDetachChannel(handle, &channel);
    rk_rgn.fnDestroyRegion(handle);
}

int rk_region_setbitmap(int handle, hal_bitmap *bitmap)
{
    rk_rgn_bmp nativeBmp = { .data = bitmap->data, .pixFmt = RK_PIXFMT_ARGB1555,
        .size.height = bitmap->dim.height, .size.width = bitmap->dim.width };

    return rk_rgn.fnSetBitmap(handle, &nativeBmp);
}

int rk_sensor_find_v4l2_endpoint(void)
{
    int index = -1;

    for (int i = 0; i < 64; i++)
    {
        char path[256];
        sprintf(path, "/sys/class/video4linux/video%d/name", i);

        FILE* fp = fopen(path, "rb");
        if (!fp) continue;

        char line[32];
        fgets(line, 32, fp);
        fclose(fp);

        if (EQUALS(line, "rkisp_mainpath"))
        {
            index = i;
            break;
        }
    }

    if (index == -1)
        HAL_ERROR("rk_hal", "Cannot find the corresponding V4L2 ISP device!\n");

    return index;
}

int rk_video_create(char index, hal_vidconfig *config)
{
    int ret;
    rk_venc_chn channel;
    memset(&channel, 0, sizeof(channel));
    channel.gop.mode = RK_VENC_GOPMODE_NORMALP;
    if (config->codec == HAL_VIDCODEC_JPG || config->codec == HAL_VIDCODEC_MJPG) {
        channel.attrib.codec = RK_VENC_CODEC_MJPG;
        switch (config->mode) {
            case HAL_VIDMODE_CBR:
                channel.rate.mode = RK_VENC_RATEMODE_MJPGCBR;
                channel.rate.mjpgCbr = (rk_venc_rate_mjpgcbr){ .statTime = 1, 
                    .srcFpsNum = config->framerate, .srcFpsDen = 1,
                    .dstFpsNum = config->framerate, .dstFpsDen = 1,
                    .bitrate = config->bitrate }; break;
            case HAL_VIDMODE_VBR:
                channel.rate.mode = RK_VENC_RATEMODE_MJPGVBR;
                channel.rate.mjpgVbr = (rk_venc_rate_mjpgvbr){ .statTime = 1, 
                    .srcFpsNum = config->framerate, .srcFpsDen = 1,
                    .dstFpsNum = config->framerate, .dstFpsDen = 1, .bitrate = config->bitrate,
                    .maxBitrate = MAX(config->bitrate, config->maxBitrate),
                    .minBitrate = MIN(config->bitrate, config->maxBitrate) }; break;
            case HAL_VIDMODE_QP:
                channel.rate.mode = RK_VENC_RATEMODE_MJPGQP;
                channel.rate.mjpgQp = (rk_venc_rate_mjpgqp){ .srcFpsNum = config->framerate,
                    .srcFpsDen = 1, .dstFpsNum = config->framerate, .dstFpsDen = 1,
                    .quality = config->maxQual }; break;
            default:
                HAL_ERROR("rk_venc", "MJPEG encoder can only support CBR, VBR or fixed QP modes!");
        }
    } else if (config->codec == HAL_VIDCODEC_H265) {
        channel.attrib.codec = RK_VENC_CODEC_H265;
        channel.attrib.profile = MAX(config->profile, 1);
        switch (config->mode) {
            case HAL_VIDMODE_CBR:
                channel.rate.mode = RK_VENC_RATEMODE_H265CBR;
                channel.rate.h265Cbr = (rk_venc_rate_h26xcbr){ .gop = config->gop,
                    .statTime = 1, .srcFpsNum = config->framerate, .srcFpsDen = 1,
                    .dstFpsNum = config->framerate, .dstFpsDen = 1,
                    .bitrate = config->bitrate }; break;
            case HAL_VIDMODE_VBR:
                channel.rate.mode = RK_VENC_RATEMODE_H265VBR;
                channel.rate.h265Vbr = (rk_venc_rate_h26xvbr){ .gop = config->gop,
                    .statTime = 1, .srcFpsNum = config->framerate, .srcFpsDen = 1,
                    .dstFpsNum = config->framerate, .dstFpsDen = 1, .bitrate = config->bitrate,
                    .maxBitrate = MAX(config->bitrate, config->maxBitrate),
                    .minBitrate = MIN(config->bitrate, config->maxBitrate) }; break;
            case HAL_VIDMODE_QP:
                channel.rate.mode = RK_VENC_RATEMODE_H265QP;
                channel.rate.h265Qp = (rk_venc_rate_h26xqp){ .gop = config->gop,
                    .srcFpsNum = config->framerate, .srcFpsDen = 1,
                    .dstFpsNum = config->framerate, .dstFpsDen = 1, .interQual = config->maxQual, 
                    .predQual = config->minQual, .bipredQual = config->minQual }; break;
            case HAL_VIDMODE_AVBR:
                channel.rate.mode = RK_VENC_RATEMODE_H265AVBR;
                channel.rate.h265Avbr = (rk_venc_rate_h26xvbr){ .gop = config->gop,
                    .statTime = 1, .srcFpsNum = config->framerate, .srcFpsDen = 1,
                    .dstFpsNum = config->framerate, .dstFpsDen = 1, .bitrate = config->bitrate,
                    .maxBitrate = MAX(config->bitrate, config->maxBitrate),
                    .minBitrate = MIN(config->bitrate, config->maxBitrate) }; break;
            default:
                HAL_ERROR("rk_venc", "H.265 encoder does not support this mode!");
        }
    } else if (config->codec == HAL_VIDCODEC_H264) {
        channel.attrib.codec = RK_VENC_CODEC_H264;
        switch (config->profile) {
            case 0: channel.attrib.profile = 66; break;
            case 1: channel.attrib.profile = 77; break;
            case 2: channel.attrib.profile = 100; break;
            default: HAL_ERROR("rk_venc", "H.264 encoder does not support this profile!");
        }
        channel.attrib.h264.level = 41;
        switch (config->mode) {
            case HAL_VIDMODE_CBR:
                channel.rate.mode = RK_VENC_RATEMODE_H264CBR;
                channel.rate.h264Cbr = (rk_venc_rate_h26xcbr){ .gop = config->gop,
                    .statTime = 1, .srcFpsNum = config->framerate, .srcFpsDen = 1,
                    .dstFpsNum = config->framerate, .dstFpsDen = 1,
                    .bitrate = config->bitrate }; break;
            case HAL_VIDMODE_VBR:
                channel.rate.mode = RK_VENC_RATEMODE_H264VBR;
                channel.rate.h264Vbr = (rk_venc_rate_h26xvbr){ .gop = config->gop,
                    .statTime = 1, .srcFpsNum = config->framerate, .srcFpsDen = 1,
                    .dstFpsNum = config->framerate, .dstFpsDen = 1, .bitrate = config->bitrate,
                    .maxBitrate = MAX(config->bitrate, config->maxBitrate),
                    .minBitrate = MIN(config->bitrate, config->maxBitrate) }; break;
            case HAL_VIDMODE_QP:
                channel.rate.mode = RK_VENC_RATEMODE_H264QP;
                channel.rate.h264Qp = (rk_venc_rate_h26xqp){ .gop = config->gop,
                    .srcFpsNum = config->framerate, .srcFpsDen = 1,
                    .dstFpsNum = config->framerate, .dstFpsDen = 1, .interQual = config->maxQual, 
                    .predQual = config->minQual, .bipredQual = config->minQual }; break;
            case HAL_VIDMODE_AVBR:
                channel.rate.mode = RK_VENC_RATEMODE_H264AVBR;
                channel.rate.h264Avbr = (rk_venc_rate_h26xvbr){ .gop = config->gop,
                    .statTime = 1, .srcFpsNum = config->framerate, .srcFpsDen = 1,
                    .dstFpsNum = config->framerate, .dstFpsDen = 1, .bitrate = config->bitrate,
                    .maxBitrate = MAX(config->bitrate, config->maxBitrate),
                    .minBitrate = MIN(config->bitrate, config->maxBitrate) }; break;
            default:
                HAL_ERROR("rk_venc", "H.264 encoder does not support this mode!");
        }
    } else HAL_ERROR("rk_venc", "This codec is not supported by the hardware!");

    channel.attrib.pixFmt = RK_PIXFMT_YUV420SP;
    channel.attrib.bufSize = config->height * config->width * 3 / 2;
    channel.attrib.byFrame = 1;
    channel.attrib.pic.width = config->width;
    channel.attrib.pic.height = config->height;
    channel.attrib.vir.width = config->width;
    channel.attrib.vir.height = config->height;
    channel.attrib.strmBufCnt = 2;

    if (ret = rk_venc.fnCreateChannel(index, &channel))
        return ret;

    {
        int enable = 1;
        if (ret = rk_venc.fnSetChannelBufferShare(index, &enable))
        return ret;
    }

    {
        int count = -1;
        if (config->codec != HAL_VIDCODEC_JPG && 
            (ret = rk_venc.fnStartReceivingEx(index, &count)))
            return ret;
    }
    
    rk_state[index].payload = config->codec;

    return EXIT_SUCCESS;
}

int rk_video_destroy(char index)
{
    int ret;

    rk_state[index].enable = 0;
    rk_state[index].payload = HAL_VIDCODEC_UNSPEC;

    rk_venc.fnStopReceiving(index);

    {
        rk_sys_bind source = { .module = RK_SYS_MOD_VPSS, 
            .device = _rk_vpss_grp, .channel = index };
        rk_sys_bind dest = { .module = RK_SYS_MOD_VENC,
            .device = _rk_venc_dev, .channel = index };
        if (ret = rk_sys.fnUnbind(&source, &dest))
            return ret;
    }

    if (ret = rk_venc.fnDestroyChannel(index))
        return ret;
    
    if (ret = rk_vpss.fnDisableChannel(_rk_vpss_grp, index))
        return ret;

    return EXIT_SUCCESS;
}
    
int rk_video_destroy_all(void)
{
    int ret;

    for (char i = 0; i < RK_VENC_CHN_NUM; i++)
        if (rk_state[i].enable)
            if (ret = rk_video_destroy(i))
                return ret;

    return EXIT_SUCCESS;
}

void rk_video_request_idr(char index)
{
    rk_venc.fnRequestIdr(index, 1);
}

int rk_video_snapshot_grab(char index, hal_jpegdata *jpeg)
{
    int ret;

    if (ret = rk_channel_bind(index)) {
        HAL_DANGER("rk_venc", "Binding the encoder channel "
            "%d failed with %#x!\n", index, ret);
        goto abort;
    }

    unsigned int count = 1;
    if (rk_venc.fnStartReceivingEx(index, &count)) {
        HAL_DANGER("rk_venc", "Requesting one frame "
            "%d failed with %#x!\n", index, ret);
        goto abort;
    }

    int fd = rk_venc.fnGetDescriptor(index);

    struct timeval timeout = { .tv_sec = 2, .tv_usec = 0 };
    fd_set readFds;
    FD_ZERO(&readFds);
    FD_SET(fd, &readFds);
    ret = select(fd + 1, &readFds, NULL, NULL, &timeout);
    if (ret < 0) {
        HAL_DANGER("rk_venc", "Select operation failed!\n");
        goto abort;
    } else if (ret == 0) {
        HAL_DANGER("rk_venc", "Capture stream timed out!\n");
        goto abort;
    }

    if (FD_ISSET(fd, &readFds)) {
        rk_venc_stat stat;
        if (rk_venc.fnQuery(index, &stat)) {
            HAL_DANGER("rk_venc", "Querying the encoder channel "
                "%d failed with %#x!\n", index, ret);
            goto abort;
        }

        if (!stat.curPacks) {
            HAL_DANGER("rk_venc", "Current frame is empty, skipping it!\n");
            goto abort;
        }

        rk_venc_strm strm;
        memset(&strm, 0, sizeof(strm));
        strm.packet = (rk_venc_pack*)malloc(sizeof(rk_venc_pack) * stat.curPacks);
        if (!strm.packet) {
            HAL_DANGER("rk_venc", "Memory allocation on channel %d failed!\n", index);
            goto abort;
        }
        strm.count = stat.curPacks;

        if (ret = rk_venc.fnGetStream(index, &strm, 40)) {
            HAL_DANGER("rk_venc", "Getting the stream on "
                "channel %d failed with %#x!\n", index, ret);
            free(strm.packet);
            strm.packet = NULL;
            goto abort;
        }

        {
            jpeg->jpegSize = 0;
            for (unsigned int i = 0; i < strm.count; i++) {
                rk_venc_pack *pack = &strm.packet[i];
                unsigned int packLen = pack->length - pack->offset;
                unsigned char *packData = rk_mb.fnGetData(pack->mbBlk) + pack->offset;

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
        rk_venc.fnFreeStream(index, &strm);
    }

    rk_venc.fnFreeDescriptor(index);

    rk_venc.fnStopReceiving(index);

    rk_channel_unbind(index);

    return ret;
}

void *rk_video_thread(void)
{
    int ret;
    int maxFd = 0;

    for (int i = 0; i < RK_VENC_CHN_NUM; i++) {
        if (!rk_state[i].enable) continue;
        if (!rk_state[i].mainLoop) continue;

        ret = rk_venc.fnGetDescriptor(i);
        if (ret < 0) {
            HAL_DANGER("rk_venc", "Getting the encoder descriptor failed with %#x!\n", ret);
            return NULL;
        }
        rk_state[i].fileDesc = ret;

        if (maxFd <= rk_state[i].fileDesc)
            maxFd = rk_state[i].fileDesc;
    }

    rk_venc_stat stat;
    rk_venc_strm stream;
    struct timeval timeout;
    fd_set readFds;

    while (keepRunning) {
        FD_ZERO(&readFds);
        for(int i = 0; i < RK_VENC_CHN_NUM; i++) {
            if (!rk_state[i].enable) continue;
            if (!rk_state[i].mainLoop) continue;
            FD_SET(rk_state[i].fileDesc, &readFds);
        }

        timeout.tv_sec = 2;
        timeout.tv_usec = 0;
        ret = select(maxFd + 1, &readFds, NULL, NULL, &timeout);
        if (ret < 0) {
            HAL_DANGER("rk_venc", "Select operation failed!\n");
            break;
        } else if (ret == 0) {
            HAL_WARNING("rk_venc", "Main stream loop timed out!\n");
            continue;
        } else {
            for (int i = 0; i < RK_VENC_CHN_NUM; i++) {
                if (!rk_state[i].enable) continue;
                if (!rk_state[i].mainLoop) continue;
                if (FD_ISSET(rk_state[i].fileDesc, &readFds)) {
                    memset(&stream, 0, sizeof(stream));
                    
                    /*if (ret = rk_venc.fnQuery(i, &stat)) {
                        HAL_DANGER("rk_venc", "Querying the encoder channel "
                            "%d failed with %#x!\n", i, ret);
                        break;
                    }

                    if (!stat.curPacks) {
                        HAL_INFO("rk_venc", "Current frame is empty, skipping it!\n");
                        continue;
                    }*/stat.curPacks = 1;

                    stream.packet = (rk_venc_pack*)malloc(
                        sizeof(rk_venc_pack) * stat.curPacks);
                    if (!stream.packet) {
                        HAL_DANGER("rk_venc", "Memory allocation on channel %d failed!\n", i);
                        break;
                    }
                    stream.count = stat.curPacks;

                    if (ret = rk_venc.fnGetStream(i, &stream, 40)) {
                        HAL_DANGER("rk_venc", "Getting the stream on "
                            "channel %d failed with %#x!\n", i, ret);
                        goto free;
                    }

                    if (rk_vid_cb) {
                        hal_vidstream outStrm;
                        hal_vidpack outPack[stream.count];
                        outStrm.count = stream.count;
                        outStrm.seq = stream.sequence;
                        for (int j = 0; j < stream.count; j++) {
                            rk_venc_pack *pack = &stream.packet[j];
                            outPack[j].data = rk_mb.fnGetData(pack->mbBlk);
                            outPack[j].length = pack->length;
                            outPack[j].naluCnt = 0;
                            outPack[j].nalu[0].length = pack->length;
                            outPack[j].nalu[0].offset = 0;
                            switch (rk_state[i].payload) {
                                case HAL_VIDCODEC_H264:
                                    if (pack->naluType.h264Nalu != RK_VENC_NALU_H264_IDRSLICE) {
                                        signed char n = 0;
                                        for (unsigned int p = 0; p < outPack[j].length - 4; p++) {
                                            if (outPack[j].data[p] || outPack[j].data[p + 1] ||
                                                outPack[j].data[p + 2] || outPack[j].data[p + 3] != 1) continue;
                                            outPack[0].nalu[n].type = outPack[j].data[p + 4] & 0x1F;
                                            outPack[0].nalu[n++].offset = p;
                                        }
                                        outPack[0].naluCnt = n;
                                        outPack[0].nalu[n].offset = pack->length;
                                        for (n = 0; n < outPack[0].naluCnt; n++)
                                            outPack[0].nalu[n].length = 
                                                outPack[0].nalu[n + 1].offset -
                                                outPack[0].nalu[n].offset;
                                    }
                                    else outPack[j].nalu[++outPack[j].naluCnt].type = pack->naluType.h264Nalu;
                                    break;
                                case HAL_VIDCODEC_H265:
                                    outPack[j].nalu[++outPack[j].naluCnt].type = pack->naluType.h265Nalu;
                                    break;
                            }
                            outPack[j].offset = 0;
                            outPack[j].timestamp = pack->timestamp;
                        }
                        outStrm.pack = outPack;
                        (*rk_vid_cb)(i, &outStrm);
                    }

                    if (ret = rk_venc.fnFreeStream(i, &stream)) {
                        HAL_DANGER("rk_venc", "Releasing the stream on "
                            "channel %d failed with %#x!\n", i, ret);
                    }
free:
                    free(stream.packet);
                    stream.packet = NULL;
                }
            }
        }
    }
    HAL_INFO("rk_venc", "Shutting down encoding thread...\n");
}

void rk_system_deinit(void)
{
    rk_sys.fnExit();
}

int rk_system_init(char *snrConfig)
{
    int ret;

    printf("App built with headers v%s\n", RK_SYS_API);

    rk_sys.fnExit();

    if (ret = rk_sys.fnInit())
        return ret;

    return EXIT_SUCCESS;
}

#endif