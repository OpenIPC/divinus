#if defined(__riscv) || defined(__riscv__)

#include "cvi_hal.h"

cvi_aud_impl     cvi_aud;
cvi_config_impl  cvi_config;
cvi_isp_impl     cvi_isp;
cvi_snr_drv_impl cvi_snr_drv;
cvi_rgn_impl     cvi_rgn;
cvi_sys_impl     cvi_sys;
cvi_vb_impl      cvi_vb;
cvi_venc_impl    cvi_venc;
cvi_vi_impl      cvi_vi;
cvi_vpss_impl    cvi_vpss;

hal_chnstate cvi_state[CVI_VENC_CHN_NUM] = {0};
int (*cvi_aud_cb)(hal_audframe*);
int (*cvi_vid_cb)(char, hal_vidstream*);

const char _cvi_vi_pipe = 0;
char _cvi_aud_chn = 0;
char _cvi_aud_dev = 0;
char _cvi_venc_dev = 0;
char _cvi_vi_chn = 0;
char _cvi_vi_dev = 0;
char _cvi_vpss_chn = 0;
char _cvi_vpss_grp = 0;

static volatile char cvi_ready[CVI_VENC_CHN_NUM];
static volatile char cvi_busy[CVI_VENC_CHN_NUM];
static char cvi_rgn_attached[16];

cvi_isp_alg cvi_ae_lib = { .id = _cvi_vi_pipe, .libName = "cvi_ae_lib" };
cvi_isp_alg cvi_awb_lib = { .id = _cvi_vi_pipe, .libName = "cvi_awb_lib" };

static void cvi_video_pause(void);
static void cvi_video_resume(void);

void cvi_hal_deinit(void)
{
    cvi_vpss_unload(&cvi_vpss);
    cvi_vi_unload(&cvi_vi);
    cvi_venc_unload(&cvi_venc);
    cvi_vb_unload(&cvi_vb);
    cvi_rgn_unload(&cvi_rgn);
    cvi_isp_unload(&cvi_isp);
    cvi_aud_unload(&cvi_aud);
    cvi_sys_unload(&cvi_sys);
}

static cvi_sys_bind cvi_rgn_channel(void) {
    return (cvi_sys_bind){ .module = CVI_SYS_MOD_VPSS,
        .device = _cvi_vpss_grp, .channel = 0 };
}

int cvi_hal_init(void)
{
    int ret;

    if (ret = cvi_sys_load(&cvi_sys))
        return ret;
    if (ret = cvi_aud_load(&cvi_aud))
        return ret;
    if (ret = cvi_isp_load(&cvi_isp))
        return ret;
    if (ret = cvi_rgn_load(&cvi_rgn))
        return ret;

    {
        cvi_sys_bind stale = cvi_rgn_channel();
        for (int i = 0; i < 16; i++) {
            cvi_rgn.fnDetachChannel(i, &stale);
            cvi_rgn.fnDestroyRegion(i);
        }
    }

    if (ret = cvi_vb_load(&cvi_vb))
        return ret;
    if (ret = cvi_venc_load(&cvi_venc))
        return ret;
    if (ret = cvi_vi_load(&cvi_vi))
        return ret;
    if (ret = cvi_vpss_load(&cvi_vpss))
        return ret;

    return EXIT_SUCCESS;
}

void cvi_audio_deinit(void)
{
    cvi_aud.fnDisableChannel(_cvi_aud_dev, _cvi_aud_chn);

    cvi_aud.fnDisableDevice(_cvi_aud_dev);

    cvi_aud.fnDeinit();
}

int cvi_audio_init(int samplerate)
{
    int ret;

    if (ret = cvi_aud.fnInit())
        return ret;

    {
        cvi_aud_cnf config;
        config.rate = samplerate;
        config.bit = CVI_AUD_BIT_16;
        config.intf = CVI_AUD_INTF_I2S_MASTER;
        config.stereoOn = 0;
        config.expandOn = 0;
        config.frmNum = 30;
        config.packNumPerFrm = 320;
        config.chnNum = 1;
        config.syncRxClkOn = 0;
        config.i2sType = CVI_AUD_I2ST_INNERCODEC;
        if (ret = cvi_aud.fnSetDeviceConfig(_cvi_aud_dev, &config))
            return ret;
    }
    if (ret = cvi_aud.fnEnableDevice(_cvi_aud_dev))
        return ret;

    if (ret = cvi_aud.fnEnableChannel(_cvi_aud_dev, _cvi_aud_chn))
        return ret;

    return EXIT_SUCCESS;
}

void *cvi_audio_thread(void)
{
    int ret;

    cvi_aud_frm frame;
    cvi_aud_efrm echoFrame;
    memset(&frame, 0, sizeof(frame));
    memset(&echoFrame, 0, sizeof(echoFrame));

    while (keepRunning && audioOn) {
        while ((ret = cvi_aud.fnGetFrame(_cvi_aud_dev, _cvi_aud_chn,
            &frame, &echoFrame, 0)) == 0) {
            if (cvi_aud_cb) {
                hal_audframe outFrame;
                outFrame.channelCnt = 1;
                outFrame.data[0] = frame.addr[0];
                outFrame.length[0] = frame.length;
                outFrame.seq = frame.sequence;
                outFrame.timestamp = frame.timestamp;
                (cvi_aud_cb)(&outFrame);
            }

            if (ret = cvi_aud.fnFreeFrame(_cvi_aud_dev, _cvi_aud_chn,
                &frame, &echoFrame)) {
                HAL_WARNING("cvi_aud", "Releasing the frame failed"
                    " with %#x!\n", ret);
            }
        }

        usleep(5000);
    }
    fprintf(stderr, "[cvi_aud] Shutting down capture thread...\n");
}

int cvi_channel_bind(char index)
{
    int ret;

    if (ret = cvi_vpss.fnEnableChannel(_cvi_vpss_grp, index))
        return ret;

    cvi_ready[(unsigned char)index] = 1;

    return EXIT_SUCCESS;
}

int cvi_channel_create(char index, short width, short height, char framerate,
    char mirror, char flip)
{
    int ret;

    {
        cvi_vpss_chn channel;
        memset(&channel, 0, sizeof(channel));
        channel.dest.width = width;
        channel.dest.height = height;
        channel.pixFmt = CVI_PIXFMT_YUV420P;
        channel.srcFps = cvi_config.isp.framerate;
        channel.dstFps = framerate;
        channel.mirror = mirror;
        channel.flip = flip;
        channel.depth = 2;
        channel.aspectRatio = 1;
        channel.aspectBgOn = 1;
        channel.aspectBgCol = 0;

        if (ret = cvi_vpss.fnSetChannelConfig(_cvi_vpss_grp, index, &channel))
            return ret;
    }

    return EXIT_SUCCESS;
}

int cvi_channel_grayscale(char enable)
{
    int ret;

    for (char i = 0; i < CVI_VENC_CHN_NUM; i++) {
        cvi_venc_para param;
        if (!cvi_state[i].enable) continue;
        if (ret = cvi_venc.fnGetChannelParam(i, &param))
            return ret;
        param.grayscaleOn = enable;
        if (ret = cvi_venc.fnSetChannelParam(i, &param))
            return ret;
    }

    return EXIT_SUCCESS;
}

int cvi_channel_unbind(char index)
{
    int ret;

    cvi_ready[(unsigned char)index] = 0;
    while (cvi_busy[(unsigned char)index]) usleep(1000);

    if (ret = cvi_vpss.fnDisableChannel(_cvi_vpss_grp, index))
        return ret;

    return EXIT_SUCCESS;
}

void *cvi_image_thread(void)
{
    int ret;

    if (ret = cvi_isp.fnRun(_cvi_vi_pipe))
        HAL_DANGER("cvi_isp", "Operation failed with %#x!\n", ret);
    HAL_INFO("cvi_isp", "Shutting down ISP thread...\n");
}

static int cvi_pipeline_forward(int channel) {
    int ret;
    cvi_frame frame;

    if (ret = cvi_vpss.fnGetChannelFrame(_cvi_vpss_grp, channel, &frame, 1000))
        return ret;

    if (ret = cvi_venc.fnSendFrame(channel, &frame, 1000))
        HAL_WARNING("cvi_venc", "Sending the frame on channel %d "
            "failed with %#x!\n", channel, ret);

    cvi_vpss.fnReleaseChannelFrame(_cvi_vpss_grp, channel, &frame);

    return ret;
}

int cvi_pipeline_create(void)
{
    int ret;

    cvi_vpss.fnDestroyGroup(_cvi_vpss_grp);

    {
        cvi_sys_vimd mode[CVI_VI_BIND_PIPE_NUM];
        memset(mode, 0, sizeof(mode));

        mode[0] = CVI_SYS_VIMD_VIOFF_VPSSON;
        if (ret = cvi_sys.fnSetViVpssMode((cvi_sys_vimd**)&mode))
            return ret;
    }

    {
        cvi_sys_vpcf config = {
            .mode = CVI_SYS_VPSS_DUAL,
            .inputIsIsp = {0, 1},
            .pipeId = {0, _cvi_vi_pipe}
        };
        if (ret = cvi_sys.fnSetVpssMode(&config))
            return ret;
    }

    if (ret = cvi_vi.fnSetDeviceConfig(_cvi_vi_dev, &cvi_config.videv))
        return ret;
    if (ret = cvi_vi.fnEnableDevice(_cvi_vi_dev))
        return ret;

    if (ret = cvi_snr_drv.obj->pfnRegisterCallback(_cvi_vi_pipe, &cvi_ae_lib, &cvi_awb_lib))
        return ret;

    if (ret = cvi_sensor_config())
        return ret;

    {
        cvi_vi_bind bind;
        memset(&bind, 0, sizeof(bind));
        bind.num = 1;
        bind.pipeId[0] = _cvi_vi_pipe;
        if (ret = cvi_vi.fnBindPipe(_cvi_vi_dev, &bind))
            return ret;
    }

    {
        cvi_vi_pipe pipe;
        memset(&pipe, 0, sizeof(pipe));
        pipe.bypass = 0;
        pipe.yuvSkipOn = 0;
        pipe.ispBypassOn = 0;
        pipe.maxSize.width = cvi_config.isp.capt.width;
        pipe.maxSize.height = cvi_config.isp.capt.height;
        pipe.pixFmt = CVI_PIXFMT_RGB_BAYER_8BPP + cvi_config.mipi.prec;
        pipe.compress = CVI_COMPR_NONE;
        pipe.prec = cvi_config.mipi.prec;
        pipe.nRedOn = 1;
        pipe.sharpenOn = 0;
        pipe.srcFps = -1;
        pipe.dstFps = -1;
        if (ret = cvi_vi.fnCreatePipe(_cvi_vi_pipe, &pipe))
            return ret;
    }
    if (ret = cvi_vi.fnStartPipe(_cvi_vi_pipe))
        return ret;

    if (ret = cvi_isp.fnRegisterAE(_cvi_vi_pipe, &cvi_ae_lib))
        return ret;
    if (ret = cvi_isp.fnRegisterAWB(_cvi_vi_pipe, &cvi_awb_lib))
        return ret;
    if (ret = cvi_isp.fnMemInit(_cvi_vi_pipe))
        return ret;

    cvi_config.isp.capt.x = 0;
    cvi_config.isp.capt.y = 0;
    if (ret = cvi_isp.fnSetDeviceConfig(_cvi_vi_pipe, &cvi_config.isp))
        return ret;
    if (ret = cvi_isp.fnInit(_cvi_vi_pipe))
        return ret;

    {
        cvi_vi_chn channel;
        memset(&channel, 0, sizeof(channel));
        channel.size.width = cvi_config.isp.capt.width;
        channel.size.height = cvi_config.isp.capt.height;
        channel.pixFmt = CVI_PIXFMT_NV21;
        channel.dynRange = CVI_HDR_SDR8;
        channel.compress = CVI_COMPR_NONE;
        channel.mirror = 0;
        channel.flip = 0;
        channel.depth = 1;
        channel.srcFps = -1;
        channel.dstFps = -1;
        if (ret = cvi_vi.fnSetChannelConfig(_cvi_vi_pipe, _cvi_vi_chn, &channel))
            return ret;
    }
    if (ret = cvi_vi.fnEnableChannel(_cvi_vi_pipe, _cvi_vi_chn))
        return ret;

    {
        cvi_vpss_grp group;
        memset(&group, 0, sizeof(group));
        group.dest.width = cvi_config.isp.capt.width;
        group.dest.height = cvi_config.isp.capt.height;
        group.pixFmt = CVI_PIXFMT_NV21;
        group.srcFps = -1;
        group.dstFps = -1;
        group.device = 1;
        if (ret = cvi_vpss.fnCreateGroup(_cvi_vpss_grp, &group))
            return ret;
        if (ret = cvi_vpss.fnResetGroup(_cvi_vpss_grp))
            return ret;
    }

    return EXIT_SUCCESS;
}

int cvi_pipeline_start(void)
{
    int ret;

    if (ret = cvi_vpss.fnStartGroup(_cvi_vpss_grp))
        return ret;

    {
        cvi_sys_bind source = { .module = CVI_SYS_MOD_VI,
            .device = _cvi_vi_dev, .channel = _cvi_vi_chn };
        cvi_sys_bind dest = { .module = CVI_SYS_MOD_VPSS,
            .device = _cvi_vpss_grp, .channel = 0 };
        if (ret = cvi_sys.fnBind(&source, &dest))
            return ret;
    }

    return EXIT_SUCCESS;
}

void cvi_pipeline_destroy(void)
{
    cvi_isp.fnExit(_cvi_vi_pipe);
    cvi_isp.fnUnregisterAE(_cvi_vi_pipe, &cvi_ae_lib);
    cvi_isp.fnUnregisterAWB(_cvi_vi_pipe, &cvi_awb_lib);

    cvi_snr_drv.obj->pfnUnRegisterCallback(_cvi_vi_pipe, &cvi_ae_lib, &cvi_awb_lib);

    for (char grp = 0; grp < CVI_VPSS_GRP_NUM; grp++)
    {
        for (char chn = 0; chn < CVI_VPSS_CHN_NUM; chn++)
            cvi_vpss.fnDisableChannel(grp, chn);

        {
            cvi_sys_bind source = { .module = CVI_SYS_MOD_VI,
                .device = _cvi_vi_dev, .channel = _cvi_vi_chn };
            cvi_sys_bind dest = { .module = CVI_SYS_MOD_VPSS,
                .device = grp, .channel = 0 };
            cvi_sys.fnUnbind(&source, &dest);
        }

        cvi_vpss.fnStopGroup(grp);
        cvi_vpss.fnDestroyGroup(grp);
    }

    cvi_vi.fnDisableChannel(_cvi_vi_pipe, _cvi_vi_chn);

    cvi_vi.fnStopPipe(_cvi_vi_pipe);
    cvi_vi.fnDestroyPipe(_cvi_vi_pipe);

    cvi_vi.fnDisableDevice(_cvi_vi_dev);
}

int cvi_region_create(char handle, hal_rect rect, short opacity)
{
    int ret = EXIT_SUCCESS;
    unsigned int id = (unsigned char)handle % 16;

    cvi_sys_bind channel = cvi_rgn_channel();
    cvi_rgn_cnf region, regionCurr;
    cvi_rgn_chn attrib;

    cvi_video_pause();

    memset(&region, 0, sizeof(region));
    region.type = CVI_RGN_TYPE_OVERLAY;
    region.overlay.pixFmt = CVI_PIXFMT_ARGB8888;
    region.overlay.size.width = rect.width;
    region.overlay.size.height = rect.height;
    region.overlay.canvas = 2;

    if (cvi_rgn.fnGetRegionConfig(handle, &regionCurr)) {
        if (ret = cvi_rgn.fnCreateRegion(handle, &region))
            goto resume;
    } else if (regionCurr.overlay.size.height != rect.height ||
        regionCurr.overlay.size.width != rect.width) {
        if (cvi_rgn_attached[id])
            cvi_rgn.fnDetachChannel(handle, &channel);
        cvi_rgn.fnDestroyRegion(handle);
        cvi_rgn_attached[id] = 0;
        if (ret = cvi_rgn.fnCreateRegion(handle, &region))
            goto resume;
    }

    memset(&attrib, 0, sizeof(attrib));
    attrib.show = 1;
    attrib.type = CVI_RGN_TYPE_OVERLAY;
    attrib.overlay.point.x = rect.x;
    attrib.overlay.point.y = rect.y;

    if (!cvi_rgn_attached[id]) {
        if (ret = cvi_rgn.fnAttachChannel(handle, &channel, &attrib))
            HAL_WARNING("cvi_rgn", "Attaching region %d failed with %#x!\n", handle, ret);
        else
            cvi_rgn_attached[id] = 1;
    } else if (ret = cvi_rgn.fnSetChannelConfig(handle, &channel, &attrib))
        HAL_WARNING("cvi_rgn", "Moving region %d failed with %#x!\n", handle, ret);

resume:
    cvi_video_resume();
    return ret;
}

void cvi_region_destroy(char handle)
{
    unsigned int id = (unsigned char)handle % 16;
    cvi_sys_bind channel = cvi_rgn_channel();

    cvi_video_pause();

    if (cvi_rgn_attached[id])
        cvi_rgn.fnDetachChannel(handle, &channel);
    cvi_rgn.fnDestroyRegion(handle);
    cvi_rgn_attached[id] = 0;

    cvi_video_resume();
}

int cvi_region_setbitmap(int handle, hal_bitmap *bitmap)
{
    unsigned int pixels = bitmap->dim.width * bitmap->dim.height;
    unsigned int *argb = malloc(pixels * 4);
    unsigned short *src = bitmap->data;
    cvi_rgn_canv canvas;
    unsigned int width, height;
    int ret;

    if (!argb)
        return EXIT_FAILURE;

    for (unsigned int i = 0; i < pixels; i++) {
        unsigned short px = src[i];
        unsigned int r = (px >> 10) & 0x1F, g = (px >> 5) & 0x1F, b = px & 0x1F;
        argb[i] = ((px & 0x8000) ? 0xFF000000 : 0) |
            ((r * 255 / 31) << 16) | ((g * 255 / 31) << 8) | (b * 255 / 31);
    }

    if (ret = cvi_rgn.fnGetCanvas(handle, &canvas)) {
        HAL_WARNING("cvi_rgn", "Getting the canvas on region %d failed with %#x!\n",
            handle, ret);
        free(argb);
        return ret;
    }

    width = bitmap->dim.width < canvas.size.width ?
        bitmap->dim.width : canvas.size.width;
    height = bitmap->dim.height < canvas.size.height ?
        bitmap->dim.height : canvas.size.height;
    for (unsigned int y = 0; y < height; y++)
        memcpy(canvas.virtAddr + y * canvas.stride,
            argb + y * bitmap->dim.width, width * 4);

    if (ret = cvi_rgn.fnUpdateCanvas(handle))
        HAL_WARNING("cvi_rgn", "Updating the canvas on region %d failed with %#x!\n",
            handle, ret);

    free(argb);
    return ret;
}

static void cvi_sensor_name(const char *type, char *name, unsigned int size) {
    const char *start = type;
    unsigned int len;

    if (!strncmp(start, "stSns", 5))
        start += 5;
    len = strcspn(start, "_");
    if (len >= size)
        len = size - 1;
    for (unsigned int i = 0; i < len; i++) {
        char c = start[i];
        name[i] = (c >= 'A' && c <= 'Z') ? c + ('a' - 'A') : c;
    }
    name[len] = '\0';
}

int cvi_sensor_config(void) {
    int device = 0, fd;
    cvi_snr_dev config;
    memset(&config, 0, sizeof(config));
    config.device = device;
    config.input = cvi_config.input_mode;
    config.dest.width = cvi_config.isp.capt.width;
    config.dest.height = cvi_config.isp.capt.height;
    config.pllck.cam = device;
    config.pllck.freq = CVI_SNR_PLLFR_25M;
    if (config.input == CVI_SNR_INPUT_MIPI)
        memcpy(&config.mipi, &cvi_config.mipi, sizeof(cvi_snr_mipi));
    else if (config.input == CVI_SNR_INPUT_LVDS)
        memcpy(&config.lvds, &cvi_config.lvds, sizeof(cvi_snr_lvds));

    cvi_snr_init init;
    memset(&init, 0, sizeof(init));
    init.gain = 0;
    if (cvi_snr_drv.obj->pfnSetInit)
        cvi_snr_drv.obj->pfnSetInit(_cvi_vi_pipe, &init);

    cvi_snr_bus bus;
    memset(&bus, 0, sizeof(bus));
    bus.i2c = cvi_config.i2c_bus;
    if (cvi_snr_drv.obj->pfnSetBusInfo)
        cvi_snr_drv.obj->pfnSetBusInfo(_cvi_vi_pipe, bus);
    if (cvi_snr_drv.obj->pfnPatchI2cAddr && cvi_config.i2c_addr)
        cvi_snr_drv.obj->pfnPatchI2cAddr(cvi_config.i2c_addr);

    cvi_snr_rx rx;
    memset(&rx, 0, sizeof(rx));
    rx.mipiDev = device;
    for (int i = 0; i < CVI_SNR_LANE_ARRAY; i++) {
        rx.laneId[i] = cvi_config.mipi.laneId[i];
        rx.pnSwap[i] = 0;
    }
    rx.mclk = 2;
    rx.mclkOn = 1;
    if (cvi_snr_drv.obj->pfnPatchRxAttr)
        cvi_snr_drv.obj->pfnPatchRxAttr(&rx);

    cvi_snr_exp exp;
    memset(&exp, 0, sizeof(exp));
    if (cvi_snr_drv.obj->pfnExpSensorCb)
        cvi_snr_drv.obj->pfnExpSensorCb(&exp);
    if (exp.fn_set_image_mode) {
        cvi_snr_imgmode mode;
        memset(&mode, 0, sizeof(mode));
        mode.width = cvi_config.isp.capt.width;
        mode.height = cvi_config.isp.capt.height;
        mode.fps = cvi_config.isp.framerate;
        if (exp.fn_set_image_mode)
            exp.fn_set_image_mode(_cvi_vi_pipe, &mode);
    }
    if (exp.fn_set_wdr_mode)
        exp.fn_set_wdr_mode(_cvi_vi_pipe, cvi_config.mode);

    if (exp.fn_sensor_init)
        exp.fn_sensor_init(_cvi_vi_pipe);

    cvi_snr_dev rxattr;
    memset(&rxattr, 0, sizeof(rxattr));
    if (cvi_snr_drv.obj->pfnGetRxAttr &&
        cvi_snr_drv.obj->pfnGetRxAttr(_cvi_vi_pipe, &rxattr) == EXIT_SUCCESS) {
        if (rxattr.mipi.dphyEnable) {
            config.mipi.dphyEnable = rxattr.mipi.dphyEnable;
            config.mipi.dphyHsSettle = rxattr.mipi.dphyHsSettle;
        }
        if (rxattr.input) config.input = rxattr.input;
        if (rxattr.pllck.freq) config.pllck.freq = rxattr.pllck.freq;
    }

    cvi_isp.fnResetSensor(device, 1);

    cvi_isp.fnResetIntf(device, 1);

    cvi_isp.fnSetIntfConfig(_cvi_vi_pipe, &config);

    cvi_isp.fnSetSensorClock(device, 1);

    usleep(20);

    cvi_isp.fnResetSensor(device, 0);

    usleep(10000);

    return EXIT_SUCCESS;
}

void cvi_sensor_deconfig(void) {
    int device = 0, fd;

    cvi_isp.fnResetSensor(device, 1);

    cvi_isp.fnSetSensorClock(device, 0);

    cvi_isp.fnResetIntf(device, 0);

    cvi_isp.fnResetSensor(device, 0);
}

void cvi_sensor_deinit(void)
{
    dlclose(cvi_snr_drv.handle);
    cvi_snr_drv.handle = NULL;
}

int cvi_sensor_init(char *name, char *obj)
{
    char path[128];
    char* dirs[] = {"%s", "./%s", "/usr/lib/sensors/%s", "/usr/lib/%s",
        "/mnt/system/lib/%s", "/mnt/system/lib/libsns_full.so", NULL};
    char **dir = dirs;

    while (*dir) {
        sprintf(path, *dir++, name);
        if (cvi_snr_drv.handle = dlopen(path, RTLD_LAZY | RTLD_GLOBAL))
            break;
    } if (!cvi_snr_drv.handle)
        HAL_ERROR("cvi_snr", "Failed to load the sensor driver\n");

    if (!(cvi_snr_drv.obj = (cvi_snr_obj*)dlsym(cvi_snr_drv.handle, obj)))
        HAL_ERROR("cvi_snr", "Failed to connect the sensor object\nError: %s\n", dlerror());

    return EXIT_SUCCESS;
}

static void cvi_video_pause(void) {
    for (int i = 0; i < CVI_VENC_CHN_NUM; i++)
        if (cvi_state[i].enable && cvi_state[i].mainLoop) {
            cvi_ready[i] = 0;
            while (cvi_busy[i]) usleep(1000);
        }
}

static void cvi_video_resume(void) {
    for (int i = 0; i < CVI_VENC_CHN_NUM; i++)
        if (cvi_state[i].enable && cvi_state[i].mainLoop)
            cvi_ready[i] = 1;
}

int cvi_video_create(char index, hal_vidconfig *config)
{
    int ret;
    cvi_venc_chn channel;
    memset(&channel, 0, sizeof(channel));

    unsigned char minQual = config->minQual, maxQual = config->maxQual,
        jpgQual = config->maxQual > 99 ? 75 : config->maxQual;

    if (!maxQual || maxQual > 51) maxQual = 32;
    if (!minQual || minQual > 51) minQual = 32;

    {
        int modparam[3] = { 1, 1, 1 };
        if (cvi_venc.fnSetModParam && (ret = cvi_venc.fnSetModParam(modparam)))
            HAL_WARNING("cvi_venc", "Setting the module parameter failed with %#x!\n", ret);
    }

    channel.gop.mode = CVI_VENC_GOPMODE_NORMALP;
    if (config->codec == HAL_VIDCODEC_JPG || config->codec == HAL_VIDCODEC_MJPG) {
        channel.attrib.codec = CVI_VENC_CODEC_MJPG;
        switch (config->mode) {
            case HAL_VIDMODE_CBR:
                channel.rate.mode = CVI_VENC_RATEMODE_MJPGCBR;
                channel.rate.mjpgCbr = (cvi_venc_rate_mjpgbr){ .statTime = 1, .srcFps = config->framerate,
                    .dstFps = config->framerate, .maxBitrate = config->bitrate }; break;
            case HAL_VIDMODE_VBR:
                channel.rate.mode = CVI_VENC_RATEMODE_MJPGVBR;
                channel.rate.mjpgVbr = (cvi_venc_rate_mjpgbr){ .statTime = 1,
                    .srcFps = config->framerate, .dstFps = config->framerate,
                    .maxBitrate = MAX(config->bitrate, config->maxBitrate) }; break;
            case HAL_VIDMODE_QP:
                channel.rate.mode = CVI_VENC_RATEMODE_MJPGQP;
                channel.rate.mjpgQp = (cvi_venc_rate_mjpgqp){ .srcFps = config->framerate,
                    .dstFps = config->framerate, .quality = jpgQual }; break;
            default:
                HAL_ERROR("cvi_venc", "MJPEG encoder can only support CBR, VBR or fixed QP modes!");
        }
    } else if (config->codec == HAL_VIDCODEC_H265) {
        channel.attrib.codec = CVI_VENC_CODEC_H265;
        channel.gop.normalP.ipQualDelta = config->gop / config->framerate;
        switch (config->mode) {
            case HAL_VIDMODE_CBR:
                channel.rate.mode = CVI_VENC_RATEMODE_H265CBR;
                channel.rate.h265Cbr = (cvi_venc_rate_h26xbr){ .gop = config->gop,
                    .statTime = 1, .srcFps = config->framerate, .dstFps = config->framerate,
                    .maxBitrate = config->bitrate }; break;
            case HAL_VIDMODE_VBR:
                channel.rate.mode = CVI_VENC_RATEMODE_H265VBR;
                channel.rate.h265Vbr = (cvi_venc_rate_h26xbr){ .gop = config->gop,
                    .statTime = 1, .srcFps = config->framerate, .dstFps = config->framerate,
                    .maxBitrate = MAX(config->bitrate, config->maxBitrate) }; break;
            case HAL_VIDMODE_QP:
                channel.rate.mode = CVI_VENC_RATEMODE_H265QP;
                channel.rate.h265Qp = (cvi_venc_rate_h26xqp){ .gop = config->gop,
                    .srcFps = config->framerate, .dstFps = config->framerate, .interQual = maxQual,
                    .predQual = minQual, .bipredQual = minQual }; break;
            case HAL_VIDMODE_AVBR:
                channel.rate.mode = CVI_VENC_RATEMODE_H265AVBR;
                channel.rate.h265Avbr = (cvi_venc_rate_h26xbr){ .gop = config->gop,
                    .statTime = 1, .srcFps = config->framerate, .dstFps = config->framerate,
                    .maxBitrate = config->bitrate }; break;
            default:
                HAL_ERROR("cvi_venc", "H.265 encoder does not support this mode!");
        }
    } else if (config->codec == HAL_VIDCODEC_H264) {
        channel.attrib.codec = CVI_VENC_CODEC_H264;
        channel.gop.normalP.ipQualDelta = config->gop / config->framerate;
        switch (config->mode) {
            case HAL_VIDMODE_CBR:
                channel.rate.mode = CVI_VENC_RATEMODE_H264CBR;
                channel.rate.h264Cbr = (cvi_venc_rate_h26xbr){ .gop = config->gop,
                    .statTime = 1, .srcFps = config->framerate, .dstFps = config->framerate,
                    .maxBitrate = config->bitrate }; break;
            case HAL_VIDMODE_VBR:
                channel.rate.mode = CVI_VENC_RATEMODE_H264VBR;
                channel.rate.h264Vbr = (cvi_venc_rate_h26xbr){ .gop = config->gop,
                    .statTime = 1, .srcFps = config->framerate, .dstFps = config->framerate,
                    .maxBitrate = MAX(config->bitrate, config->maxBitrate) }; break;
            case HAL_VIDMODE_QP:
                channel.rate.mode = CVI_VENC_RATEMODE_H264QP;
                channel.rate.h264Qp = (cvi_venc_rate_h26xqp){ .gop = config->gop,
                    .srcFps = config->framerate, .dstFps = config->framerate, .interQual = maxQual,
                    .predQual = minQual, .bipredQual = minQual }; break;
            case HAL_VIDMODE_AVBR:
                channel.rate.mode = CVI_VENC_RATEMODE_H264AVBR;
                channel.rate.h264Avbr = (cvi_venc_rate_h26xbr){ .gop = config->gop,
                    .statTime = 1, .srcFps = config->framerate, .dstFps = config->framerate,
                    .maxBitrate = config->bitrate }; break;
            default:
                HAL_ERROR("cvi_venc", "H.264 encoder does not support this mode!");
        }
    } else HAL_ERROR("cvi_venc", "This codec is not supported by the hardware!");
    channel.attrib.maxPic.width = config->width;
    channel.attrib.maxPic.height = config->height;
    channel.attrib.bufSize = 0;
    if (channel.attrib.codec == CVI_VENC_CODEC_H264)
        channel.attrib.profile = config->profile;
    channel.attrib.byFrame = 1;
    channel.attrib.pic.width = config->width;
    channel.attrib.pic.height = config->height;
    channel.attrib.esBufQueueOn = 1;
    channel.attrib.isolateFrmOn = 1;

    if (ret = cvi_venc.fnCreateChannel(index, &channel))
        return ret;

    {
        int count = -1;
        if (config->codec != HAL_VIDCODEC_JPG &&
            (ret = cvi_venc.fnStartReceivingEx(index, &count)))
            return ret;
    }

    cvi_state[index].payload = config->codec;

    return EXIT_SUCCESS;
}

int cvi_video_destroy(char index)
{
    int ret;

    cvi_ready[(unsigned char)index] = 0;
    while (cvi_busy[(unsigned char)index]) usleep(1000);

    cvi_state[index].enable = 0;
    cvi_state[index].payload = HAL_VIDCODEC_UNSPEC;

    cvi_venc.fnStopReceiving(index);

    if (ret = cvi_venc.fnDestroyChannel(index))
        return ret;

    return EXIT_SUCCESS;
}

int cvi_video_destroy_all(void)
{
    int ret;

    for (int i = 0; i < 16; i++)
        if (cvi_rgn_attached[i])
            cvi_region_destroy(i);

    for (char i = 0; i < CVI_VENC_CHN_NUM; i++)
        if (cvi_state[i].enable)
            if (ret = cvi_video_destroy(i))
                return ret;

    return EXIT_SUCCESS;
}

void cvi_video_request_idr(char index)
{
    cvi_venc.fnRequestIdr(index, 1);
}

int cvi_video_snapshot_grab(char index, hal_jpegdata *jpeg)
{
    int ret = EXIT_SUCCESS;

    cvi_venc_stat stat = {0};
    cvi_venc_strm strm = {0};
    char bound = 0, receiving = 0, opened = 0;

    if (ret = cvi_channel_bind(index)) {
        HAL_DANGER("cvi_venc", "Binding the encoder channel "
            "%d failed with %#x!\n", index, ret);
        goto abort;
    }
    bound = 1;

    int count = 1;
    if (ret = cvi_venc.fnStartReceivingEx(index, &count)) {
        HAL_DANGER("cvi_venc", "Requesting one frame "
            "%d failed with %#x!\n", index, ret);
        goto abort;
    }
    receiving = 1;

    if (ret = cvi_pipeline_forward(index)) {
        HAL_DANGER("cvi_venc", "Getting the frame on channel "
            "%d failed with %#x!\n", index, ret);
        goto abort;
    }

    int fd = cvi_venc.fnGetDescriptor(index);
    if (fd < 0) {
        ret = fd;
        HAL_DANGER("cvi_venc", "Getting the encoder descriptor "
            "failed with %#x!\n", ret);
        goto abort;
    }
    opened = 1;

    struct timeval timeout = { .tv_sec = 2, .tv_usec = 0 };
    fd_set readFds;
    FD_ZERO(&readFds);
    FD_SET(fd, &readFds);
    ret = select(fd + 1, &readFds, NULL, NULL, &timeout);
    if (ret < 0) {
        HAL_DANGER("cvi_venc", "Select operation failed!\n");
        goto abort;
    } else if (ret == 0) {
        HAL_DANGER("cvi_venc", "Capture stream timed out!\n");
        ret = EXIT_FAILURE;
        goto abort;
    }

    if (FD_ISSET(fd, &readFds)) {
        if (ret = cvi_venc.fnQuery(index, &stat)) {
            HAL_DANGER("cvi_venc", "Querying the encoder channel "
                "%d failed with %#x!\n", index, ret);
            goto abort;
        }

        if (!stat.curPacks) {
            HAL_DANGER("cvi_venc", "Current frame is empty, skipping it!\n");
            ret = EXIT_FAILURE;
            goto abort;
        }

        cvi_venc_pack packs[8];
        if (stat.curPacks > 8)
            strm.packet = (cvi_venc_pack*)malloc(sizeof(cvi_venc_pack) * stat.curPacks);
        else
            strm.packet = packs;

        if (!strm.packet) {
            HAL_DANGER("cvi_venc", "Memory allocation on channel %d failed!\n", index);
            ret = EXIT_FAILURE;
            goto abort;
        }
        strm.count = stat.curPacks;

        if (ret = cvi_venc.fnGetStream(index, &strm, 1000)) {
            HAL_DANGER("cvi_venc", "Getting the stream on "
                "channel %d failed with %#x!\n", index, ret);
            goto abort;
        }

        {
            jpeg->jpegSize = 0;
            for (unsigned int i = 0; i < strm.count; i++) {
                cvi_venc_pack *pack = &strm.packet[i];
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
    }

abort:
    if (strm.count) cvi_venc.fnFreeStream(index, &strm);
    if (stat.curPacks > 8 && strm.packet) free(strm.packet);
    if (opened) cvi_venc.fnFreeDescriptor(index);
    if (receiving) cvi_venc.fnStopReceiving(index);
    if (bound) cvi_channel_unbind(index);

    return ret;
}

void *cvi_video_thread(void)
{
    int ret, maxFd = 0;

    if (ret = cvi_pipeline_start())
        HAL_DANGER("cvi_vpss", "Starting the group failed with %#x!\n", ret);

    for (int i = 0; i < CVI_VENC_CHN_NUM; i++) {
        if (!cvi_state[i].enable) continue;
        if (!cvi_state[i].mainLoop) continue;

        ret = cvi_venc.fnGetDescriptor(i);
        if (ret < 0) {
            HAL_DANGER("cvi_venc", "Getting the encoder descriptor failed with %#x!\n", ret);
            return (void*)0;
        }
        cvi_state[i].fileDesc = ret;

        if (maxFd <= cvi_state[i].fileDesc)
            maxFd = cvi_state[i].fileDesc;
    }

    cvi_venc_strm stream;

    while (keepRunning) {
        for (int i = 0; i < CVI_VENC_CHN_NUM; i++) {
            if (!cvi_state[i].enable) continue;
            if (!cvi_state[i].mainLoop) continue;
            if (!cvi_ready[i]) continue;

            cvi_busy[i] = 1;

            if (ret = cvi_pipeline_forward(i)) {
                cvi_busy[i] = 0;
                continue;
            }

            cvi_venc_pack packs[16];
            stream.packet = packs;
            stream.count = 16;

            if (ret = cvi_venc.fnGetStream(i, &stream, 1000)) {
                if (ret != 0xc0078012)
                    HAL_WARNING("cvi_venc", "Getting the stream on "
                        "channel %d failed with %#x!\n", i, ret);
                cvi_busy[i] = 0;
                continue;
            }

            if (cvi_vid_cb) {
                hal_vidstream outStrm;
                hal_vidpack outPack[stream.count];
                outStrm.count = stream.count;
                outStrm.seq = stream.sequence;
                for (int j = 0; j < stream.count; j++) {
                    cvi_venc_pack *pack = &stream.packet[j];
                    outPack[j].data = pack->data;
                    outPack[j].length = pack->length;
                    outPack[j].naluCnt = 1;
                    outPack[j].nalu[0].length = pack->length;
                    outPack[j].nalu[0].offset = pack->offset;
                    switch (cvi_state[i].payload) {
                        case HAL_VIDCODEC_H264:
                            outPack[j].nalu[0].type = pack->naluType.h264Nalu;
                            break;
                        case HAL_VIDCODEC_H265:
                            outPack[j].nalu[0].type = pack->naluType.h265Nalu;
                            break;
                    }
                    outPack[j].offset = pack->offset;
                    outPack[j].timestamp = pack->timestamp;
                }
                outStrm.pack = outPack;
                (*cvi_vid_cb)(i, &outStrm);
            }

            if (ret = cvi_venc.fnFreeStream(i, &stream)) {
                HAL_WARNING("cvi_venc", "Releasing the stream on "
                    "channel %d failed with %#x!\n", i, ret);
            }

            cvi_busy[i] = 0;
        }
    }

    HAL_INFO("cvi_venc", "Shutting down encoding thread...\n");
}

void cvi_system_deinit(void)
{
    cvi_sys.fnExit();
    cvi_vb.fnExit();

    cvi_sensor_deinit();
}

int cvi_system_init(char *snrConfig)
{
    int ret;

    printf("App built with headers v%s\n", CVI_SYS_API);

    {
        cvi_sys_ver version;
        cvi_sys.fnGetVersion(&version);
        puts(version.version);
    }

    cvi_sys.fnExit();
    cvi_vb.fnExit();

    if (cvi_parse_sensor_config(snrConfig, &cvi_config) != CONFIG_OK)
        HAL_ERROR("cvi_sys", "Can't load sensor config\n");
    cvi_sensor_name(cvi_config.sensor_type, sensor, sizeof(sensor));

    if (ret = cvi_sensor_init(cvi_config.dll_file, cvi_config.sensor_type))
        return ret;

    {
        cvi_vb_pool pool;
        memset(&pool, 0, sizeof(pool));

        pool.count = 3;
        pool.comm[0].blockSize = 3110400;
        pool.comm[0].blockCnt = 3;
        pool.comm[0].rempVirt = 2;
        pool.comm[1].blockSize = 3110400;
        pool.comm[1].blockCnt = 3;
        pool.comm[1].rempVirt = 2;
        pool.comm[2].blockSize = 2764800;
        pool.comm[2].blockCnt = 1;
        pool.comm[2].rempVirt = 2;

        if (ret = cvi_vb.fnConfigPool(&pool))
            return ret;
    }
    if (ret = cvi_vb.fnInit())
        return ret;

    if (ret = cvi_sys.fnInit())
        return ret;

    return EXIT_SUCCESS;
}

int cvi_system_identify(void)
{
    if (access("/proc/cvi", F_OK))
        return 0;

    plat = HAL_PLATFORM_CVI;

    {
        FILE *file = fopen("/proc/meminfo", "r");
        unsigned long kb = 0;
        char line[64];

        if (file) {
            while (fgets(line, sizeof(line), file))
                if (sscanf(line, "MemTotal: %lu kB", &kb) == 1)
                    break;
            fclose(file);
        }
        if (kb >= 262144)
            strcpy(chip, "SG2000");
        else if (kb)
            strcpy(chip, "SG2002");
    }

    strcpy(family, "CV181x");
    chnCount = CVI_VENC_CHN_NUM;
    chnState = (hal_chnstate*)cvi_state;
    aud_thread = cvi_audio_thread;
    isp_thread = cvi_image_thread;
    vid_thread = cvi_video_thread;
    return 1;
}

#endif
