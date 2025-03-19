#pragma once

#include "i6_common.h"

#if 0
#define MI_ISP_BOOL_e char
#define MI_U8 unsigned char
#define MI_U16 unsigned short
#define MI_S32 int
#define MI_U32 unsigned int
#define MS_U32 unsigned int

typedef enum

{

    SS_OP_TYP_AUTO = 0,

    SS_OP_TYP_MANUAL = !SS_OP_TYP_AUTO,

    SS_OP_TYP_MODE_MAX

} MI_ISP_OP_TYPE_e;

typedef struct NR3D_PARAM_s

{

    MI_U8 u8MdThd;

    MI_U16 u16MdGain;

    MI_U8 u8TfStr;

    MI_U8 u8TfStrEx;

    MI_U8 u8MdThdByY[16];

    MI_U8 u8MdGainByY[16];

    MI_U8 u8M2SLut[16];

    MI_U16 u16TfLut[16];

    MI_U8 u8YSfBlendLut[16];

    MI_U8 u8YSfBlendOffset;

} NR3D_PARAM_t;

typedef struct NR3D_MANUAL_ATTR_s

{

    NR3D_PARAM_t stParaAPI;

} NR3D_MANUAL_ATTR_t;

#define MI_ISP_AUTO_NUM 16
typedef struct NR3D_AUTO_ATTR_s

{

    NR3D_PARAM_t stParaAPI[MI_ISP_AUTO_NUM];

} NR3D_AUTO_ATTR_t;

typedef struct MI_ISP_IQ_NR3D_TYPE_s

{

    MI_ISP_BOOL_e bEnable;

    MI_ISP_OP_TYPE_e  enOpType;

    NR3D_AUTO_ATTR_t stAuto;

    NR3D_MANUAL_ATTR_t stManual;

} MI_ISP_IQ_NR3D_TYPE_t;

typedef enum

{

    SS_ISP_STATE_NORMAL = 0,

    SS_ISP_STATE_PAUSE = 1,

    SS_ISP_STATE_MAX

} MI_ISP_SM_STATE_TYPE_e;

typedef struct MWB_ATTR_PARAM_s

{

    MI_U16 u16Rgain;

    MI_U16 u16Grgain;

    MI_U16 u16Gbgain;

    MI_U16 u16Bgain;

} MWB_ATTR_PARAM_t;

typedef enum

{

    SS_AWB_ALG_GRAYWORLD = 0,

    SS_AWB_ALG_NORMAL = 1,

    SS_AWB_ALG_BALANCE =2,

    SS_AWB_ALG_FOCUS =3,

    SS_AWB_ALG_MAX

} MI_ISP_AWB_ALG_TYPE_e;

typedef enum

{

    SS_AWB_ADV_DEFAULT = 0,

    SS_AWB_ADV_ADVANCE = 1,

    SS_AWB_ADV_MAX

} MI_ISP_AWB_ADV_TYPE_e;



typedef struct CT_LIMIT_PARAM_s

{

    MI_U16 u16MaxRgain;

    MI_U16 u16MinRgain;

    MI_U16 u16MaxBgain;

    MI_U16 u16MinBgain;

} CT_LIMIT_PARAM_t;


#define MI_ISP_AWB_CT_TBL_NUM 10
#define MI_ISP_AWB_LV_CT_TBL_NUM 18
#define MI_ISP_AWB_WEIGHT_WIN_NUM 4

typedef enum

{

    SS_AWB_MODE_AUTO,

    SS_AWB_MODE_MANUAL,

    SS_AWB_MODE_CTMANUAL,

    SS_AWB_MODE_MAX

} MI_ISP_AWB_MODE_TYPE_e;

typedef struct CT_WEIGHT_PARAM_s

{

    MI_U16 u16Weight[MI_ISP_AWB_CT_TBL_NUM];

} CT_WEIGHT_PARAM_t;

typedef struct CT_RATIO_PARAM_s

{

    MI_U16 u16Ratio[MI_ISP_AWB_CT_TBL_NUM];

} CT_RATIO_PARAM_t;

typedef struct AWB_ATTR_PARAM_s

{

    MI_U8 u8Speed;

    MI_U8 u8ConvInThd;

    MI_U8 u8ConvOutThd;

    MI_ISP_AWB_ALG_TYPE_e eAlgType;

    MI_ISP_AWB_ADV_TYPE_e eAdvType;

    MI_U8 u8RGStrength;

    MI_U8 u8BGStrength;

    CT_LIMIT_PARAM_t stCTLimit;

    CT_WEIGHT_PARAM_t stLvWeight[MI_ISP_AWB_LV_CT_TBL_NUM];

    CT_RATIO_PARAM_t stPreferRRatio[MI_ISP_AWB_LV_CT_TBL_NUM];

    CT_RATIO_PARAM_t 
    stPreferBRatio[MI_ISP_AWB_LV_CT_TBL_NUM];

    MI_U16 u16WpWeight[MI_ISP_AWB_CT_TBL_NUM];

    MS_U32 u4WeightWin[MI_ISP_AWB_WEIGHT_WIN_NUM];

} AWB_ATTR_PARAM_t;


typedef struct MI_ISP_AWB_ATTR_TYPE_s

{

    MI_ISP_SM_STATE_TYPE_e  eState;

    MI_ISP_AWB_MODE_TYPE_e    eOpType;

    MWB_ATTR_PARAM_t        stManualParaAPI;

    AWB_ATTR_PARAM_t        stAutoParaAPI;

} MI_ISP_AWB_ATTR_TYPE_t;



MI_S32 MI_ISP_IQ_SetNR3D(MI_U32 Channel, MI_ISP_IQ_NR3D_TYPE_t *data);
MI_S32 MI_ISP_IQ_GetNR3D(MI_U32 Channel, MI_ISP_IQ_NR3D_TYPE_t *data);
MI_S32 MI_ISP_AE_SetState(MI_U32 Channel, MI_ISP_SM_STATE_TYPE_e *data);
MI_S32 MI_ISP_AE_GetState(MI_U32 Channel, MI_ISP_SM_STATE_TYPE_e *data);
MI_S32 MI_ISP_AWB_SetAttr(MI_U32 Channel, MI_ISP_AWB_ATTR_TYPE_t *data);
MI_S32 MI_ISP_AWB_GetAttr(MI_U32 Channel, MI_ISP_AWB_ATTR_TYPE_t *data);
#endif

typedef struct 

{
    unsigned int args[13];
} cus3AEnable_t;



typedef struct {
    void *handle, *handleCus3a, *handleIspAlgo;

    int (*fnLoadChannelConfig)(int channel, char *path, unsigned int key);
    int (*fnSetColorToGray)(int channel, char *enable);
    int (*fnDisableUserspace3A)(int channel);
    int (*fnCUS3AEnable)(int channel, cus3AEnable_t *data);
} i6_isp_impl;

static int i6_isp_load(i6_isp_impl *isp_lib) {
    isp_lib->handleIspAlgo = dlopen("libispalgo.so", RTLD_LAZY | RTLD_GLOBAL);

    isp_lib->handleCus3a = dlopen("libcus3a.so", RTLD_LAZY | RTLD_GLOBAL);

    if (!(isp_lib->handle = dlopen("libmi_isp.so", RTLD_LAZY | RTLD_GLOBAL)))
        HAL_ERROR("i6_isp", "Failed to load library!\nError: %s\n", dlerror());

    if (!(isp_lib->fnLoadChannelConfig = (int(*)(int channel, char *path, unsigned int key))
        hal_symbol_load("i6_isp", isp_lib->handle, "MI_ISP_API_CmdLoadBinFile")))
        return EXIT_FAILURE;

    if (!(isp_lib->fnSetColorToGray = (int(*)(int channel, char *enable))
        hal_symbol_load("i6_isp", isp_lib->handle, "MI_ISP_IQ_SetColorToGray")))
        return EXIT_FAILURE;

    if (!(isp_lib->fnDisableUserspace3A = (int(*)(int channel))
        hal_symbol_load("i6_isp", isp_lib->handle, "MI_ISP_DisableUserspace3A")))
        return EXIT_FAILURE;

    if (!(isp_lib->fnCUS3AEnable = (int(*)(int channel, cus3AEnable_t *data))
        hal_symbol_load("i6_isp", isp_lib->handle, "MI_ISP_CUS3A_Enable")))
        return EXIT_FAILURE;


    return EXIT_SUCCESS;
}

static void i6_isp_unload(i6_isp_impl *isp_lib) {
    if (isp_lib->handle) dlclose(isp_lib->handle);
    isp_lib->handle = NULL;
    if (isp_lib->handleCus3a) dlclose(isp_lib->handleCus3a);
    isp_lib->handleCus3a = NULL;
    if (isp_lib->handleIspAlgo) dlclose(isp_lib->handleIspAlgo);
    isp_lib->handleIspAlgo = NULL;
    memset(isp_lib, 0, sizeof(*isp_lib));
}