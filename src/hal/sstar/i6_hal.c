#include "i6_hal.h"

i6_isp_impl  i6_isp;
i6_rgn_impl  i6_rgn;
i6_snr_impl  i6_snr;
i6_sys_impl  i6_sys;
i6_venc_impl i6_venc;
i6_vif_impl  i6_vif;
i6_vpe_impl  i6_vpe;

hal_chnstate i6_state[I6_VENC_CHN_NUM] = {0};
int (*i6_venc_cb)(char, hal_vidstream*);

i6_snr_pad _i6_snr_pad;
i6_snr_plane _i6_snr_plane;
char _i6_snr_framerate, _i6_snr_hdr, _i6_snr_index, _i6_snr_profile;

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
    i6_sys_unload(&i6_sys);
}

int i6_hal_init(void)
{
    int ret;

    if (ret = i6_sys_load(&i6_sys))
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

int i6_channel_bind(char index, char framerate, char jpeg)
{
    int ret;

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
        if (ret = i6_sys.fnBindExt(&source, &dest, framerate, framerate,
            I6_SYS_LINK_FRAMEBASE, 0))
            return ret;
    }

    return EXIT_SUCCESS;
}

int i6_channel_create(char index, short width, short height, char mirror, char flip, char jpeg)
{
    i6_vpe_port port;
    port.output.width = width;
    port.output.height = height;
    port.mirror = mirror;
    port.flip = flip;
    port.compress = I6_COMPR_NONE;
    port.pixFmt = jpeg ? I6_PIXFMT_YUV422_YUYV : I6_PIXFMT_YUV420SP;

    return i6_vpe.fnSetPortConfig(_i6_vpe_chn, index, &port);
}

int i6_channel_grayscale(char enable)
{
    return i6_isp.fnSetColorToGray(0, &enable);
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

int i6_encoder_create(char index, hal_vidconfig *config)
{
    int ret;
    i6_venc_chn channel;
    i6_venc_attr_h26x *attrib;
    
    if (config->codec == HAL_VIDCODEC_JPG || config->codec == HAL_VIDCODEC_MJPG) {
        channel.attrib.codec = I6_VENC_CODEC_MJPG;
        switch (config->mode) {
            case HAL_VIDMODE_CBR:
                channel.rate.mode = I6_VENC_RATEMODE_MJPGCBR;
                channel.rate.mjpgCbr.bitrate = config->bitrate << 10;
                channel.rate.mjpgCbr.fpsNum = 
                    config->codec == HAL_VIDCODEC_JPG ? 1 : config->framerate;
                channel.rate.mjpgCbr.fpsDen = 1;
                break;
            case HAL_VIDMODE_QP:
                channel.rate.mode = I6_VENC_RATEMODE_MJPGQP;
                channel.rate.mjpgQp.fpsNum = 
                    config->codec == HAL_VIDCODEC_JPG ? 1 : config->framerate;
                channel.rate.mjpgCbr.fpsDen = 1;
                channel.rate.mjpgQp.quality = MAX(config->minQual, config->maxQual);
                break;
            default:
                I6_ERROR("MJPEG encoder can only support CBR or fixed QP modes!");
        }

        channel.attrib.mjpg.maxHeight = ALIGN_BACK(config->height, 16);
        channel.attrib.mjpg.maxWidth = ALIGN_BACK(config->width, 16);
        channel.attrib.mjpg.bufSize = config->width * config->height;
        channel.attrib.mjpg.byFrame = 1;
        channel.attrib.mjpg.height = ALIGN_BACK(config->height, 16);
        channel.attrib.mjpg.width = ALIGN_BACK(config->width, 16);
        channel.attrib.mjpg.dcfThumbs = 0;
        channel.attrib.mjpg.markPerRow = 0;

        goto attach;
    } else if (config->codec == HAL_VIDCODEC_H265) {
        channel.attrib.codec = I6_VENC_CODEC_H265;
        attrib = &channel.attrib.h265;
        switch (config->mode) {
            case HAL_VIDMODE_CBR:
                channel.rate.mode = I6_VENC_RATEMODE_H265CBR;
                channel.rate.h265Cbr = (i6_venc_rate_h26xcbr){ .gop = config->gop,
                    .statTime = 1, .fpsNum = config->framerate, .fpsDen = 1, .bitrate = 
                    (unsigned int)(config->bitrate) << 10, .avgLvl = 1 }; break;
            case HAL_VIDMODE_VBR:
                channel.rate.mode = I6_VENC_RATEMODE_H265VBR;
                channel.rate.h265Vbr = (i6_venc_rate_h26xvbr){ .gop = config->gop,
                    .statTime = 1, .fpsNum = config->framerate, .fpsDen = 1, .maxBitrate = 
                    (unsigned int)(MAX(config->bitrate, config->maxBitrate)) << 10,
                    .maxQual = config->maxQual, .minQual = config->minQual }; break;
            case HAL_VIDMODE_QP:
                channel.rate.mode = I6_VENC_RATEMODE_H265QP;
                channel.rate.h265Qp = (i6_venc_rate_h26xqp){ .gop = config->gop,
                    .fpsNum = config->framerate, .fpsDen = 1, .interQual = config->maxQual,
                    .predQual = config->minQual }; break;
            case HAL_VIDMODE_ABR:
                I6_ERROR("H.265 encoder does not support ABR mode!");
            case HAL_VIDMODE_AVBR:
                channel.rate.mode = I6_VENC_RATEMODE_H265AVBR;
                channel.rate.h265Avbr = (i6_venc_rate_h26xvbr){ .gop = config->gop,
                    .statTime = 1, .fpsNum = config->framerate, .fpsDen = 1, .maxBitrate = 
                    (unsigned int)(MAX(config->bitrate, config->maxBitrate)) << 10,
                    .maxQual = config->maxQual, .minQual = config->minQual }; break;
            default:
                I6_ERROR("H.265 encoder does not support this mode!");
        }  
    } else if (config->codec == HAL_VIDCODEC_H264) {
        channel.attrib.codec = I6_VENC_CODEC_H264;
        attrib = &channel.attrib.h264;
        switch (config->mode) {
            case HAL_VIDMODE_CBR:
                channel.rate.mode = I6_VENC_RATEMODE_H264CBR;
                channel.rate.h264Cbr = (i6_venc_rate_h26xcbr){ .gop = config->gop,
                    .statTime = 1, .fpsNum = config->framerate, .fpsDen = 1, .bitrate = 
                    (unsigned int)(config->bitrate) << 10, .avgLvl = 1 }; break;
            case HAL_VIDMODE_VBR:
                channel.rate.mode = I6_VENC_RATEMODE_H264VBR;
                channel.rate.h264Vbr = (i6_venc_rate_h26xvbr){ .gop = config->gop,
                    .statTime = 1, .fpsNum = config->framerate, .fpsDen = 1, .maxBitrate = 
                    (unsigned int)(MAX(config->bitrate, config->maxBitrate)) << 10,
                    .maxQual = config->maxQual, .minQual = config->minQual }; break;
            case HAL_VIDMODE_QP:
                channel.rate.mode = I6_VENC_RATEMODE_H264QP;
                channel.rate.h264Qp = (i6_venc_rate_h26xqp){ .gop = config->gop,
                    .fpsNum = config->framerate, .fpsDen = 1, .interQual = config->maxQual,
                    .predQual = config->minQual }; break;
            case HAL_VIDMODE_ABR:
                channel.rate.mode = I6_VENC_RATEMODE_H264ABR;
                channel.rate.h264Abr = (i6_venc_rate_h26xabr){ .gop = config->gop,
                    .statTime = 1, .fpsNum = config->framerate, .fpsDen = 1,
                    .avgBitrate = (unsigned int)(config->bitrate) << 10,
                    .maxBitrate = (unsigned int)(config->maxBitrate) << 10 }; break;
            case HAL_VIDMODE_AVBR:
                channel.rate.mode = I6_VENC_RATEMODE_H264AVBR;
                channel.rate.h264Avbr = (i6_venc_rate_h26xvbr){ .gop = config->gop, .statTime = 1,
                    .fpsNum = config->framerate, .fpsDen = 1, .maxBitrate = 
                    (unsigned int)(MAX(config->bitrate, config->maxBitrate)) << 10,
                    .maxQual = config->maxQual, .minQual = config->minQual }; break;
            default:
                I6_ERROR("H.264 encoder does not support this mode!");
        }
    } else I6_ERROR("This codec is not supported by the hardware!");
    attrib->maxHeight = ALIGN_BACK(config->height, 16);
    attrib->maxWidth = ALIGN_BACK(config->width, 16);
    attrib->bufSize = config->height * config->width;
    attrib->profile = config->profile;
    attrib->byFrame = 1;
    attrib->height = ALIGN_BACK(config->height, 16);
    attrib->width = ALIGN_BACK(config->width, 16);
    attrib->bFrameNum = 0;
    attrib->refNum = 1;
attach:
    if (ret = i6_venc.fnCreateChannel(index, &channel))
        return ret;

    if (config->codec != HAL_VIDCODEC_JPG && 
        (ret = i6_venc.fnStartReceiving(index)))
        return ret;

    i6_state[index].payload = config->codec;

    return EXIT_SUCCESS;
}

int i6_encoder_destroy(char index)
{
    int ret;

    i6_state[index].payload = HAL_VIDCODEC_UNSPEC;

    if (ret = i6_venc.fnStopReceiving(index))
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

    if (ret = i6_venc.fnDestroyChannel(index))
        return ret;
    
    if (ret = i6_vpe.fnDisablePort(_i6_vpe_chn, index))
        return ret;

    return EXIT_SUCCESS;
}

int i6_encoder_destroy_all(void)
{
    int ret;

    for (char i = 0; i < I6_VENC_CHN_NUM; i++)
        if (i6_state[i].enable)
            if (ret = i6_encoder_destroy(i))
                return ret;

    return EXIT_SUCCESS;
}

int i6_encoder_snapshot_grab(char index, short width, short height, 
    char quality, char grayscale, hal_jpegdata *jpeg)
{
    int ret;

    if (ret = i6_channel_bind(index, 1, 1)) {
        fprintf(stderr, "[i6_venc] Binding the encoder channel "
            "%d failed with %#x!\n", index, ret);
        goto abort;
    }

    i6_venc_jpg param;
    memset(&param, 0, sizeof(param));
    if (ret = i6_venc.fnGetJpegParam(index, &param)) {
        fprintf(stderr, "[i6_venc] Reading the JPEG settings "
            "%d failed with %#x!\n", index, ret);
        goto abort;
    }

    param.quality = quality;
    if (ret = i6_venc.fnSetJpegParam(index, &param)) {
        fprintf(stderr, "[i6_venc] Writing the JPEG settings "
            "%d failed with %#x!\n", index, ret);
        goto abort;
    }

    i6_channel_grayscale(grayscale);

    unsigned int count = 1;
    if (i6_venc.fnStartReceivingEx(index, &count)) {
        fprintf(stderr, "[i6_venc] Requesting one frame "
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
        fprintf(stderr, "[i6_venc] Select operation failed!\n");
        goto abort;
    } else if (ret == 0) {
        fprintf(stderr, "[i6_venc] Capture stream timed out!\n");
        goto abort;
    }

    if (FD_ISSET(fd, &readFds)) {
        i6_venc_stat stat;
        if (i6_venc.fnQuery(index, &stat)) {
            fprintf(stderr, "[i6_venc] Querying the encoder channel "
                "%d failed with %#x!\n", index, ret);
            goto abort;
        }

        if (!stat.curPacks) {
            fprintf(stderr, "[i6_venc] Current frame is empty, skipping it!\n");
            goto abort;
        }

        i6_venc_strm strm;
        memset(&strm, 0, sizeof(strm));
        strm.packet = (i6_venc_pack*)malloc(sizeof(i6_venc_pack) * stat.curPacks);
        if (!strm.packet) {
            fprintf(stderr, "[i6_venc] Memory allocation on channel %d failed!\n", index);
            goto abort;
        }
        strm.count = stat.curPacks;

        if (ret = i6_venc.fnGetStream(index, &strm, stat.curPacks)) {
            fprintf(stderr, "[i6_venc] Getting the stream on "
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

void *i6_encoder_thread(void)
{
    int ret;
    int maxFd = 0;

    for (int i = 0; i < I6_VENC_CHN_NUM; i++) {
        if (!i6_state[i].enable) continue;
        if (!i6_state[i].mainLoop) continue;

        ret = i6_venc.fnGetDescriptor(i);
        if (ret < 0) {
            fprintf(stderr, "[i6_venc] Getting the encoder descriptor failed with %#x!\n", ret);
            return NULL;
        }
        i6_state[i].fileDesc = ret;

        if (maxFd <= i6_state[i].fileDesc)
            maxFd = i6_state[i].fileDesc;
    }

    i6_venc_stat stat;
    i6_venc_strm stream;
    struct timeval timeout;
    fd_set readFds;

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
            fprintf(stderr, "[i6_venc] Select operation failed!\n");
            break;
        } else if (ret == 0) {
            fprintf(stderr, "[i6_venc] Main stream loop timed out!\n");
            continue;
        } else {
            for (int i = 0; i < I6_VENC_CHN_NUM; i++) {
                if (!i6_state[i].enable) continue;
                if (!i6_state[i].mainLoop) continue;
                if (FD_ISSET(i6_state[i].fileDesc, &readFds)) {
                    memset(&stream, 0, sizeof(stream));
                    
                    if (ret = i6_venc.fnQuery(i, &stat)) {
                        fprintf(stderr, "[i6_venc] Querying the encoder channel "
                            "%d failed with %#x!\n", i, ret);
                        break;
                    }

                    if (!stat.curPacks) {
                        fprintf(stderr, "[i6_venc] Current frame is empty, skipping it!\n");
                        continue;
                    }

                    stream.packet = (i6_venc_pack*)malloc(
                        sizeof(i6_venc_pack) * stat.curPacks);
                    if (!stream.packet) {
                        fprintf(stderr, "[i6_venc] Memory allocation on channel %d failed!\n", i);
                        break;
                    }
                    stream.count = stat.curPacks;

                    if (ret = i6_venc.fnGetStream(i, &stream, stat.curPacks)) {
                        fprintf(stderr, "[i6_venc] Getting the stream on "
                            "channel %d failed with %#x!\n", i, ret);
                        break;
                    }

                    if (i6_venc_cb) {
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
                        (*i6_venc_cb)(i, &outStrm);
                    }

                    if (ret = i6_venc.fnFreeStream(i, &stream)) {
                        fprintf(stderr, "[i6_venc] Releasing the stream on "
                            "channel %d failed with %#x!\n", i, ret);
                    }
                    free(stream.packet);
                    stream.packet = NULL;
                }
            }
        }
    }
    fprintf(stderr, "[i6_venc] Shutting down encoding thread...\n");
}

int i6_pipeline_create(char sensor, short width, short height, char framerate)
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

            if (width > resolution.crop.width ||
                height > resolution.crop.height ||
                framerate > resolution.maxFps)
                continue;
        
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
        if (ret = i6_vif.fnSetPortConfig(_i6_vif_chn, _i6_vif_port, &port))
            return ret;
    }
    if (ret = i6_vif.fnEnablePort(_i6_vif_chn, _i6_vif_port))
        return ret;

    {
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
    }

    {
        i6_vpe_para param;
        param.hdr = I6_HDR_OFF;
        param.level3DNR = 0;
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

int i6_region_create(char handle, hal_rect rect)
{
    int ret;

    i6_sys_bind channel = { .module = I6_SYS_MOD_VPE,
        .device = _i6_vpe_dev, .channel = _i6_vpe_chn };
    i6_rgn_cnf region, regionCurr;
    i6_rgn_chn attrib, attribCurr;

    region.type = I6_RGN_TYPE_OSD;
    region.pixFmt = I6_RGN_PIXFMT_ARGB1555;
    region.size.width = rect.width;
    region.size.height = rect.height;
    if (ret = i6_rgn.fnGetRegionParam(handle, &regionCurr))
        return ret;
    if (ret = i6_rgn.fnCreateRegion(handle, &region))
        return ret;

    if (regionCurr.size.height != region.size.height || 
        regionCurr.size.width != region.size.width) {
        fprintf(stderr, "[i6_rgn] Parameters are different, recreating "
            "region %d...\n", handle);
        channel.port = 1;
        i6_rgn.fnDetachChannel(handle, &channel);
        channel.port = 0;
        i6_rgn.fnDetachChannel(handle, &channel);
        i6_rgn.fnDestroyRegion(handle);
        if (ret = i6_rgn.fnCreateRegion(handle, &region))
            return ret;
    }

    if (ret = i6_rgn.fnGetChannelConfig(handle, &channel, &attribCurr))
        fprintf(stderr, "[i6_rgn] Attaching region %d...\n", handle);
    else if (attribCurr.point.x != rect.x || attribCurr.point.x != rect.y) {
        fprintf(stderr, "[i6_rgn] Position has changed, detaching "
            "region %d...\n", handle);
        channel.port = 1;
        i6_rgn.fnDetachChannel(handle, &channel);
        channel.port = 0;
        i6_rgn.fnDetachChannel(handle, &channel);
    }

    memset(&attrib, 0, sizeof(attrib));
    attrib.show = 1;
    attrib.point.x = rect.x;
    attrib.point.y = rect.y;
    attrib.osd.layer = 0;
    attrib.osd.constAlphaOn = 0;
    attrib.osd.bgFgAlpha[0] = 0;
    attrib.osd.bgFgAlpha[1] = 255;

    channel.port = 0;
    i6_rgn.fnAttachChannel(handle, &channel, &attrib);
    channel.port = 1;
    i6_rgn.fnAttachChannel(handle, &channel, &attrib);

    return ret;
}

void i6_region_destroy(char handle)
{
    i6_sys_bind channel = { .module = I6_SYS_MOD_VPE,
        .device = _i6_vpe_dev, .channel = _i6_vpe_chn };
    
    channel.port = 1;
    i6_rgn.fnDetachChannel(handle, &channel);
    channel.port = 0;
    i6_rgn.fnDetachChannel(handle, &channel);
    i6_rgn.fnDestroyRegion(handle);
}

int i6_region_setbitmap(int handle, hal_dim dim, void *data)
{
    i6_rgn_bmp bitmap = { .data = data, .pixFmt = I6_RGN_PIXFMT_ARGB1555,
        .size.height = dim.height, .size.width = dim.width };

    return i6_rgn.fnSetBitmap(handle, &bitmap);
}

void i6_system_deinit(void)
{
    i6_sys.fnExit();
}

int i6_system_init(void)
{
    int ret;

    {
        i6_sys_ver version;
        if (ret = i6_sys.fnGetVersion(&version))
            return ret;
        printf("App built with headers v%s\n", I6_SYS_API);
        puts(version.version);
    }

    if (ret = i6_sys.fnInit())
        return ret;

    return EXIT_SUCCESS;
}