#pragma once

#include "v4_common.h"
#include "v4_isp.h"
#include "v4_snr.h"
#include "v4_vi.h"

#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../config.h"
#include "../tools.h"

typedef struct {
    // [sensor]
    char sensor_type[128];
    v4_common_wdr mode;
    char dll_file[256];

    // [mode]
    v4_snr_input input_mode;

    // [mipi]
    v4_snr_mipi mipi;

    // [lvds]
    v4_snr_lvds lvds;

    // [isp_image]
    v4_isp_dev isp;

    // [vi_dev]
    v4_vi_dev videv;
} v4_config_impl;

extern v4_config_impl v4_config;

static enum ConfigError v4_parse_config_lvds(
    struct IniConfig *ini, const char *section, v4_snr_lvds *lvds) {
    enum ConfigError err;
    {
        const char *possible_values[] = {
            "HI_WDR_MODE_NONE",  "HI_WDR_MODE_2F",     "HI_WDR_MODE_3F",
            "HI_WDR_MODE_4F",    "HI_WDR_MODE_DOL_2F", "HI_WDR_MODE_DOL_3F",
            "HI_WDR_MODE_DOL_4F"};
        const int count = sizeof(possible_values) / sizeof(const char *);
        err = parse_enum(
            ini, section, "wdr_mode", (void*)&lvds->wdr, possible_values,
            count, 0);
        if (err != CONFIG_OK)
            return err;
    }
    {
        const char *possible_values[] = {
            "LVDS_SYNC_MODE_SOL", "LVDS_SYNC_MODE_SAV"};
        const int count = sizeof(possible_values) / sizeof(const char *);
        err = parse_enum(
            ini, section, "sync_mode", (void*)&lvds->syncSavOn,
            possible_values, count, 0);
        if (err != CONFIG_OK)
            return err;
    }
    {
        const char *possible_values[] = {
            "RAW_DATA_8BIT",                "RAW_DATA_10BIT",       "RAW_DATA_12BIT",
            "RAW_DATA_14BIT",               "RAW_DATA_16BIT",       "RAW_DATA_YUV420_8BIT_NORMAL",
            "RAW_DATA_YUV420_8BIT_LEGACY",  "RAW_DATA_YUV422_8BIT"};
        const int count = sizeof(possible_values) / sizeof(const char *);
        err = parse_enum(
            ini, section, "raw_data_type", (void*)&lvds->prec,
            possible_values, count, 1);
        if (err != CONFIG_OK)
            return err;
    }
    {
        const char *possible_values[] = {
            "LVDS_ENDIAN_LITTLE", "LVDS_ENDIAN_BIG"};
        const int count = sizeof(possible_values) / sizeof(const char *);
        err = parse_enum(
            ini, section, "data_endian", (void*)&lvds->dataBeOn,
            possible_values, count, 1);
        if (err != CONFIG_OK)
            return err;
        err = parse_enum(
            ini, section, "sync_code_endian", (void*)&lvds->syncBeOn,
            possible_values, count, 1);
        if (err != CONFIG_OK)
            return err;
    }
    int laneId[4];
    err = parse_array(ini, section, "lane_id", laneId, 4);
    if (err != CONFIG_OK)
        return err;
    else for (char i = 0; i < 4; i++)
        lvds->laneId[i] = (short)laneId[i];
    char syncname[16];
    int synccode[128];
    for (int i = 0; i < 8; i++) {
        sprintf(syncname, "sync_code_%d", i);
        err = parse_array(ini, section, syncname, synccode + i * 16, 16);
        if (err != CONFIG_OK)
            return err;
    }
    for (int j = 0; j < 32; j++)
        lvds->syncCode[j] = (unsigned short)synccode[j];
    return CONFIG_OK;
}

static enum ConfigError v4_parse_config_videv(
    struct IniConfig *ini, const char *section, v4_vi_dev *device) {
    enum ConfigError err;
    memset(device, 0, sizeof(*device));
    {
        const char *possible_values[] = {
            "VI_MODE_BT656",            "VI_MODE_BT656_PACKED_YUV",
            "VI_MODE_BT601",            "VI_MODE_DIGITAL_CAMERA",
            "VI_MODE_BT1120_STANDARD",  "VI_MODE_BT1120_INTERLEAVED",
            "VI_MODE_MIPI",             "VI_MODE_MIPI_YUV420_NORMAL",
            "VI_MODE_YUV420_LEGACY",    "VI_MODE_MIPI_YUV422",
            "VI_MODE_LVDS",             "VI_MODE_HISPI",
            "VI_MODE_SLVS"};
        const int count = sizeof(possible_values) / sizeof(const char *);
        err = parse_enum(
            ini, section, "input_mod", (void*)&device->intf,
            possible_values, count, 0);
        if (err != CONFIG_OK)
            return err;
    }
    {
        const char *possible_values[] = {
            "VI_WORK_MODE_1Multiplex", "VI_WORK_MODE_2Multiplex",
            "VI_WORK_MODE_4Multiplex"};
        const int count = sizeof(possible_values) / sizeof(const char *);
        err = parse_enum(
            ini, section, "work_mod", (void*)&device->work, possible_values,
            count, 0);
        if (err != CONFIG_OK)
            device->work = V4_VI_WORK_1MULTIPLEX;
    }
    err = parse_uint32(ini, section, "mask_0", 0, UINT_MAX - 1, &device->cmpntMask[0]);
    if (err != CONFIG_OK)
        return err;
    err = parse_uint32(ini, section, "mask_1", 0, UINT_MAX - 1, &device->cmpntMask[1]);
    if (err != CONFIG_OK)
        return err;
    unsigned int maskNum;
    err = parse_uint32(ini, section, "mask_num", 0, 2, &maskNum);
    if (err != CONFIG_OK)
        return err;
    if (maskNum < 1) device->cmpntMask[1] = UINT_MAX;
    if (maskNum < 2) device->cmpntMask[0] = UINT_MAX;
    {
        const char *possible_values[] = {
            "VI_SCAN_INTERLACED", "VI_SCAN_PROGRESSIVE"};
        const int count = sizeof(possible_values) / sizeof(const char *);
        err = parse_enum(
            ini, section, "scan_mode", (void*)&device->progressiveOn,
            possible_values, count, 0);
        if (err != CONFIG_OK)
            return err;
    }
    {
        const char *possible_values[] = {
            "VI_DATA_SEQ_VUVU", "VI_DATA_SEQ_UVUV",
            "VI_DATA_SEQ_UYVY", "VI_DATA_SEQ_VYUY",
            "VI_DATA_SEQ_YUYV", "VI_DATA_SEQ_YVYU"};
            const int count = sizeof(possible_values) / sizeof(const char *);
            err = parse_enum(
                ini, section, "data_seq", (void*)&device->seq,
                possible_values, count, 0);
            if (err != CONFIG_OK)
                return err;
    }
    {
        const char *possible_values[] = {"VI_VSYNC_FIELD", "VI_VSYNC_PULSE"};
        const int count = sizeof(possible_values) / sizeof(const char *);
        err = parse_enum(
            ini, section, "vsync", (void*)&device->sync.vsyncPulse, possible_values,
            count, 0);
        if (err != CONFIG_OK)
            return err;
    }
    {
        const char *possible_values[] = {
            "VI_VSYNC_NEG_HIGH", "VI_VSYNC_NEG_LOW"};
        const int count = sizeof(possible_values) / sizeof(const char *);
        err = parse_enum(
            ini, section, "vsyncneg", (void*)&device->sync.vsyncInv,
            possible_values, count, 0);
        if (err != CONFIG_OK)
            return err;
    }
    {
        const char *possible_values[] = {
            "VI_HSYNC_VALID_SIGNAL", "VI_HSYNC_PULSE"};
        const int count = sizeof(possible_values) / sizeof(const char *);
        err = parse_enum(
            ini, section, "hsync", (void*)&device->sync.hsyncPulse, possible_values,
            count, 0);
        if (err != CONFIG_OK)
            return err;
    }
    {
        const char *possible_values[] = {
            "VI_HSYNC_NEG_HIGH", "VI_HSYNC_NEG_LOW"};
        const int count = sizeof(possible_values) / sizeof(const char *);
        err = parse_enum(
            ini, section, "hsyncneg", (void*)&device->sync.hsyncInv,
            possible_values, count, 0);
        if (err != CONFIG_OK)
            return err;
    }
    {
        const char *possible_values[] = {
            "VI_VSYNC_NORM_PULSE", "VI_VSYNC_VALID_SIGNAL"};
        const int count = sizeof(possible_values) / sizeof(const char *);
        err = parse_enum(
            ini, section, "vsyncvalid", (void*)&device->sync.vsyncValid,
            possible_values, count, 0);
        if (err != CONFIG_OK)
            return err;
    }
    {
        const char *possible_values[] = {
            "VI_VSYNC_VALID_NEG_HIGH", "VI_VSYNC_VALID_NEG_LOW"};
        const int count = sizeof(possible_values) / sizeof(const char *);
        err = parse_enum(
            ini, section, "vsyncvalidneg", (void*)&device->sync.vsyncValidInv,
            possible_values, count, 0);
        if (err != CONFIG_OK)
            return err;
    }
    err = parse_int(
        ini, section, "timingblank_hsynchfb", 0, INT_MAX,
        &device->sync.timing.hsyncFront);
    if (err != CONFIG_OK)
        return err;
    err = parse_int(
        ini, section, "timingblank_hsyncact", 0, INT_MAX,
        &device->sync.timing.hsyncWidth);
    if (err != CONFIG_OK)
        return err;
    err = parse_int(
        ini, section, "timingblank_hsynchbb", 0, INT_MAX,
        &device->sync.timing.hsyncBack);
    if (err != CONFIG_OK)
        return err;
    err = parse_int(
        ini, section, "timingblank_vsyncvfb", 0, INT_MAX,
        &device->sync.timing.vsyncFront);
    if (err != CONFIG_OK)
        return err;
    err = parse_int(
        ini, section, "timingblank_vsyncvact", 0, INT_MAX,
        &device->sync.timing.vsyncWidth);
    if (err != CONFIG_OK)
        return err;
    err = parse_int(
        ini, section, "timingblank_vsyncvbb", 0, INT_MAX,
        &device->sync.timing.vsyncBack);
    if (err != CONFIG_OK)
        return err;
    err = parse_int(
        ini, section, "timingblank_vsyncvbfb", 0, INT_MAX,
        &device->sync.timing.vsyncIntrlFront);
    if (err != CONFIG_OK)
        return err;
    err = parse_int(
        ini, section, "timingblank_vsyncvbact", 0, INT_MAX,
        &device->sync.timing.vsyncIntrlWidth);
    if (err != CONFIG_OK)
        return err;
    err = parse_int(
        ini, section, "timingblank_vsyncvbbb", 0, INT_MAX,
        &device->sync.timing.vsyncIntrlBack);
    if (err != CONFIG_OK)
        return err;
    err = parse_int(ini, section, "datarev", 0, INT_MAX, &device->dataRevOn);
    if (err != CONFIG_OK)
        return err;
    err = parse_int(ini, section, "devrect_x", 0, INT_MAX, &v4_config.isp.capt.x);
    if (err != CONFIG_OK)
        return err;
    err = parse_int(ini, section, "devrect_y", 0, INT_MAX, &v4_config.isp.capt.y);
    if (err != CONFIG_OK)
        return err;
    err = parse_int(ini, section, "devrect_w", 0, INT_MAX, &v4_config.isp.capt.width);
    if (err != CONFIG_OK)
        return err;
    err = parse_int(ini, section, "devrect_h", 0, INT_MAX, &v4_config.isp.capt.height);
    if (err != CONFIG_OK)
        return err;
    
    if (device->intf == V4_VI_INTF_MIPI || device->intf == V4_VI_INTF_LVDS)
        device->rgbModeOn = 1;
    else device->rgbModeOn = 0;

    return CONFIG_OK;
}

static enum ConfigError v4_parse_config_isp(
    struct IniConfig *ini, const char *section, v4_isp_dev *isp) {
    enum ConfigError err;

    int value;
    err = parse_int(
        ini, "isp_image", "isp_framerate", 0, INT_MAX, &value);
    if (err != CONFIG_OK)
        return err;
    else isp->framerate = value * 1.0f;
    {
        const char *possible_values[] = {
            "BAYER_RGGB", "BAYER_GRBG", "BAYER_GBRG", "BAYER_BGGR"};
        const int count = sizeof(possible_values) / sizeof(const char *);
        err = parse_enum(
            ini, "isp_image", "isp_bayer", (void*)&isp->bayer,
            possible_values, count, 0);
        if (err != CONFIG_OK)
            return err;
    }
    return CONFIG_OK;
}

static enum ConfigError v4_parse_sensor_config(char *path, v4_config_impl *config) {
    if (config == NULL)
        return (enum ConfigError)-1;
    memset(config, 0, sizeof(config));
    struct IniConfig ini;
    memset(&ini, 0, sizeof(ini));
    enum ConfigError err;

    // load config file to string
    ini.str = NULL;
    {
        FILE *file = fopen(path, "rb");
        if (!file) {
            printf("Can't open file %s\n", path);
            return (enum ConfigError)-1;
        }

        fseek(file, 0, SEEK_END);
        size_t length = (size_t)ftell(file);
        fseek(file, 0, SEEK_SET);

        ini.str = (char*)malloc(length + 1);
        if (!ini.str) {
            printf("Can't allocate buf in parse_sensor_config\n");
            fclose(file);
            return (enum ConfigError)-1;
        }
        size_t n = fread(ini.str, 1, length, file);
        if (n != length) {
            printf("Can't read all file %s\n", path);
            fclose(file);
            free(ini.str);
            return (enum ConfigError)-1;
        }
        fclose(file);
        ini.str[length] = 0;
    }

    find_sections(&ini);

    // [sensor]
    err = parse_param_value(&ini, "sensor", "sensor_type", config->sensor_type);
    if (err != CONFIG_OK)
        goto RET_ERR;
    {
        const char *possible_values[] = {
            "WDR_MODE_NONE",
            "WDR_MODE_BUILT_IN",
            "WDR_MODE_QUDRA",
            "WDR_MODE_2To1_LINE",
            "WDR_MODE_2To1_FRAME",
            "WDR_MODE_2To1_FRAME_FULL_RATE",
            "WDR_MODE_3To1_LINE",
            "WDR_MODE_3To1_FRAME",
            "WDR_MODE_3To1_FRAME_FULL_RATE",
            "WDR_MODE_4To1_LINE",
            "WDR_MODE_4To1_FRAME",
            "WDR_MODE_4To1_FRAME_FULL_RATE"};
        const int count = sizeof(possible_values) / sizeof(const char *);
        err = parse_enum(
            &ini, "sensor", "mode", (void*)&config->mode, possible_values,
            count, 0);
        if (err != CONFIG_OK)
            config->mode = V4_WDR_NONE;
    }
    err = parse_param_value(&ini, "sensor", "dllfile", config->dll_file);
    if (err != CONFIG_OK)
        goto RET_ERR;

    // [mode]
    {
        const char *possible_values[] = {
            "INPUT_MODE_MIPI",   "INPUT_MODE_SUBLVDS",  "INPUT_MODE_LVDS",
            "INPUT_MODE_HISPI",  "INPUT_MODE_CMOS",     "INPUT_MODE_BT601",
            "INPUT_MODE_BT656",  "INPUT_MODE_BT1120",   "INPUT_MODE_BYPASS"};
        const int count = sizeof(possible_values) / sizeof(const char *);
        err = parse_enum(
            &ini, "mode", "input_mode", (void*)&config->input_mode,
            possible_values, count, 0);
        if (err != CONFIG_OK)
            config->input_mode = V4_SNR_INPUT_MIPI;
    }

    if (config->input_mode == V4_SNR_INPUT_MIPI) {
        // [mipi]
        {
            int rawBitness;
            parse_int(&ini, "mode", "raw_bitness", 0, INT_MAX, &rawBitness);
            switch (rawBitness) {
                case 8:  config->mipi.prec = V4_PREC_8BPP;  break;
                case 10: config->mipi.prec = V4_PREC_10BPP; break;
                case 12: config->mipi.prec = V4_PREC_12BPP; break;
                case 14: config->mipi.prec = V4_PREC_14BPP; break;
                case 16: config->mipi.prec = V4_PREC_16BPP; break;
                default: config->mipi.prec = V4_PREC_10BPP; break;
            }
        }
        int laneId[4];
        err = parse_array(&ini, "mipi", "lane_id", (int*)&laneId, 4);
        if (err != CONFIG_OK)
            goto RET_ERR;
        else for (char i = 0; i < 4; i++)
            config->mipi.laneId[i] = laneId[i];
    } else if (config->input_mode == V4_SNR_INPUT_LVDS) {
        // [lvds]
        err = v4_parse_config_lvds(&ini, "lvds", &config->lvds);
        if (err != CONFIG_OK)
            goto RET_ERR;
    }

    // [isp_image]
    err = v4_parse_config_isp(&ini, "isp_image", &config->isp);
    if (err != CONFIG_OK)
        goto RET_ERR;

    // [vi_dev]
    err = v4_parse_config_videv(&ini, "vi_dev", &config->videv);
    if (err != CONFIG_OK)
        goto RET_ERR;

    for (char i = 0; i < 4; i++)
        config->videv.adChn[i] = -1;

    // Fallbacks for default sensor configuration files
    if (!config->isp.size.width)
        config->isp.size.width = config->isp.capt.width;
    if (!config->isp.size.height)
        config->isp.size.height = config->isp.capt.height;

    if (!config->videv.size.width)
        config->videv.size.width = config->isp.capt.width;
    if (!config->videv.size.height)
        config->videv.size.height = config->isp.capt.height;

    if (!config->videv.bayerSize.width)
        config->videv.bayerSize.width = config->isp.capt.width;
    if (!config->videv.bayerSize.height)
        config->videv.bayerSize.height = config->isp.capt.height;

    config->videv.wdrCacheLine = v4_config.isp.capt.height;

    if (!config->isp.wdr)
        config->isp.wdr = config->mode;

    v4_config = *config;
    free(ini.str);
    return CONFIG_OK;
RET_ERR:
    free(ini.str);
    return err;
}