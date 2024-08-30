#ifdef __arm__

#include "ak_hal.h"

ak_aud_impl  ak_aud;
ak_sys_impl  ak_sys;
ak_venc_impl ak_venc;
ak_vi_impl   ak_vi;

hal_chnstate ak_state[AK_VENC_CHN_NUM] = {0};
int (*ak_aud_cb)(hal_audframe*);
int (*ak_vid_cb)(char, hal_vidstream*);

ak_vi_cnf _ak_vi_cnf;

void *_ak_vi_dev;
void *_ak_venc_dev[AK_VENC_CHN_NUM];
void *_ak_venc_strm[AK_VENC_CHN_NUM];

void ak_hal_deinit(void)
{
    ak_vi_unload(&ak_vi);
    ak_venc_unload(&ak_venc);
    ak_aud_unload(&ak_aud);
    ak_sys_unload(&ak_sys);
}

int ak_hal_init(void)
{
    int ret;

    if (ret = ak_sys_load(&ak_sys))
        return ret;
    if (ret = ak_aud_load(&ak_aud))
        return ret;
    if (ret = ak_venc_load(&ak_venc))
        return ret;
    if (ret = ak_vi_load(&ak_vi))
        return ret;

    return EXIT_SUCCESS;
}

int ak_channel_bind(char index)
{
    if (!(_ak_venc_strm[index] =
        ak_venc.fnBindChannel(_ak_vi_dev, _ak_venc_dev[index])))
        return EXIT_FAILURE;

    return EXIT_SUCCESS;
}

int ak_channel_grayscale(char enable)
{
    return ak_vi.fnSetDeviceMode(_ak_vi_dev, enable & 1);
}

int ak_channel_unbind(char index)
{
    int ret = ak_venc.fnUnbindChannel(_ak_venc_strm[index]);

    _ak_venc_strm[index] = NULL;

    return ret;
}

int ak_pipeline_create(char mirror, char flip)
{
    int ret;

    if (!(_ak_vi_dev = ak_vi.fnEnableDevice(0)))
        return EXIT_FAILURE;

    if (ret = ak_vi.fnGetSensorResolution(_ak_vi_dev, _ak_vi_cnf.dest))
        return ret;

    _ak_vi_cnf.capt.x = 0;
    _ak_vi_cnf.capt.y = 0;
    _ak_vi_cnf.capt.width = _ak_vi_cnf.dest[0].width;
    _ak_vi_cnf.capt.height = _ak_vi_cnf.dest[0].height;
    if (ret = ak_vi.fnSetDeviceConfig(_ak_vi_dev, &_ak_vi_cnf))
        return ret;

    if (ret = ak_vi.fnStartDevice(_ak_vi_dev))
        return ret;

    if (ret = ak_vi.fnSetDeviceFlipMirror(_ak_vi_dev, flip & 1, mirror & 1))
        return ret;

    return EXIT_SUCCESS;
}

void ak_pipeline_destroy(void)
{
    ak_vi.fnStopDevice(_ak_vi_dev);
    ak_vi.fnDisableDevice(_ak_vi_dev);
}

int ak_video_create(char index, hal_vidconfig *config)
{
    int ret;

    ak_venc_codec codec;
    char ratemode;
    ak_venc_prof profile;

    switch (config->mode) {
        case HAL_VIDMODE_CBR: ratemode = 0; break;
        case HAL_VIDMODE_VBR: ratemode = 1; break;
        default: HAL_ERROR("ak_venc", "Video encoder does not support this mode!");
    }
    switch (config->codec) {
        case HAL_VIDCODEC_JPG:
        case HAL_VIDCODEC_MJPG:
            codec = AK_VENC_CODEC_MJPG;
            profile = AK_VENC_PROF_HEVC_MAINSTILL;
            break;
        case HAL_VIDCODEC_H265:
            codec = AK_VENC_CODEC_H265;
            profile = AK_VENC_PROF_HEVC_MAIN;
            break;
        case HAL_VIDCODEC_H264:
            codec = AK_VENC_CODEC_H264;
            switch (config->profile) {
                case HAL_VIDPROFILE_BASELINE: profile = AK_VENC_PROF_BASE; break;
                case HAL_VIDPROFILE_MAIN: profile = AK_VENC_PROF_MAIN; break;
                default: profile = AK_VENC_PROF_HIGH; break;
            } break;
        default: HAL_ERROR("ak_venc", "This codec is not supported by the hardware!");
    }

    {
        ak_venc_cnf channel = {.codec = codec, .width = config->width, .height = config->height,
            .minQual = config->minQual, .maxQual = config->maxQual, .dstFps = config->framerate,
            .gop = config->gop, .maxBitrate = config->maxBitrate, .profile = profile, .subChnOn = index,
            .output = index ? AK_VENC_OUT_SUBSTRM : AK_VENC_OUT_MAINSTRM, .vbrModeOn = ratemode};

        if (!(_ak_venc_dev[index] = ak_venc.fnEnableChannel(&channel)))
            HAL_ERROR("ak_venc", "Creating channel %d failed with %#x!\n%s\n",
                index, ret = ak_sys.fnGetErrorNum(), ak_sys.fnGetErrorStr(ret));
    }

    ak_state[index].payload = config->codec;

    return EXIT_SUCCESS;
}

int ak_video_destroy(char index)
{
    if (ak_venc.fnDisableChannel(_ak_venc_dev[index]))
        return EXIT_FAILURE;

    return EXIT_SUCCESS;
}

int ak_video_destroy_all(void)
{
    int ret;

    for (char i = 0; i < AK_VENC_CHN_NUM; i++)
        if (ak_state[i].enable)
            if (ret = ak_video_destroy(i))
                return ret;

    return EXIT_SUCCESS;
}

void ak_video_request_idr(char index)
{
    ak_venc.fnRequestIdr(_ak_venc_dev[index]);
}

void *ak_video_thread(void)
{
    int ret;

    while (keepRunning) {
        for (int i = 0; i < AK_VENC_CHN_NUM; i++) {
            if (!ak_state[i].enable) continue;
            if (!ak_state[i].mainLoop) continue;

            ak_venc_strm stream;
            if (ak_venc.fnGetStream(_ak_venc_strm[i], &stream)) {
                ret = ak_sys.fnGetErrorNum();
                HAL_DANGER("ak_venc", "Getting the stream on "
                    "channel %d failed with %#x!\n%s\n", i, ret,
                    ak_sys.fnGetErrorStr(ret));
            };
            if (!stream.length) continue;

            if (ak_vid_cb) {
                hal_vidstream outStrm;
                hal_vidpack outPack[1];
                memset(outPack, 0, sizeof(outPack));
                outStrm.count = 1;
                outStrm.seq = stream.sequence;
                outPack[0].data = stream.data;
                outPack[0].length = stream.length;
                outPack[0].naluCnt = 1;
                outPack[0].nalu[0].length = outPack[0].length;
                outPack[0].nalu[0].offset = 0;
                outPack[0].nalu[0].type = stream.naluType;
                outPack[0].offset = 0;
                outPack[0].timestamp = stream.timestamp;
                outStrm.pack = outPack;
                (*ak_vid_cb)(i, &outStrm);

                if (ak_venc.fnFreeStream(_ak_venc_strm[i], &stream)) {
                    HAL_DANGER("ak_venc", "Releasing the stream on "
                        "channel %d failed!\n", i);
                }
            }
        }
    }
    HAL_INFO("ak_venc", "Shutting down encoding thread...\n");
}

void ak_system_deinit(void)
{

}

int ak_system_init(char *snrConfig)
{
    int ret;

    if (ret = ak_vi.fnLoadSensorConfig(snrConfig))
        return ret;

    return EXIT_SUCCESS;
}

#endif