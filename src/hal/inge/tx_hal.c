#include "tx_hal.h"

tx_aud_impl  tx_aud;
tx_fs_impl   tx_fs;
tx_isp_impl  tx_isp;
tx_osd_impl  tx_osd;
tx_sys_impl  tx_sys;
tx_venc_impl tx_venc;

hal_chnstate tx_state[TX_VENC_CHN_NUM] = {0};
int (*tx_venc_cb)(char, hal_vidstream*);

tx_isp_snr _tx_isp_snr;

char _tx_fs_chn = 0;

void tx_hal_deinit(void)
{
    tx_venc_unload(&tx_venc);
    tx_osd_unload(&tx_osd);
    tx_isp_unload(&tx_isp);
    tx_fs_unload(&tx_fs);
    tx_aud_unload(&tx_aud);
    tx_sys_unload(&tx_sys);
}

int tx_hal_init(void)
{
    int ret;

    if (ret = tx_sys_load(&tx_sys))
    return ret;
    if (ret = tx_aud_load(&tx_aud))
    return ret;
    if (ret = tx_fs_load(&tx_fs))
    return ret;
    if (ret = tx_isp_load(&tx_isp))
    return ret;
    if (ret = tx_osd_load(&tx_osd))
    return ret;
    if (ret = tx_venc_load(&tx_venc))
    return ret;

    return EXIT_SUCCESS;
}

int tx_pipeline_create(short width, short height, char framerate)
{
    int ret;

    {
        tx_fs_chn channel = {
            .dest = { .width = width, .height = height },
            .fpsDen = framerate, .fpsNum = 1
        };

        if (ret = tx_fs.fnCreateChannel(_tx_fs_chn, &channel))
            return ret;
    }
}

void tx_pipeline_destroy()
{
    tx_fs.fnDestroyChannel(_tx_fs_chn);
}

void *tx_video_thread(void)
{

}

void tx_system_deinit(void)
{
    tx_sys.fnExit();

    tx_isp.fnDisableSensor();
    tx_isp.fnDeleteSensor(&_tx_isp_snr);
    tx_isp.fnExit();
}

int tx_system_init(char *sensor)
{
    int ret;

    {
        tx_sys_ver version;
        if (ret = tx_sys.fnGetVersion(&version))
            return ret;
        printf("App built with headers v%s\n", TX_SYS_API);
        puts(version.version);
    }

    for (char i = 0; i < sizeof(tx_sensors) / sizeof(*tx_sensors); i++) {
        if (strcmp(sensor, tx_sensors[i].name)) continue;
        _tx_isp_snr = tx_sensors[i];
        ret = 0;
        break;
    }
    if (ret)
        return EXIT_FAILURE;

    if (ret = tx_isp.fnInit())
        return ret;
    if (ret = tx_isp.fnAddSensor(&_tx_isp_snr))
        return ret;
    if (ret = tx_isp.fnEnableSensor())
        return ret;

    if (ret = tx_sys.fnInit())
        return ret;

    return EXIT_SUCCESS;
}