#include "i6f_common.h"
#include "i6f_isp.h"
#include "i6f_rgn.h"
#include "i6f_scl.h"
#include "i6f_snr.h"
#include "i6f_sys.h"
#include "i6f_venc.h"
#include "i6f_vif.h"

i6f_isp_impl  i6f_isp;
i6f_scl_impl  i6f_scl;
i6f_snr_impl  i6f_snr;
i6f_sys_impl  i6f_sys;
i6f_venc_impl i6f_venc;
i6f_vif_impl  i6f_vif;

hal_chnstate i6f_state[I6F_VENC_CHN_NUM] = {0};

i6f_snr_pad _i6f_snr_pad;
i6f_snr_plane _i6f_snr_plane;
char snr_framerate, snr_hdr, snr_index, snr_profile;

char _i6f_isp_chn = 0;
char _i6f_isp_dev = 0;
char _i6f_isp_port = 0;
char _i6f_scl_chn = 0;
char _i6f_scl_dev = 0;
char _i6f_venc_chn = 0;
char _i6f_venc_port = 0;
char _i6f_vif_chn = 0;
char _i6f_vif_dev = 0;
char _i6f_vif_grp = 0;

int i6f_hal_init()
{
    int ret;

    if (ret = i6f_isp_load(&i6f_isp))
        return ret;
    if (ret = i6f_scl_load(&i6f_scl))
        return ret;
    if (ret = i6f_snr_load(&i6f_snr))
        return ret;
    if (ret = i6f_sys_load(&i6f_sys))
        return ret;
    if (ret = i6f_venc_load(&i6f_venc))
        return ret;
    if (ret = i6f_vif_load(&i6f_vif))
        return ret;

    return EXIT_SUCCESS;
}

void i6f_hal_deinit()
{
    i6f_vif_unload(&i6f_vif);
    i6f_venc_unload(&i6f_venc);
    i6f_sys_unload(&i6f_sys);
    i6f_snr_unload(&i6f_snr);
    i6f_scl_unload(&i6f_scl);
    i6f_isp_unload(&i6f_isp);
}

int i6f_channel_bind(char index, char framerate, char jpeg)
{
    int ret;

    if (ret = i6f_scl.fnEnablePort(_i6f_scl_dev, _i6f_scl_chn, index))
        return ret;

    {
        i6f_sys_bind source = { .module = I6F_SYS_MOD_SCL, 
            .device = _i6f_scl_dev, .channel = _i6f_scl_chn, .port = index };
        i6f_sys_bind dest = { .module = I6F_SYS_MOD_VENC,
            .device = jpeg ? 8 : 0, .channel = _i6f_venc_chn, .port = _i6f_venc_port };
        if (ret = i6f_sys.fnBindExt(0, &source, &dest, framerate, framerate,
            jpeg ? I6F_SYS_LINK_REALTIME : I6F_SYS_LINK_RING, 0))
            return ret;
    }

    return EXIT_SUCCESS;
}

int i6f_channel_create(char index, short width, short height, char jpeg)
{
    i6f_scl_port port;
    port.crop.x = 0;
    port.crop.y = 0;
    port.crop.width = 0;
    port.crop.height = 0;
    port.output.width = width;
    port.output.height = height;
    port.mirror = 0;
    port.flip = 0;
    port.compress = I6F_COMPR_NONE;
    port.pixFmt = jpeg ? I6F_PIXFMT_YUV422_YUYV : I6F_PIXFMT_YUV420SP;

    return i6f_scl.fnSetPortConfig(_i6f_scl_dev, _i6f_scl_chn, index, &port);
}

int i6f_channel_grayscale(int index, char enable)
{
    return i6f_isp.fnSetColorToGray(_i6f_isp_dev, index, &enable);
}

int i6f_channel_unbind(char index, char jpeg)
{
    int ret;

    if (ret = i6f_scl.fnDisablePort(_i6f_scl_dev, _i6f_scl_chn, index))
        return ret;

    {
        i6f_sys_bind source = { .module = I6F_SYS_MOD_SCL, 
            .device = _i6f_scl_dev, .channel = _i6f_scl_chn, .port = index };
        i6f_sys_bind dest = { .module = I6F_SYS_MOD_VENC,
            .device = jpeg ? 8 : 0, .channel = _i6f_venc_chn, .port = _i6f_venc_port };
        if (ret = i6f_sys.fnUnbind(0, &source, &dest))
            return ret;
    }

    return EXIT_SUCCESS;    
}

int i6f_config_load(char *path)
{
    return i6f_isp.fnLoadChannelConfig(_i6f_isp_dev, _i6f_isp_chn, path, 1234);
}

int i6f_encoder_create(char index, hal_vidconfig *config)
{
    int ret;
    char device = I6F_VENC_DEV_H26X_0;
    i6f_venc_chn channel;
    i6f_venc_attr_h26x *attrib;
    
    if (config->codec == HAL_VIDCODEC_JPG || config->codec == HAL_VIDCODEC_MJPG) {
        device = I6F_VENC_DEV_MJPG_0;
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
                channel.rate.mode = I6F_VENC_RATEMODE_H265CBR;
                channel.rate.h265Cbr = (i6f_venc_rate_h26xcbr){ .gop = config->gop,
                    .statTime = 0, .fpsNum = config->framerate, .fpsDen = 1, .bitrate = 
                    (unsigned int)(config->bitrate << 10), .avgLvl = 0 }; break;
            case HAL_VIDMODE_VBR:
                channel.rate.mode = I6F_VENC_RATEMODE_H265VBR;
                channel.rate.h265Vbr = (i6f_venc_rate_h26xvbr){ .gop = config->gop,
                    .statTime = 0, .fpsNum = config->framerate, .fpsDen = 1, .maxBitrate = 
                    (unsigned int)(MAX(config->bitrate, config->maxBitrate) << 10),
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
                    .statTime = 0, .fpsNum = config->framerate, .fpsDen = 1, .maxBitrate = 
                    (unsigned int)(MAX(config->bitrate, config->maxBitrate) << 10),
                    .maxQual = config->maxQual, .minQual = config->minQual }; break;
            default:
                I6F_ERROR("H.265 encoder does not support this mode!");
        }  
    } else if (config->codec == HAL_VIDCODEC_H264) {
        attrib = &channel.attrib.h264;
        switch (config->mode) {
            case HAL_VIDMODE_CBR:
                channel.rate.mode = I6F_VENC_RATEMODE_H264CBR;
                channel.rate.h264Cbr = (i6f_venc_rate_h26xcbr){ .gop = config->gop,
                    .statTime = 0, .fpsNum = config->framerate, .fpsDen = 1, .bitrate = 
                    (unsigned int)(config->bitrate << 10), .avgLvl = 0 }; break;
            case HAL_VIDMODE_VBR:
                channel.rate.mode = I6F_VENC_RATEMODE_H264VBR;
                channel.rate.h264Vbr = (i6f_venc_rate_h26xvbr){ .gop = config->gop,
                    .statTime = 0, .fpsNum = config->framerate, .fpsDen = 1, .maxBitrate = 
                    (unsigned int)(MAX(config->bitrate, config->maxBitrate) << 10),
                    .maxQual = config->maxQual, .minQual = config->minQual }; break;
            case HAL_VIDMODE_QP:
                channel.rate.mode = I6F_VENC_RATEMODE_H264QP;
                channel.rate.h264Qp = (i6f_venc_rate_h26xqp){ .gop = config->gop,
                    .fpsNum = config->framerate, .fpsDen = 1, .interQual = config->maxQual,
                    .predQual = config->minQual }; break;
            case HAL_VIDMODE_ABR:
                channel.rate.mode = I6F_VENC_RATEMODE_H264ABR;
                channel.rate.h264Abr = (i6f_venc_rate_h26xabr){ .gop = config->gop,
                    .statTime = 0, .fpsNum = config->framerate, .fpsDen = 1,
                    .avgBitrate = (unsigned int)(config->bitrate << 10),
                    .maxBitrate = (unsigned int)(config->maxBitrate << 10) }; break;
            case HAL_VIDMODE_AVBR:
                channel.rate.mode = I6F_VENC_RATEMODE_H265AVBR;
                channel.rate.h265Avbr = (i6f_venc_rate_h26xvbr){ .gop = config->gop,
                    .statTime = 0, .fpsNum = config->framerate, .fpsDen = 1, .maxBitrate = 
                    (unsigned int)(MAX(config->bitrate, config->maxBitrate) << 10),
                    .maxQual = config->maxQual, .minQual = config->minQual }; break;
            default:
                I6F_ERROR("H.264 encoder does not support this mode!");
        }
    } else I6F_ERROR("This codec is not supported by the hardware!");
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
    if (ret = i6f_venc.fnCreateChannel(device, index, &channel))
        return ret;

    if (config->codec != HAL_VIDCODEC_JPG && 
        (ret = i6f_venc.fnStartReceiving(device, index)))
        return ret;

    i6f_state[index].payload = config->codec;

    return EXIT_SUCCESS;
}

int i6f_encoder_destroy(char index, char jpeg)
{
    int ret;
    char device = jpeg ? I6F_VENC_DEV_MJPG_0 : I6F_VENC_DEV_H26X_0;

    i6f_state[index].payload = HAL_VIDCODEC_UNSPEC;

    if (ret = i6f_venc.fnStopReceiving(device, index))
        return ret;

    {
        i6f_sys_bind source = { .module = I6F_SYS_MOD_SCL, 
            .device = _i6f_scl_dev, .channel = _i6f_scl_chn, .port = index };
        i6f_sys_bind dest = { .module = I6F_SYS_MOD_VENC,
            .device = device, .channel = index, .port = _i6f_venc_port };
        if (ret = i6f_sys.fnUnbind(0, &source, &dest))
            return ret;
    }

    if (ret = i6f_venc.fnDestroyChannel(device, index))
        return ret;
    
    if (ret = i6f_scl.fnDisablePort(_i6f_scl_dev, _i6f_scl_chn, index))
        return ret;
    
    return EXIT_SUCCESS;
}

int i6f_encoder_destroy_all(void)
{
    int ret;

    for (char i = 0; i < I6F_VENC_CHN_NUM; i++)
        if (i6f_state[i].enable)
            if (ret = i6f_encoder_destroy(i, 
                i6f_state[i].payload == HAL_VIDCODEC_JPG || 
                i6f_state[i].payload == HAL_VIDCODEC_MJPG))
                return ret;
    
    return EXIT_SUCCESS;
}

int i6f_encoder_snapshot_grab(char index, short width, short height, char quality, char grayscale, hal_vidstream *stream)
{
    int ret;
    char device = 
        (i6f_state[index].payload == HAL_VIDCODEC_JPG ||
         i6f_state[index].payload == HAL_VIDCODEC_MJPG) ? 
         I6F_VENC_DEV_MJPG_0 : I6F_VENC_DEV_H26X_0;

    if (ret = i6f_channel_bind(index, 1, 1)) {
        fprintf(stderr, "[i6f_venc] Binding the encoder channel "
            "%d failed with %#x!\n", index, ret);
        goto abort;
    }
    return ret;

    i6f_venc_jpg param;
    memset(&param, 0, sizeof(param));
    if (ret = i6f_venc.fnGetJpegParam(device, index, &param)) {
        fprintf(stderr, "[i6f_venc] Reading the JPEG settings "
            "%d failed with %#x!\n", index, ret);
        goto abort;
    }
    return ret;
        return ret;
    param.quality = quality;
    if (ret = i6f_venc.fnSetJpegParam(device, index, &param)) {
        fprintf(stderr, "[i6f_venc] Writing the JPEG settings "
            "%d failed with %#x!\n", index, ret);
        goto abort;
    }

    i6f_channel_grayscale(index, grayscale);

    unsigned int count = 1;
    if (i6f_venc.fnStartReceivingEx(device, index, &count)) {
        fprintf(stderr, "[i6f_venc] Requesting one frame "
            "%d failed with %#x!\n", index, ret);
        goto abort;
    }

    int fd = i6f_venc.fnGetDescriptor(device, index);

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
        if (i6f_venc.fnQuery(device, index, &stat)) {
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

        if (ret = i6f_venc.fnGetStream(device, index, &stream, stat.curPacks)) {
            fprintf(stderr, "[i6f_venc] Getting the stream on "
                "channel %d failed with %#x!\n", index, ret);
            free(strm.packet);
            strm.packet = NULL;
            goto abort;
        }

        stream->count = stat.curPacks;
        stream->pack = strm.packet;
abort:
        if (ret = i6f_venc.fnFreeStream(device, index, &stream)) {
            fprintf(stderr, "[i6f_venc] Releasing the stream on "
                "channel %d failed with %#x!\n", index, ret);
        }
    }

    if (i6f_venc.fnFreeDescriptor(device, index)) {
        fprintf(stderr, "[i6f_venc] Releasing the stream on "
            "channel %d failed with %#x!\n", index, ret);
    }

    i6f_venc.fnStopReceiving(device, index);

    i6f_channel_unbind(device, index);

    return EXIT_SUCCESS;    
}

void *i6f_encoder_thread(void)
{
    int ret;
    int maxFd = 0;

    for (int i = 0; i < I6F_VENC_CHN_NUM; i++) {
        if (!i6f_state[i].enable) continue;
        if (!i6f_state[i].mainLoop) continue;
        char device = 
            (i6f_state[i].payload == HAL_VIDCODEC_JPG ||
             i6f_state[i].payload == HAL_VIDCODEC_MJPG) ? 
             I6F_VENC_DEV_MJPG_0 : I6F_VENC_DEV_H26X_0;

        ret = i6f_venc.fnGetDescriptor(device, i);
        if (ret < 0) return ret;
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

        timeout.tv_sec = 0;
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
                    char device = 
                        (i6f_state[i].payload == HAL_VIDCODEC_JPG ||
                         i6f_state[i].payload == HAL_VIDCODEC_MJPG) ? 
                         I6F_VENC_DEV_MJPG_0 : I6F_VENC_DEV_H26X_0;

                    memset(&stream, 0, sizeof(stream));
                    
                    if (ret = i6f_venc.fnQuery(device, i, &stat)) {
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

                    if (ret = i6f_venc.fnGetStream(device, i, &stream, stat.curPacks)) {
                        fprintf(stderr, "[i6f_venc] Getting the stream on "
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

                    if (ret = i6f_venc.fnFreeStream(device, i, &stream)) {
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

int i6f_pipeline_create(char sensor, short width, short height, char framerate, char hdr)
{
    int ret;

    snr_index = sensor;
    snr_profile = -1;
    snr_hdr = hdr;

    {
        unsigned int count;
        i6f_snr_res resolution;
        if (ret = i6f_snr.fnSetHDR(snr_index, hdr))
            return ret;

        if (ret = i6f_snr.fnGetResolutionCount(snr_index, &count))
            return ret;
        for (char i = 0; i < count; i++) {
            if (ret = i6f_snr.fnGetResolution(snr_index, i, &resolution))
                return ret;

            if (width > resolution.crop.width ||
                height > resolution.crop.height ||
                framerate > resolution.maxFps)
                continue;
        
            snr_profile = i;
            if (ret = i6f_snr.fnSetResolution(snr_index, snr_profile))
                return ret;
            snr_framerate = framerate;
            if (ret = i6f_snr.fnSetFramerate(snr_index, snr_framerate))
                return ret;
            break;
        }
        if (snr_profile < 0)
            return EXIT_FAILURE;

        if (ret = i6f_snr.fnEnable(snr_index))
            return ret;
    }

    if (ret = i6f_snr.fnGetPadInfo(snr_index, &_i6f_snr_pad))
        return ret;
    if (ret = i6f_snr.fnGetPlaneInfo(snr_index, hdr & 1, &_i6f_snr_plane))
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
        unsigned int combo = 0;
        if (ret = i6f_isp.fnCreateDevice(_i6f_isp_dev, &combo))
            return ret;
    }

    {
        i6f_isp_chn channel;
        memset(&_i6f_isp_chn, 0, sizeof(_i6f_isp_chn));
        channel.sensorId = snr_index;
        if (ret = i6f_isp.fnCreateChannel(_i6f_isp_dev, _i6f_isp_chn, &channel))
            return ret;
    }

    {
        i6f_isp_para param;
        param.hdr = _i6f_snr_pad.hdr;
        param.level3DNR = 0;
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
        if (ret = i6f_sys.fnBindExt(0, &source, &dest, snr_framerate, snr_framerate,
            I6F_SYS_LINK_REALTIME, 0))
            return ret;
    }

    {
        i6f_sys_bind source = { .module = I6F_SYS_MOD_ISP, 
            .device = _i6f_isp_dev, .channel = _i6f_isp_chn, .port = _i6f_isp_port };
        i6f_sys_bind dest = { .module = I6F_SYS_MOD_SCL,
            .device = _i6f_scl_dev, .channel = _i6f_scl_chn, .port = 0 };
        return i6f_sys.fnBindExt(0, &source, &dest, snr_framerate, snr_framerate,
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

    i6f_snr.fnDisable(snr_index);
}

void i6f_system_deinit(void)
{
    i6f_sys.fnExit(0);
}

int i6d_system_init(void)
{
    int ret;

    if (ret = i6f_sys.fnInit(0))
        return ret;

    {
        i6f_sys_ver version;
        if (ret = i6f_sys.fnGetVersion(&version))
            return ret;
        printf("App built with headers v%s\n", I6F_SYS_API);
        printf("mi_sys version: %s\n", version.version);
    }

    return EXIT_SUCCESS;
}