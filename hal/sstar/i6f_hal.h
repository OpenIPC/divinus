#include "i6f_common.h"

i6f_isp_impl  i6f_isp;
i6f_scl_impl  i6f_scl;
i6f_snr_impl  i6f_snr;
i6f_sys_impl  i6f_sys;
i6f_venc_impl i6f_venc;
i6f_vif_impl  i6f_vif;

i6f_snr_pad snr_pad;
i6f_snr_plane snr_plane;
char snr_framerate, snr_hdr, snr_index, snr_profile;

char isp_chn = 0;
char isp_dev = 0;
char isp_port = 0;
char scl_chn = 0;
char scl_dev = 0;
char venc_chn = 0;
int venc_fd[I6F_VENC_CHN_NUM] = {0};
char venc_port = 0;
char vif_chn = 0;
char vif_dev = 0;
char vif_grp = 0;

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

    if (ret = i6f_scl.fnEnablePort(scl_dev, scl_chn, index))
        return ret;

    {
        i6f_sys_bind source = { .module = I6F_SYS_MOD_SCL, 
            .device = scl_dev, .channel = scl_chn, .port = index };
        i6f_sys_bind dest = { .module = I6F_SYS_MOD_VENC,
            .device = jpeg ? 8 : 0, .channel = venc_chn, .port = venc_port };
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

    return i6f_scl.fnSetPortConfig(scl_dev, scl_chn, index, &port);
}

int i6f_channel_grayscale(int index, char enable)
{
    return i6f_isp.fnSetColorToGray(isp_dev, index, &enable);
}

int i6f_channel_unbind(char index, char jpeg)
{
    int ret;

    if (ret = i6f_scl.fnDisablePort(scl_dev, scl_chn, index))
        return ret;

    {
        i6f_sys_bind source = { .module = I6F_SYS_MOD_SCL, 
            .device = scl_dev, .channel = scl_chn, .port = index };
        i6f_sys_bind dest = { .module = I6F_SYS_MOD_VENC,
            .device = jpeg ? 8 : 0, .channel = venc_chn, .port = venc_port };
        if (ret = i6f_sys.fnUnbind(0, &source, &dest))
            return ret;
    }

    return EXIT_SUCCESS;    
}

int i6f_config_load(int index, char *path)
{
    return i6f_isp.fnLoadChannelConfig(isp_dev, index, path, 1234);
}

int i6c_encoder_create(char index, hal_vidconfig config)
{
    int ret;
    char device = I6F_VENC_DEV_H26X_0;
    i6f_venc_chn channel;
    i6f_venc_attr_h26x *attrib;
    
    if (config.codec == HAL_VIDCODEC_JPG || config.codec == HAL_VIDCODEC_MJPG) {
        device = I6F_VENC_DEV_MJPG_0;
        channel.attrib.codec = I6F_VENC_CODEC_MJPG;
        switch (config.mode) {
            case HAL_VIDMODE_CBR:
                channel.rate.mode = I6F_VENC_RATEMODE_MJPGCBR;
                channel.rate.mjpgCbr.bitrate = config.bitrate << 10;
                channel.rate.mjpgCbr.fpsNum = 
                    config.codec == HAL_VIDCODEC_JPG ? 1 : config.framerate;
                channel.rate.mjpgCbr.fpsDen = 1;
                break;
            case HAL_VIDMODE_QP:
                channel.rate.mode = I6F_VENC_RATEMODE_MJPGQP;
                channel.rate.mjpgQp.fpsNum = config.framerate;
                channel.rate.mjpgQp.fpsDen = 
                    config.codec == HAL_VIDCODEC_JPG ? 1 : config.framerate;
                channel.rate.mjpgQp.quality = MAX(config.minQual, config.maxQual);
                break;
            default:
                I6F_ERROR("MJPEG encoder can only support CBR or fixed QP modes!");
        }
        channel.attrib.mjpg.maxHeight = ALIGN_BACK(config.height, 16);
        channel.attrib.mjpg.maxWidth = ALIGN_BACK(config.width, 16);
        channel.attrib.mjpg.bufSize = config.width * config.height;
        channel.attrib.mjpg.byFrame = 1;
        channel.attrib.mjpg.height = ALIGN_BACK(config.height, 16);
        channel.attrib.mjpg.width = ALIGN_BACK(config.width, 16);
        channel.attrib.mjpg.dcfThumbs = 0;
        channel.attrib.mjpg.markPerRow = 0;

        goto attach;
    } else if (config.codec == HAL_VIDCODEC_H265) {
        attrib = &channel.attrib.h265;
        switch (config.mode) {
            case HAL_VIDMODE_CBR:
                channel.rate.mode = I6F_VENC_RATEMODE_H265CBR;
                channel.rate.h265Cbr = { .gop = config.gop, .statTime = 0,
                    .fpsNum = config.framerate, .fpsDen = 1, .bitrate = 
                    (unsigned int)(config.bitrate << 10), .avgLvl = 0 }; break;
            case HAL_VIDMODE_VBR:
                channel.rate.mode = I6F_VENC_RATEMODE_H265VBR;
                channel.rate.h265Vbr = { .gop = config.gop, .statTime = 0,
                    .fpsNum = config.framerate, .fpsDen = 1,
                    .maxBitrate = (unsigned int)(config.bitrate << 10),
                    .maxQual = config.maxQual, .minQual = config.minQual }; break;
            case HAL_VIDMODE_QP:
                channel.rate.mode = I6F_VENC_RATEMODE_H265QP;
                channel.rate.h265Qp = { .gop = config.gop, .fpsNum = 
                    config.framerate, .fpsDen = 1, .interQual = config.maxQual,
                    .predQual = config.minQual }; break;
            case HAL_VIDMODE_ABR:
                I6F_ERROR("H.265 encoder does not support ABR mode!");
            case HAL_VIDMODE_AVBR:
                channel.rate.mode = I6F_VENC_RATEMODE_H265AVBR;
                channel.rate.h265Avbr = { .gop = config.gop, .statTime = 0,
                    .fpsNum = config.framerate, .fpsDen = 1,
                    .maxBitrate = (unsigned int)(config.bitrate << 10),
                    .maxQual = config.maxQual, .minQual = config.minQual }; break;
            default:
                I6F_ERROR("H.265 encoder does not support this mode!");
        }  
    } else if (config.codec == HAL_VIDCODEC_H264) {
        attrib = &channel.attrib.h264;
        switch (config.mode) {
            case HAL_VIDMODE_CBR:
                channel.rate.mode = I6F_VENC_RATEMODE_H264CBR;
                channel.rate.h264Cbr = { .gop = config.gop, .statTime = 0,
                    .fpsNum = config.framerate, .fpsDen = 1, .bitrate = 
                    (unsigned int)(config.bitrate << 10), .avgLvl = 0 }; break;
            case HAL_VIDMODE_VBR:
                channel.rate.mode = I6F_VENC_RATEMODE_H264VBR;
                channel.rate.h264Vbr = { .gop = config.gop, .statTime = 0,
                    .fpsNum = config.framerate, .fpsDen = 1,
                    .maxBitrate = (unsigned int)(config.bitrate << 10),
                    .maxQual = config.maxQual, .minQual = config.minQual }; break;
            case HAL_VIDMODE_QP:
                channel.rate.mode = I6F_VENC_RATEMODE_H264QP;
                channel.rate.h264Qp = { .gop = config.gop, .fpsNum = 
                    config.framerate, .fpsDen = 1, .interQual = config.maxQual,
                    .predQual = config.minQual }; break;
            case HAL_VIDMODE_ABR:
                channel.rate.mode = I6F_VENC_RATEMODE_H264ABR;
                channel.rate.h264Abr = { .gop = config.gop, .statTime = 0,
                    .fpsNum = config.framerate, .fpsDen = 1,
                    .avgBitrate = (unsigned int)(config.bitrate << 10),
                    .maxBitrate = (unsigned int)(config.maxBitrate << 10) }; break;
            case HAL_VIDMODE_AVBR:
                channel.rate.mode = I6F_VENC_RATEMODE_H265AVBR;
                channel.rate.h265Avbr = { .gop = config.gop, .statTime = 0,
                    .fpsNum = config.framerate, .fpsDen = 1, .maxBitrate = 
                    (unsigned int)(MAX(config.bitrate, config.maxBitrate) << 10),
                    .maxQual = config.maxQual, .minQual = config.minQual }; break;
            default:
                I6F_ERROR("H.264 encoder does not support this mode!");
        }
    } else I6F_ERROR("This codec is not supported by the hardware!");
    attrib->maxHeight = ALIGN_BACK(config.height, 16);
    attrib->maxWidth = ALIGN_BACK(config.width, 16);
    attrib->bufSize = config.height * config.width;
    attrib->profile = config.profile;
    attrib->byFrame = 1;
    attrib->height = ALIGN_BACK(config.height, 16);
    attrib->width = ALIGN_BACK(config.width, 16);
    attrib->bFrameNum = 0;
    attrib->refNum = 1;
attach:
    if (ret = i6f_venc.fnCreateChannel(device, index, &channel))
        return ret;

    if (config.codec != HAL_VIDCODEC_JPG && 
        (ret = i6f_venc.fnStartReceiving(device, index)))
        return ret;

    return EXIT_SUCCESS;
}

int i6f_encoder_destroy(char index, char jpeg)
{
    int ret;
    char device = jpeg ? I6F_VENC_DEV_MJPG_0 : I6F_VENC_DEV_H26X_0;

    if (ret = i6f_venc.fnStopReceiving(device, index))
        return ret;

    {
        i6f_sys_bind source = { .module = I6F_SYS_MOD_SCL, 
            .device = scl_dev, .channel = scl_chn, .port = index };
        i6f_sys_bind dest = { .module = I6F_SYS_MOD_VENC,
            .device = device, .channel = index, .port = venc_port };
        if (ret = i6f_sys.fnUnbind(0, &source, &dest))
            return ret;
    }

    if (ret = i6f_venc.fnDestroyChannel(device, index))
        return ret;
    
    if (ret = i6f_scl.fnDisablePort(scl_dev, scl_chn, index))
        return ret;
    
    return EXIT_SUCCESS;
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

    if (ret = i6f_snr.fnGetPadInfo(snr_index, &snr_pad))
        return ret;
    if (ret = i6f_snr.fnGetPlaneInfo(snr_index, hdr & 1, &snr_plane))
        return ret;

    {
        i6f_vif_grp group;
        group.intf = snr_pad.intf;
        group.work = I6F_VIF_WORK_1MULTIPLEX;
        group.hdr = I6F_HDR_OFF;
        group.edge = group.intf == I6F_INTF_BT656 ?
            snr_pad.intfAttr.bt656.edge : I6F_EDGE_DOUBLE;
        group.interlaceOn = 0;
        group.grpStitch = (1 << vif_grp);
        if (ret = i6f_vif.fnCreateGroup(vif_grp, &group))
            return ret;
    }
    
    {
        i6f_vif_dev device;
        device.pixFmt = (i6f_common_pixfmt)(snr_plane.bayer > I6F_BAYER_END ? 
            snr_plane.pixFmt : (I6F_PIXFMT_RGB_BAYER + snr_plane.precision * I6F_BAYER_END + snr_plane.bayer));
        device.crop = snr_plane.capt;
        device.field = 0;
        device.halfHScan = 0;
        if (ret = i6f_vif.fnSetDeviceConfig(vif_dev, &device))
            return ret;
    }
    if (ret = i6f_vif.fnEnableDevice(vif_dev))
        return ret;

    {
        unsigned int combo = 0;
        if (ret = i6f_isp.fnCreateDevice(isp_dev, &combo))
            return ret;
    }

    {
        i6f_isp_chn channel;
        memset(&isp_chn, 0, sizeof(isp_chn));
        channel.sensorId = snr_index;
        if (ret = i6f_isp.fnCreateChannel(isp_dev, isp_chn, &channel))
            return ret;
    }

    {
        i6f_isp_para param;
        param.hdr = snr_pad.hdr;
        param.level3DNR = 0;
        param.mirror = 0;
        param.flip = 0;
        param.rotate = 0;
        if (ret = i6f_isp.fnSetChannelParam(isp_dev, isp_chn, &param))
            return ret;
    }
    if (ret = i6f_isp.fnStartChannel(isp_dev, isp_chn))
        return ret;

    {
        i6f_isp_port port;
        memset(&port, 0, sizeof(port));
        port.pixFmt = I6F_PIXFMT_YUV422_YUYV;
        if (ret = i6f_isp.fnSetPortConfig(isp_dev, isp_chn, isp_port, &port))
            return ret;
    }
    if (ret = i6f_isp.fnEnablePort(isp_dev, isp_chn, isp_port))
        return ret;

    {
        unsigned int binds = (1 << 0) | (1 << 1) | (1 << 2) | (1 << 3);
        if (ret = i6f_scl.fnCreateDevice(scl_dev, &binds))
            return ret;
    }

    {
        unsigned int reserved = 0;
        if (ret = i6f_scl.fnCreateChannel(scl_dev, scl_chn, &reserved))
            return ret;
    }
    {
        int rotate = 0;
        if (ret = i6f_scl.fnAdjustChannelRotation(scl_dev, scl_chn, &rotate))
            return ret;
    }
    if (ret = i6f_scl.fnStartChannel(scl_dev, scl_chn))
        return ret;

    {
        i6f_sys_bind source = { .module = I6F_SYS_MOD_VIF, 
            .device = vif_dev, .channel = vif_chn, .port = 0 };
        i6f_sys_bind dest = { .module = I6F_SYS_MOD_ISP,
            .device = isp_dev, .channel = isp_chn, .port = isp_port };
        if (ret = i6f_sys.fnBindExt(0, &source, &dest, snr_framerate, snr_framerate,
            I6F_SYS_LINK_REALTIME, 0))
            return ret;
    }

    {
        i6f_sys_bind source = { .module = I6F_SYS_MOD_ISP, 
            .device = isp_dev, .channel = isp_chn, .port = isp_port };
        i6f_sys_bind dest = { .module = I6F_SYS_MOD_SCL,
            .device = scl_dev, .channel = scl_chn, .port = 0 };
        return i6f_sys.fnBindExt(0, &source, &dest, snr_framerate, snr_framerate,
            I6F_SYS_LINK_REALTIME, 0);
    }

    return EXIT_SUCCESS;
}

void i6f_pipeline_destroy(void)
{
    for (char i = 0; i < 4; i++)
        i6f_scl.fnDisablePort(scl_dev, scl_chn, i);

    {
        i6f_sys_bind source = { .module = I6F_SYS_MOD_ISP, 
            .device = isp_dev, .channel = isp_chn, .port = isp_port };
        i6f_sys_bind dest = { .module = I6F_SYS_MOD_SCL,
            .device = scl_dev, .channel = scl_chn, .port = 0 };
        i6f_sys.fnUnbind(0, &source, &dest);
    }

    i6f_scl.fnStopChannel(scl_dev, scl_chn);
    i6f_scl.fnDestroyChannel(scl_dev, scl_chn);

    i6f_scl.fnDestroyDevice(scl_dev);

    i6f_isp.fnStopChannel(isp_dev, isp_chn);
    i6f_isp.fnDestroyChannel(isp_dev, isp_chn);

    i6f_isp.fnDestroyDevice(isp_dev);

    {   
        i6f_sys_bind source = { .module = I6F_SYS_MOD_VIF, 
            .device = vif_dev, .channel = vif_chn, .port = 0 };
        i6f_sys_bind dest = { .module = I6F_SYS_MOD_ISP,
            .device = isp_dev, .channel = isp_chn, .port = isp_port };
        i6f_sys.fnUnbind(0, &source, &dest);
    }

    i6f_vif.fnDisablePort(vif_dev, 0);

    i6f_vif.fnDisableDevice(vif_dev);

    i6f_snr.fnDisable(snr_index);
}

void i6f_system_deinit(void)
{
    i6f_sys.fnExit(0);
}

void i6f_system_init(void)
{
    i6f_sys.fnInit(0);

    i6f_sys_ver version;
    i6f_sys.fnGetVersion(&version);
    printf("App built with headers v%s\n", I6F_SYS_API);
    printf("mi_sys version: %s\n", version.version);
}