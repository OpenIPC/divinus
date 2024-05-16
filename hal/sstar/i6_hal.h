#include "i6_common.h"

i6_isp_impl i6_isp;
i6_snr_impl i6_snr;
i6_sys_impl i6_sys;
i6_venc_impl i6_venc;
i6_vif_impl i6_vif;
i6_vpe_impl i6_vpe;

hal_chnstate i6_state[I6_VENC_CHN_NUM] = {0};

i6_snr_pad snr_pad;
i6_snr_plane snr_plane;
char snr_framerate, snr_hdr, snr_index, snr_profile;

char venc_port = 0;
char vif_chn = 0;
char vif_dev = 0;
char vif_port = 0;
char vpe_chn = 0;
char vpe_dev = 0;
char vpe_port = 0;

int i6_hal_init()
{
    int ret;

    if (ret = i6_isp_load(&i6_isp))
        return ret;
    if (ret = i6_snr_load(&i6_snr))
        return ret;
    if (ret = i6_sys_load(&i6_sys))
        return ret;
    if (ret = i6_venc_load(&i6_venc))
        return ret;
    if (ret = i6_vif_load(&i6_vif))
        return ret;
    if (ret = i6_vpe_load(&i6_vpe))
        return ret;

    return EXIT_SUCCESS;
}

void i6_hal_deinit()
{
    i6_vpe_unload(&i6_vpe);
    i6_vif_unload(&i6_vif);
    i6_venc_unload(&i6_venc);
    i6_sys_unload(&i6_sys);
    i6_snr_unload(&i6_snr);
    i6_isp_unload(&i6_isp);
}

int i6_channel_bind(char index, char framerate, char jpeg)
{
    int ret;

    if (ret = i6_vpe.fnEnablePort(vpe_chn, index))
        return ret;

    {
        unsigned int device;
        if (ret = i6_venc.fnGetChannelDeviceId(index, &device))
            return ret;
        i6_sys_bind source = { .module = I6_SYS_MOD_VPE, 
            .device = vpe_dev, .channel = vpe_chn, .port = index };
        i6_sys_bind dest = { .module = I6_SYS_MOD_VENC,
            .device = device, .channel = index, .port = venc_port };
        if (ret = i6_sys.fnBindExt(&source, &dest, framerate, framerate,
            I6_SYS_LINK_FRAMEBASE, 0))
            return ret;
    }

    return EXIT_SUCCESS;
}

int i6_channel_create(char index, short width, short height, char jpeg)
{
    i6_vpe_port port;
    port.output.width = width;
    port.output.height = height;
    port.mirror = 0;
    port.flip = 0;
    port.compress = I6_COMPR_NONE;
    port.pixFmt = jpeg ? I6_PIXFMT_YUV422_YUYV : I6_PIXFMT_YUV420SP;

    return i6_vpe.fnSetPortConfig(vpe_chn, index, &port);
}

int i6_channel_grayscale(char index, char enable)
{
    return i6_isp.fnSetColorToGray(index, &enable);
}

int i6_channel_unbind(char index)
{
    int ret;

    if (ret = i6_vpe.fnDisablePort(vpe_chn, index))
        return ret;

    {
        unsigned int device;
        if (ret = i6_venc.fnGetChannelDeviceId(index, &device))
            return ret;
        i6_sys_bind source = { .module = I6_SYS_MOD_VPE, 
            .device = vpe_dev, .channel = vpe_chn, .port = index };
        i6_sys_bind dest = { .module = I6_SYS_MOD_VENC,
            .device = device, .channel = index, .port = venc_port };
        if (ret = i6_sys.fnUnbind(&source, &dest))
            return ret;
    }

    return EXIT_SUCCESS; 
}

int i6_config_load(char *path)
{
    return i6_isp.fnLoadChannelConfig(isp_chn, path, 1234);
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
                channel.rate.mjpgQp.fpsNum = config->framerate;
                channel.rate.mjpgQp.fpsDen = 
                    config->codec == HAL_VIDCODEC_JPG ? 1 : config->framerate;
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
        attrib = &channel.attrib.h265;
        switch (config->mode) {
            case HAL_VIDMODE_CBR:
                channel.rate.mode = I6_VENC_RATEMODE_H265CBR;
                channel.rate.h265Cbr = (i6_venc_rate_h26xcbr){ .gop = config->gop,
                    .statTime = 0, .fpsNum = config->framerate, .fpsDen = 1, .bitrate = 
                    (unsigned int)(config->bitrate << 10), .avgLvl = 0 }; break;
            case HAL_VIDMODE_VBR:
                channel.rate.mode = I6_VENC_RATEMODE_H265VBR;
                channel.rate.h265Vbr = (i6_venc_rate_h26xvbr){ .gop = config->gop,
                    .statTime = 0, .fpsNum = config->framerate, .fpsDen = 1, .maxBitrate = 
                    (unsigned int)(MAX(config->bitrate, config->maxBitrate) << 10),
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
                    .statTime = 0, .fpsNum = config->framerate, .fpsDen = 1, .maxBitrate = 
                    (unsigned int)(MAX(config->bitrate, config->maxBitrate) << 10),
                    .maxQual = config->maxQual, .minQual = config->minQual }; break;
            default:
                I6_ERROR("H.265 encoder does not support this mode!");
        }  
    } else if (config->codec == HAL_VIDCODEC_H264) {
        attrib = &channel.attrib.h264;
        switch (config->mode) {
            case HAL_VIDMODE_CBR:
                channel.rate.mode = I6_VENC_RATEMODE_H264CBR;
                channel.rate.h264Cbr = (i6_venc_rate_h26xcbr){ .gop = config->gop,
                    .statTime = 0, .fpsNum = config->framerate, .fpsDen = 1, .bitrate = 
                    (unsigned int)(config->bitrate << 10), .avgLvl = 0 }; break;
            case HAL_VIDMODE_VBR:
                channel.rate.mode = I6_VENC_RATEMODE_H264VBR;
                channel.rate.h264Vbr = (i6_venc_rate_h26xvbr){ .gop = config->gop,
                    .statTime = 0, .fpsNum = config->framerate, .fpsDen = 1, .maxBitrate = 
                    (unsigned int)(MAX(config->bitrate, config->maxBitrate) << 10),
                    .maxQual = config->maxQual, .minQual = config->minQual }; break;
            case HAL_VIDMODE_QP:
                channel.rate.mode = I6_VENC_RATEMODE_H264QP;
                channel.rate.h264Qp = (i6_venc_rate_h26xqp){ .gop = config->gop,
                    .fpsNum = config->framerate, .fpsDen = 1, .interQual = config->maxQual,
                    .predQual = config->minQual }; break;
            case HAL_VIDMODE_ABR:
                channel.rate.mode = I6_VENC_RATEMODE_H264ABR;
                channel.rate.h264Abr = (i6_venc_rate_h26xabr){ .gop = config->gop,
                    .statTime = 0, .fpsNum = config->framerate, .fpsDen = 1,
                    .avgBitrate = (unsigned int)(config->bitrate << 10),
                    .maxBitrate = (unsigned int)(config->maxBitrate << 10) }; break;
            case HAL_VIDMODE_AVBR:
                channel.rate.mode = I6_VENC_RATEMODE_H264AVBR;
                channel.rate.h264Avbr = (i6_venc_rate_h26xvbr){ .gop = config->gop, .statTime = 0,
                    .fpsNum = config->framerate, .fpsDen = 1, .maxBitrate = 
                    (unsigned int)(MAX(config->bitrate, config->maxBitrate) << 10),
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
            .device = vpe_dev, .channel = vpe_chn, .port = index };
        i6_sys_bind dest = { .module = I6_SYS_MOD_VENC,
            .device = device, .channel = index, .port = venc_port };
        if (ret = i6_sys.fnUnbind(&source, &dest))
            return ret;
    }

    if (ret = i6_venc.fnDestroyChannel(index))
        return ret;
    
    if (ret = i6_vpe.fnDisablePort(vpe_chn, index))
        return ret;

    return EXIT_SUCCESS;
}

void *i6_encoder_thread(void)
{
    int ret;
    int maxFd = 0;

    for (int i = 0; i < I6_VENC_CHN_NUM; i++) {
        if (!i6_state[i].enable) continue;
        if (!i6_state[i].mainLoop) continue;

        ret = i6_venc.fnGetDescriptor(i);
        if (ret < 0) return ret;
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

        timeout.tv_sec = 0;
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

                    if (venc_callback) {
                        hal_vidstream out;
                        out.count = stream.count;
                        out.pack = stream.packet;
                        out.seq = stream.sequence;
                        (*venc_callback)(i, &out);
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

int i6_pipeline_create(char sensor, short width, short height, char framerate, char hdr)
{
    int ret;

    snr_index = sensor;
    snr_profile = -1;
    snr_hdr = hdr;

    {
        unsigned int count;
        i6_snr_res resolution;
        if (ret = i6_snr.fnSetHDR(snr_index, hdr))
            return ret;

        if (ret = i6_snr.fnGetResolutionCount(snr_index, &count))
            return ret;
        for (char i = 0; i < count; i++) {
            if (ret = i6_snr.fnGetResolution(snr_index, i, &resolution))
                return ret;

            if (width > resolution.crop.width ||
                height > resolution.crop.height ||
                framerate > resolution.maxFps)
                continue;
        
            snr_profile = i;
            if (ret = i6_snr.fnSetResolution(snr_index, snr_profile))
                return ret;
            snr_framerate = framerate;
            if (ret = i6_snr.fnSetFramerate(snr_index, snr_framerate))
                return ret;
            break;
        }
        if (snr_profile < 0)
            return EXIT_FAILURE;

        if (ret = i6_snr.fnEnable(snr_index))
            return ret;
    }

    if (ret = i6_snr.fnGetPadInfo(snr_index, &snr_pad))
        return ret;
    if (ret = i6_snr.fnGetPlaneInfo(snr_index, hdr & 1, &snr_plane))
        return ret;

    {
        i6_vif_dev device;
        memset(&device, 0, sizeof(device));
        device.intf = snr_pad.intf;
        device.work = I6_VIF_WORK_1MULTIPLEX;
        device.hdr = snr_pad.hdr;
        if (device.intf == I6_INTF_MIPI) {
            device.edge = I6_EDGE_DOUBLE;
            device.input = snr_pad.intfAttr.mipi.input;
        } else if (device.intf == I6_INTF_BT656) {
            device.edge = snr_pad.intfAttr.bt656.edge;
            device.sync = snr_pad.intfAttr.bt656.sync;
        }
        if (ret = i6_vif.fnSetDeviceConfig(vif_dev, &device))
            return ret;
    }
    if (ret = i6_vif.fnEnableDevice(vif_dev))
        return ret;

    {
        i6_vif_port port;
        port.capt = snr_plane.capt;
        port.dest.height = snr_plane.capt.height;
        port.dest.width = snr_plane.capt.width;
        port.field = 0;
        port.interlaceOn = 0;
        port.pixFmt = (i6_common_pixfmt)(snr_plane.bayer > I6_BAYER_END ? 
            snr_plane.pixFmt : (I6_PIXFMT_RGB_BAYER + snr_plane.precision * I6_BAYER_END + snr_plane.bayer));
        port.frate = I6_VIF_FRATE_FULL;
        port.frameLineCnt = 0;
        if (ret = i6_vif.fnSetPortConfig(vif_chn, vif_port, &port))
            return ret;
    }
    if (ret = i6_vif.fnEnablePort(vif_chn, vif_port))
        return ret;

    {
        i6_vpe_chn channel;
        memset(&channel, 0, sizeof(channel));
        channel.capt.height = snr_plane.capt.height;
        channel.capt.width = snr_plane.capt.width;
        channel.pixFmt = (i6_common_pixfmt)(snr_plane.bayer > I6_BAYER_END ? 
            snr_plane.pixFmt : (I6_PIXFMT_RGB_BAYER + snr_plane.precision * I6_BAYER_END + snr_plane.bayer));
        channel.hdr = snr_pad.hdr;
        channel.sensor = (i6_vpe_sens)(snr_index + 1);
        channel.mode = I6_VPE_MODE_REALTIME;
        if (ret = i6_vpe.fnCreateChannel(vpe_chn, &channel))
            return ret;
    }

    {
        i6_vpe_para param;
        param.hdr = snr_pad.hdr;
        param.level3DNR = 0;
        param.mirror = 0;
        param.flip = 0;
        param.lensAdjOn = 0;
        if (ret = i6_vpe.fnSetChannelParam(vpe_chn, &param))
            return ret;
    }
    if (ret = i6_vpe.fnStartChannel(vpe_chn))
        return ret;

    {
        i6_sys_bind source = { .module = I6_SYS_MOD_VIF, 
            .device = vif_dev, .channel = vif_chn, .port = vif_port };
        i6_sys_bind dest = { .module = I6_SYS_MOD_VPE,
            .device = vpe_dev, .channel = vpe_chn, .port = vpe_port };
        return i6_sys.fnBindExt(&source, &dest, snr_framerate, snr_framerate,
            I6_SYS_LINK_REALTIME, 0);
    }

    return EXIT_SUCCESS;
}

void i6_pipeline_destroy(void)
{
    for (char i = 0; i < 4; i++)
        i6_vpe.fnDisablePort(vpe_chn, i);

    {
        i6_sys_bind source = { .module = I6_SYS_MOD_VIF, 
            .device = vif_dev, .channel = vif_chn, .port = vif_port };
        i6_sys_bind dest = { .module = I6_SYS_MOD_VPE,
            .device = vif_dev, .channel = vpe_chn, .port = vpe_port };
        i6_sys.fnUnbind(&source, &dest);
    }

    i6_vpe.fnStopChannel(vpe_chn);
    i6_vpe.fnDestroyChannel(vpe_chn);

    i6_vif.fnDisablePort(vif_chn, 0);
    i6_vif.fnDisableDevice(vif_dev);

    i6_snr.fnDisable(snr_index);
}

void i6_system_deinit(void)
{
    i6_sys.fnExit();
}

void i6_system_init(void)
{
    i6_sys.fnInit();

    i6_sys_ver version;
    i6_sys.fnGetVersion(&version);
    printf("App built with headers v%s\n", I6_SYS_API);
    printf("mi_sys version: %s\n", version.version);
}