#if 0

#include "ak_hal.h"

ak_aud_impl  ak_aud;
ak_venc_impl ak_venc;
ak_vi_impl   ak_vi;

hal_chnstate ak_state[AK_VENC_CHN_NUM] = {0};
int (*ak_aud_cb)(hal_audframe*);
int (*ak_vid_cb)(char, hal_vidstream*);

void *_ak_vi_dev;

void ak_hal_deinit(void)
{
    ak_vi_unload(&ak_vi);
    ak_venc_unload(&ak_venc);
    ak_aud_unload(&ak_aud);
}

int ak_hal_init(void)
{
    int ret;

    if (ret = ak_aud_load(&ak_aud))
        return ret;
    if (ret = ak_venc_load(&ak_venc))
        return ret;
    if (ret = ak_vi_load(&ak_vi))
        return ret;

    return EXIT_SUCCESS;
}

int ak_channel_grayscale(char enable)
{
    return ak_vi.fnSetDeviceMode(_ak_vi_dev, enable & 1);
}

#endif