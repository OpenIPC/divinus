#include "v3_common.h"

#include <fcntl.h>

typedef struct {
    void* handle;
    int (*fnRegister)(void);
    int (*fnUnregister)(void);
} v3_drv_impl;

v3_drv_impl v3_drv;
v3_isp_impl v3_isp;
v3_sys_impl v3_sys;
v3_vb_impl v3_vb;
v3_venc_impl v3_venc;
v3_vi_impl v3_vi;
v3_vpss_impl v3_vpss;

hal_chnstate v3_state[V3_VENC_CHN_NUM] = {0};
extern bool keepRunning;

int (*venc_callback)(char, hal_vidstream*);
char isp_dev = 0;
char venc_dev = 0;
char vi_chn = 0;
char vi_dev = 0;
char vpss_grp = 0;

int v3_hal_init()
{
    int ret;

    if (ret = v3_isp_load(&v3_isp))
        return ret;
    if (ret = v3_sys_load(&v3_sys))
        return ret;
    if (ret = v3_vb_load(&v3_vb))
        return ret;
    if (ret = v3_venc_load(&v3_venc))
        return ret;
    if (ret = v3_vi_load(&v3_vi))
        return ret;
    if (ret = v3_vpss_load(&v3_vpss))
        return ret;

    return EXIT_SUCCESS;
}

void v3_hal_deinit()
{
    v3_vpss_unload(&v3_vpss);
    v3_vi_unload(&v3_vi);
    v3_venc_unload(&v3_venc);
    v3_vb_unload(&v3_vb);
    v3_sys_unload(&v3_sys);
    v3_isp_unload(&v3_isp);
}

int v3_channel_bind(char index)
{
    int ret;
    int vpss_grp = index / V3_VPSS_CHN_NUM;
    int vpss_chn = index - vpss_grp * V3_VPSS_CHN_NUM;

    if (ret = v3_vpss.fnEnableChannel(vpss_grp, vpss_chn))
        return ret;

    {
        v3_sys_bind source = { .module = V3_SYS_MOD_VPSS, 
            .device = vpss_grp, .channel = vpss_chn };
        v3_sys_bind dest = { .module = V3_SYS_MOD_VENC,
            .device = venc_dev, .channel = index };
        if (ret = v3_sys.fnBind(&source, &dest))
            return ret;
    }

    return EXIT_SUCCESS;
}

int v3_channel_create(char index, short width, short height, char framerate)
{
    int ret;
    int vpss_grp = index / V3_VPSS_CHN_NUM;
    int vpss_chn = index - vpss_grp * V3_VPSS_CHN_NUM;

    {
        v3_vpss_chn channel;
        memset(&channel, 0, sizeof(channel));
        channel.srcFps = framerate;
        channel.dstFps = framerate;
        if (ret = v3_vpss.fnSetChannelConfig(vpss_grp, vpss_chn, &channel))
            return ret;
    }

    {
        v3_vpss_mode mode;
        mode.userModeOn = 1;
        mode.dest.height = height;
        mode.dest.width = width;
        mode.twoFldFrm = 0;
        mode.pixFmt = V3_PIXFMT_YUV420SP;
        mode.compress = V3_COMPR_NONE;
        if (ret = v3_vpss.fnSetChannelMode(vpss_grp, vpss_chn, &mode))
            return ret;
    }

    return EXIT_SUCCESS;
}

int v3_channel_grayscale(char index, int enable)
{
    return v3_venc.fnSetColorToGray(index, &enable);
}

int v3_channel_unbind(char index)
{
    int ret;
    int vpss_grp = index / V3_VPSS_CHN_NUM;
    int vpss_chn = index - vpss_grp * V3_VPSS_CHN_NUM;

    if (ret = v3_vpss.fnDisableChannel(vpss_grp, vpss_chn))
        return ret;

    {
        v3_sys_bind source = { .module = V3_SYS_MOD_VPSS, 
            .device = vpss_grp, .channel = vpss_chn };
        v3_sys_bind dest = { .module = V3_SYS_MOD_VENC,
            .device = venc_dev, .channel = index };
        if (ret = v3_sys.fnUnbind(&source, &dest))
            return ret;
    }

    return EXIT_SUCCESS;
}

int v3_encoder_create(char index, hal_vidconfig *config)
{
    int ret;
    v3_venc_chn channel;
    v3_venc_attr_h26x *attrib;

    if (config->codec == HAL_VIDCODEC_JPG) {
        channel.attrib.codec = V3_VENC_CODEC_JPEGE;
        channel.attrib.jpg.maxWidth = ALIGN_BACK(config->width, 16);
        channel.attrib.jpg.maxHeight = ALIGN_BACK(config->height, 16);
        channel.attrib.jpg.bufSize = 
            ALIGN_BACK(config->height, 16) * ALIGN_BACK(config->width, 16);
        channel.attrib.jpg.byFrame = 1;
        channel.attrib.jpg.width = config->width;
        channel.attrib.jpg.height = config->height;
        channel.attrib.jpg.dcfThumbs = 0;
    } else if (config->codec == HAL_VIDCODEC_MJPG) {
        channel.attrib.codec = V3_VENC_CODEC_MJPG;
        channel.attrib.mjpg.maxWidth = ALIGN_BACK(config->width, 16);
        channel.attrib.mjpg.maxHeight = ALIGN_BACK(config->height, 16);
        channel.attrib.mjpg.bufSize = 
            ALIGN_BACK(config->height, 16) * ALIGN_BACK(config->width, 16);
        channel.attrib.mjpg.byFrame = 1;
        channel.attrib.mjpg.width = config->width;
        channel.attrib.mjpg.height = config->height;
        switch (config->mode) {
            case HAL_VIDMODE_CBR:
                channel.rate.mode = V3_VENC_RATEMODE_MJPGCBR;
                channel.rate.mjpgCbr = (v3_venc_rate_mjpgcbr){ .statTime = 1, .srcFps = config->framerate,
                    .dstFps = config->framerate, .bitrate = config->bitrate, .avgLvl = 0 }; break;
            case HAL_VIDMODE_VBR:
                channel.rate.mode = V3_VENC_RATEMODE_MJPGVBR;
                channel.rate.mjpgVbr = (v3_venc_rate_mjpgvbr){ .statTime = 1, .srcFps = config->framerate,
                    .dstFps = config->framerate , .maxBitrate = MAX(config->bitrate, config->maxBitrate), 
                    .maxQual = config->maxQual, .minQual = config->maxQual }; break;
            case HAL_VIDMODE_QP:
                channel.rate.mode = V3_VENC_RATEMODE_MJPGQP;
                channel.rate.mjpgQp = (v3_venc_rate_mjpgqp){ .srcFps = config->framerate,
                    .dstFps = config->framerate, .quality = config->maxQual }; break;
            default:
                V3_ERROR("MJPEG encoder can only support CBR, VBR or fixed QP modes!");
        }
        goto attach;
    } else if (config->codec == HAL_VIDCODEC_H265) {
        channel.attrib.codec = V3_VENC_CODEC_H265;
        attrib = &channel.attrib.h265;
        switch (config->mode) {
            case HAL_VIDMODE_CBR:
                channel.rate.mode = V3_VENC_RATEMODE_H265CBR;
                channel.rate.h265Cbr = (v3_venc_rate_h26xcbr){ .gop = config->gop,
                    .statTime = 1, .srcFps = config->framerate, .dstFps = config->framerate,
                    .bitrate = config->bitrate, .avgLvl = 1 }; break;
            case HAL_VIDMODE_VBR:
                channel.rate.mode = V3_VENC_RATEMODE_H265VBR;
                channel.rate.h265Vbr = (v3_venc_rate_h26xvbr){ .gop = config->gop,
                    .statTime = 1, .srcFps = config->framerate, .dstFps = config->framerate, 
                    .maxBitrate = MAX(config->bitrate, config->maxBitrate), .maxQual = config->maxQual,
                    .minQual = config->minQual, .minIQual = config->minQual }; break;
            case HAL_VIDMODE_QP:
                channel.rate.mode = V3_VENC_RATEMODE_H265QP;
                channel.rate.h265Qp = (v3_venc_rate_h26xqp){ .gop = config->gop,
                    .srcFps = config->framerate, .dstFps = config->framerate, .interQual = config->maxQual, 
                    .predQual = config->minQual, .bipredQual = config->minQual }; break;
            case HAL_VIDMODE_AVBR:
                channel.rate.mode = V3_VENC_RATEMODE_H265AVBR;
                channel.rate.h265Avbr = (v3_venc_rate_h26xxvbr){ .gop = config->gop,
                    .statTime = 1, .srcFps = config->framerate, .dstFps = config->framerate,
                    .bitrate = config->bitrate }; break;
            default:
                V3_ERROR("H.265 encoder does not support this mode!");
        }
    } else if (config->codec == HAL_VIDCODEC_H264) {
        channel.attrib.codec = V3_VENC_CODEC_H264;
        attrib = &channel.attrib.h264;
        switch (config->mode) {
            case HAL_VIDMODE_CBR:
                channel.rate.mode = V3_VENC_RATEMODE_H264CBR;
                channel.rate.h264Cbr = (v3_venc_rate_h26xcbr){ .gop = config->gop,
                    .statTime = 1, .srcFps = config->framerate, .dstFps = config->framerate,
                    .bitrate = config->bitrate, .avgLvl = 1 }; break;
            case HAL_VIDMODE_VBR:
                channel.rate.mode = V3_VENC_RATEMODE_H264VBR;
                channel.rate.h264Vbr = (v3_venc_rate_h26xvbr){ .gop = config->gop,
                    .statTime = 1, .srcFps = config->framerate, .dstFps = config->framerate, 
                    .maxBitrate = MAX(config->bitrate, config->maxBitrate), .maxQual = config->maxQual,
                    .minQual = config->minQual, .minIQual = config->minQual }; break;
            case HAL_VIDMODE_QP:
                channel.rate.mode = V3_VENC_RATEMODE_H264QP;
                channel.rate.h264Qp = (v3_venc_rate_h26xqp){ .gop = config->gop,
                    .srcFps = config->framerate, .dstFps = config->framerate, .interQual = config->maxQual, 
                    .predQual = config->minQual, .bipredQual = config->minQual }; break;
            case HAL_VIDMODE_AVBR:
                channel.rate.mode = V3_VENC_RATEMODE_H264AVBR;
                channel.rate.h264Avbr = (v3_venc_rate_h26xxvbr){ .gop = config->gop,
                    .statTime = 1, .srcFps = config->framerate, .dstFps = config->framerate,
                    .bitrate = config->bitrate }; break;
            default:
                V3_ERROR("H.264 encoder does not support this mode!");
        }
    } else V3_ERROR("This codec is not supported by the hardware!");
    attrib->maxWidth = ALIGN_BACK(config->width, 16);
    attrib->maxHeight = ALIGN_BACK(config->height, 16);
    attrib->bufSize = ALIGN_BACK(config->height, 16) * ALIGN_BACK(config->width, 16);
    attrib->profile = config->profile;
    attrib->byFrame = 1;
    attrib->width = config->width;
    attrib->height = config->height;
attach:
    if (ret = v3_venc.fnCreateChannel(index, &channel))
        return ret;

    if (config->codec != HAL_VIDCODEC_JPG && 
        (ret = v3_venc.fnStartReceiving(index)))
        return ret;
    
    v3_state[index].payload = config->codec;

    return EXIT_SUCCESS;
}

int v3_encoder_destroy(char index)
{
    int ret;
    int vpss_grp = index / V3_VPSS_CHN_NUM;
    int vpss_chn = index - vpss_grp * V3_VPSS_CHN_NUM;

    v3_state[index].payload = HAL_VIDCODEC_UNSPEC;

    if (ret = v3_venc.fnStopReceiving(index))
        return ret;

    {
        v3_sys_bind source = { .module = V3_SYS_MOD_VPSS, 
            .device = vpss_grp, .channel = vpss_chn };
        v3_sys_bind dest = { .module = V3_SYS_MOD_VENC,
            .device = venc_dev, .channel = index };
        if (ret = v3_sys.fnUnbind(&source, &dest))
            return ret;
    }

    if (ret = v3_venc.fnDestroyChannel(index))
        return ret;
    
    if (ret = v3_vpss.fnDisableChannel(vpss_grp, vpss_chn))
        return ret;

    return EXIT_SUCCESS;
}
    
int v3_encoder_destroy_all(void)
{
    int ret;

    for (char i = 0; i < V3_VENC_CHN_NUM; i++)
        if (v3_state[i].enable)
            if (ret = v3_encoder_destroy(i))
                return ret;
}

void *v3_encoder_thread(void)
{
    int ret;
    int maxFd = 0;

    for (int i = 0; i < V3_VENC_CHN_NUM; i++) {
        if (!v3_state[i].enable) continue;
        if (!v3_state[i].mainLoop) continue;

        ret = v3_venc.fnGetDescriptor(i);
        if (ret < 0) return ret;
        v3_state[i].fileDesc = ret;

        if (maxFd <= v3_state[i].fileDesc)
            maxFd = v3_state[i].fileDesc;
    }

    v3_venc_stat stat;
    v3_venc_strm stream;
    struct timeval timeout;
    fd_set readFds;

    while (keepRunning) {
        FD_ZERO(&readFds);
        for(int i = 0; i < V3_VENC_CHN_NUM; i++) {
            if (!v3_state[i].enable) continue;
            if (!v3_state[i].mainLoop) continue;
            FD_SET(v3_state[i].fileDesc, &readFds);
        }

        timeout.tv_sec = 0;
        timeout.tv_usec = 0;
        ret = select(maxFd + 1, &readFds, NULL, NULL, &timeout);
        if (ret < 0) {
            fprintf(stderr, "[v3_venc] Select operation failed!\n");
            break;
        } else if (ret == 0) {
            fprintf(stderr, "[v3_venc] Main stream loop timed out!\n");
            continue;
        } else {
            for (int i = 0; i < V3_VENC_CHN_NUM; i++) {
                if (!v3_state[i].enable) continue;
                if (!v3_state[i].mainLoop) continue;
                if (FD_ISSET(v3_state[i].fileDesc, &readFds)) {
                    memset(&stream, 0, sizeof(stream));
                    
                    if (ret = v3_venc.fnQuery(i, &stat)) {
                        fprintf(stderr, "[v3_venc] Querying the encoder channel "
                            "%d failed with %#x!\n", i, ret);
                        break;
                    }

                    if (!stat.curPacks) {
                        fprintf(stderr, "[v3_venc] Current frame is empty, skipping it!\n");
                        continue;
                    }

                    stream.packet = (v3_venc_pack*)malloc(
                        sizeof(v3_venc_pack) * stat.curPacks);
                    if (!stream.packet) {
                        fprintf(stderr, "[v3_venc] Memory allocation on channel %d failed!\n", i);
                        break;
                    }
                    stream.count = stat.curPacks;

                    if (ret = v3_venc.fnGetStream(i, &stream, stat.curPacks)) {
                        fprintf(stderr, "[v3_venc] Getting the stream on "
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

                    if (ret = v3_venc.fnFreeStream(i, &stream)) {
                        fprintf(stderr, "[v3_venc] Releasing the stream on "
                            "channel %d failed with %#x!\n", i, ret);
                    }
                    free(stream.packet);
                    stream.packet = NULL;
                }
            }
        }
    }
    fprintf(stderr, "[v3_venc] Shutting down encoding thread...\n");
}

void *v3_image_thread(void)
{
    if (v3_isp.fnRun(isp_dev))
        fprintf(stderr, "[v3_isp] Shutting down ISP thread...\n");
}

void v3_pipeline_destroy(void)
{
    for (char grp = 0; grp < V3_VPSS_GRP_NUM; grp++)
    {
        for (char chn = 0; chn < V3_VPSS_CHN_NUM; chn++)
            v3_vpss.fnDisableChannel(grp, chn);

        {
            v3_sys_bind source = { .module = V3_SYS_MOD_VIU, 
                .device = vi_dev, .channel = vi_chn };
            v3_sys_bind dest = { .module = V3_SYS_MOD_VPSS,
                .device = grp, .channel = 0 };
            v3_sys.fnUnbind(&source, &dest);
        }

        v3_vpss.fnStopGroup(grp);
        v3_vpss.fnDestroyGroup(grp);
    }

    v3_vi.fnDisableChannel(vi_chn);
    v3_vi.fnDisableDevice(vi_dev);

    v3_isp.fnExit(isp_dev);
}

int v3_pipeline_create(char mirror, char flip)
{
    int ret;

    {
        v3_vi_dev device = { .adChn = {-1, -1, -1, -1} };
        if (ret = v3_vi.fnSetDeviceConfig(isp_dev, &device))
            return ret;
    }
    {
        v3_vi_wdr wdr = { .mode = V3_WDR_NONE, .comprOn = 0 };
        if (ret = v3_vi.fnSetWDRMode(isp_dev, &wdr))
            return ret;
    }
    if (ret = v3_vi.fnEnableDevice(isp_dev))
        return ret;
    {
        v3_vi_chn channel = { .mirror = mirror,  .flip = flip,
            .srcFps = -1, .dstFps = -1 };
        if (ret = v3_vi.fnSetChannelConfig(isp_chn, &channel))
            return ret;
    }
    if (ret = v3_vi.fnEnableChannel(isp_chn))
        return ret;

    {
        v3_vpss_grp group;
        group.imgEnhOn = 0;
        group.dciOn = 0;
        group.noiseRedOn = 0;
        group.histOn = 0;
        group.deintMode = 1;
        if (ret = v3_vpss.fnCreateGroup(vpss_grp, &group))
            return ret;
    }
    if (ret = v3_vpss.fnStartGroup(vpss_grp))
        return ret;

    {
        v3_sys_bind source = { .module = V3_SYS_MOD_VIU, 
            .device = vi_dev, .channel = vi_chn };
        v3_sys_bind dest = { .module = V3_SYS_MOD_VPSS, 
            .device = vpss_grp, .channel = 0 };
        if (ret = v3_sys.fnBind(&source, &dest))
            return ret;
    }


    return EXIT_SUCCESS;
}

int v3_sensor_config(void) {
    v3_snr_dev config;

    int fd = open(V3_SNR_ENDPOINT, O_RDWR);
    if (fd < 0)
        V3_ERROR("Opening imaging device has failed!\n");

    ioctl(fd, _IOW(V3_SNR_IOC_MAGIC, V3_SNR_CMD_RST_INTF, unsigned int), &config.device);
    ioctl(fd, _IOW(V3_SNR_IOC_MAGIC, V3_SNR_CMD_RST_SENS, unsigned int), &config.device);

    if (ioctl(fd, _IOW(V3_SNR_IOC_MAGIC, V3_SNR_CMD_CONF_DEV, v3_snr_dev), &config) && close(fd))
        V3_ERROR("Configuring imaging device has failed!\n");

    usleep(10000);

    ioctl(fd, _IOW(V3_SNR_IOC_MAGIC, V3_SNR_CMD_UNRST_INTF, unsigned int), &config.device);
    ioctl(fd, _IOW(V3_SNR_IOC_MAGIC, V3_SNR_CMD_UNRST_INTF, unsigned int), &config.device);

    close(fd);

    return EXIT_SUCCESS;
}

void v3_sensor_deinit(void)
{
    dlclose(v3_drv.handle);
    v3_drv.handle = NULL;
}

int v3_sensor_init(char *name)
{
    char* path;
    char* dirs[] = {"%s", "./%s", "/usr/lib/%s"};
    char **dir = dirs;

    while (dir) {
        asprintf(&path, *dir, name);
        if (v3_drv.handle = dlopen(path, RTLD_NOW | RTLD_GLOBAL))
            dir = NULL;
        free(path);
    } if (!v3_drv.handle)
        V3_ERROR("Failed to load the sensor driver");
    
    v3_drv.fnRegister = 
        (int(*)(void))dlsym(v3_drv.handle, "sensor_register_callback");
    v3_drv.fnUnregister =
        (int(*)(void))dlsym(v3_drv.handle, "sensor_unregister_callback");

    return EXIT_SUCCESS;
}

int v3_system_calculate_block(short width, short height, v3_common_pixfmt pixFmt,
    unsigned int alignWidth)
{
    if (alignWidth & 0b1110000) {
        fprintf(stderr, "[v3_sys] Alignment width (%d) "
            "is invalid!\n", alignWidth);
        return -1;
    }

    unsigned int bufSize = CEILING_2_POWER(width, alignWidth) *
        CEILING_2_POWER(height, alignWidth) *
        (pixFmt == V3_PIXFMT_YUV422SP ? 2 : 1.5);
    unsigned int headSize;
    if (pixFmt == V3_PIXFMT_YUV422SP || pixFmt >= V3_PIXFMT_RGB_BAYER_8BPP)
        headSize = 16 * height * 2;
    else if (pixFmt == V3_PIXFMT_YUV420SP)
        headSize = (16 * height * 3) >> 1;
    return bufSize + headSize;
}

void v3_system_deinit(void)
{

    v3_sys.fnExit();
    v3_vb.fnExit();

    v3_isp.fnUnregisterAF(isp_dev, &(v3_isp_alg){.libName = "hisi_af_lib"});
    v3_isp.fnUnregisterAWB(isp_dev, &(v3_isp_alg){.libName = "hisi_awb_lib"});
    v3_isp.fnUnregisterAE(isp_dev, &(v3_isp_alg){.libName = "hisi_ae_lib"});

    v3_drv.fnUnregister();
}

int v3_system_init(unsigned int alignWidth, unsigned int blockCnt, 
    unsigned int poolCnt)
{
    int ret;

    {
        v3_sys_ver version;
        v3_sys.fnGetVersion(&version);
        printf("App built with headers v%s\n", V3_SYS_API);
        printf("MPP version: %s\n", version.version);
    }

    {
        v3_vb_pool pool = {
            .count = poolCnt,
            .comm =
            {
                {
                    .blockSize = v3_system_calculate_block(0, 0, 0, alignWidth),
                    .blockCnt = blockCnt
                }
            }
        };
        if (ret = v3_vb.fnConfigPool(&pool))
            return ret;
    }
    {
        v3_vb_supl supl = V3_VB_USERINFO_MASK;
        if (ret = v3_vb.fnConfigSupplement(&supl))
            return ret;
    }
    if (ret = v3_vb.fnInit())
        return ret;

    {
        if (ret = v3_sys.fnSetAlignment(&alignWidth))
            return ret;
    }
    if (ret = v3_sys.fnInit())
        return ret;

    if (ret = v3_sensor_config())
        return ret;

    if (ret = v3_drv.fnRegister())
        return ret;

    if (ret = v3_isp.fnRegisterAE(isp_dev, &(v3_isp_alg){.libName = "hisi_ae_lib"}))
        return ret;
    if (ret = v3_isp.fnRegisterAWB(isp_dev, &(v3_isp_alg){.libName = "hisi_awb_lib"}))
        return ret;
    if (ret = v3_isp.fnRegisterAF(isp_dev, &(v3_isp_alg){.libName = "hisi_af_lib"}))
        return ret;
    if (ret = v3_isp.fnMemInit(isp_dev))
        return ret;
    {
        v3_common_wdr mode;
        if (ret = v3_isp.fnSetWDRMode(isp_dev, &mode))
            return ret;
    }
    {
        v3_isp_dev device;
        if (ret = v3_isp.fnSetDeviceConfig(isp_dev, &device))
            return ret;
    }
    if (ret = v3_isp.fnInit(isp_dev))
        return ret;
}