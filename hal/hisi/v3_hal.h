#include "v3_common.h"

v3_isp_impl v3_isp;
v3_sys_impl v3_sys;
v3_vb_impl v3_vb;
v3_venc_impl v3_venc;
v3_vi_impl v3_vi;
v3_vpss_impl v3_vpss;

char isp_dev = 0;
char venc_dev = 0;
int venc_fd[V3_VENC_CHN_NUM] = {0};
char vi_chn = 0;
char vi_dev = 0;

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

int v3_encoder_create(char index, hal_vidconfig config)
{
    
}

int v3_encoder_destroy(char index)
{
    int ret;
    int vpss_grp = index / V3_VPSS_CHN_NUM;
    int vpss_chn = index - vpss_grp * V3_VPSS_CHN_NUM;

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

void *v3_image_thread(void)
{
    if (v3_isp.fnRun(isp_dev))
        printf("[v3_isp] Shutting down ISP thread...\n");
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

int v3_pipeline_create(void)
{

}

void v3_system_deinit(void)
{
    v3_sys.fnExit();
    
    v3_vb.fnExit();
}

void v3_system_init(void)
{
    v3_sys_ver version;
    v3_sys.fnGetVersion(&version);
    printf("App built with headers v%s\n", V3_SYS_API);
    printf("MPP version: %s\n", version.version);
}