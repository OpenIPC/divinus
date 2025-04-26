#include "error.h"

#include <stdio.h>

char *errstr(int error) {
    int level;
    int module = (error >> 16) & 0xFF;

    switch (plat) {
#if defined(__ARM_PCS_VFP)
        case HAL_PLATFORM_I6:
            level = (error >> 12) & 0xF;
            error = error & 0xFF000FFF | (level > 0 ? (4 << 13) : 0);
            switch (module) {
                case I6_SYS_MOD_SYS:
                    error |= (V4_SYS_MOD_SYS << 16); break;
                case I6_SYS_MOD_ISP:
                    error |= (V4_SYS_MOD_ISP << 16); break;
                case I6_SYS_MOD_VIF:
                    error |= (V4_SYS_MOD_VIU << 16); break;
                case I6_SYS_MOD_VPE:
                    error |= (V4_SYS_MOD_VPSS << 16); break;
                case I6_SYS_MOD_VENC:
                    error |= (V4_SYS_MOD_VENC << 16); break;
                case I6_SYS_MOD_RGN:
                    error |= (V4_SYS_MOD_RGN << 16); break;
                case I6_SYS_MOD_AI:
                    error |= (V4_SYS_MOD_AI << 16); break;
                default:
                    error |= (module << 16); break;
            }
            break;
        case HAL_PLATFORM_I6C:
            level = (error >> 12) & 0xF;
            error = error & 0xFF000FFF | (level > 0 ? (4 << 13) : 0);
            switch (module) {
                case I6C_SYS_MOD_SYS:
                    error |= (V4_SYS_MOD_SYS << 16); break;
                case I6C_SYS_MOD_ISP:
                    error |= (V4_SYS_MOD_ISP << 16); break;
                case I6C_SYS_MOD_VIF:
                    error |= (V4_SYS_MOD_VIU << 16); break;
                case I6C_SYS_MOD_SCL:
                    error |= (V4_SYS_MOD_VPSS << 16); break;
                case I6C_SYS_MOD_VENC:
                    error |= (V4_SYS_MOD_VENC << 16); break;
                case I6C_SYS_MOD_RGN:
                    error |= (V4_SYS_MOD_RGN << 16); break;
                case I6C_SYS_MOD_AI:
                    error |= (V4_SYS_MOD_AI << 16); break;
                default:
                    error |= (module << 16); break;
            }
            break;
        case HAL_PLATFORM_M6:
            level = (error >> 12) & 0xF;
            error = error & 0xFF000FFF | (level > 0 ? (4 << 13) : 0);
            switch (module) {
                case M6_SYS_MOD_SYS:
                    error |= (V4_SYS_MOD_SYS << 16); break;
                case M6_SYS_MOD_ISP:
                    error |= (V4_SYS_MOD_ISP << 16); break;
                case M6_SYS_MOD_VIF:
                    error |= (V4_SYS_MOD_VIU << 16); break;
                case M6_SYS_MOD_SCL:
                    error |= (V4_SYS_MOD_VPSS << 16); break;
                case M6_SYS_MOD_VENC:
                    error |= (V4_SYS_MOD_VENC << 16); break;
                case M6_SYS_MOD_RGN:
                    error |= (V4_SYS_MOD_RGN << 16); break;
                case M6_SYS_MOD_AI:
                    error |= (V4_SYS_MOD_AI << 16); break;
                default:
                    error |= (module << 16); break;
            }
            break;
        case HAL_PLATFORM_RK:
            level = (error >> 13) & 0x7;
            error = error & 0xFF001FFF | (level > 0 ? (4 << 13) : 0);
            switch (module) {
                case RK_SYS_MOD_SYS:
                    error |= (V4_SYS_MOD_SYS << 16); break;
                case RK_SYS_MOD_ISP:
                    error |= (V4_SYS_MOD_ISP << 16); break;
                case RK_SYS_MOD_VI:
                    error |= (V4_SYS_MOD_VIU << 16); break;
                case RK_SYS_MOD_VPSS:
                    error |= (V4_SYS_MOD_VPSS << 16); break;
                case RK_SYS_MOD_VENC:
                    error |= (V4_SYS_MOD_VENC << 16); break;
                case RK_SYS_MOD_RGN:
                    error |= (V4_SYS_MOD_RGN << 16); break;
                case RK_SYS_MOD_AI:
                    error |= (V4_SYS_MOD_AI << 16); break;
                default:
                    error |= (module << 16); break;
            }
            break;
#endif
    }

    switch (error) {
        case 0xA0028003:
            return "ERR_SYS_ILLEGAL_PARAM: The parameter "
                "configuration is invalid.";
        case 0xA0028006:
            return "ERR_SYS_NULL_PTR: The pointer is null.";
        case 0xA0028009:
            return "ERR_SYS_NOT_PERM: The operation is forbidden.";
        case 0xA0028010:
            return "ERR_SYS_NOTREADY: The system control attributes "
                "are not configured.";
        case 0xA0028012:
            return "ERR_SYS_BUSY: The system is busy.";
        case 0xA002800C:
            return "ERR_SYS_NOMEM: The memory fails to be allocated "
                "due to some causes such as insufficient system memory.";

        case 0xA0018003:
            return "ERR_VB_ILLEGAL_PARAM: The parameter configuration "
                "is invalid.";
        case 0xA0018005:
            return "ERR_VB_UNEXIST: The VB pool does not exist.";
        case 0xA0018006:
            return "ERR_VB_NULL_PTR: The pointer is null.";
        case 0xA0018009:
            return "ERR_VB_NOT_PERM: The operation is forbidden.";
        case 0xA001800C:
            return "ERR_VB_NOMEM: The memory fails to be allocated.";
        case 0xA001800D:
            return "ERR_VB_NOBUF: The buffer fails to be allocated.";
        case 0xA0018010:
            return "ERR_VB_NOTREADY: The system control attributes "
                "are not configured.";
        case 0xA0018012:
            return "ERR_VB_BUSY: The system is busy.";
        case 0xA0018040:
            return "ERR_VB_2MPOOLS: Too many VB pools are created.";

        case 0xA01C8006:
            return "ERR_ISP_NULL_PTR: The input pointer is null.";
        case 0xA01C8003:
            return "ERR_ISP_ILLEGAL_PARAM: The input parameter is "
                "invalid.";
        case 0xA01C8008:
            return "ERR_ISP_NOT_SUPPORT: This function is not "
                "supported by the ISP.";
        case 0xA01C8043:
            return "ERR_ISP_SNS_UNREGISTER: The sensor is not "
                "registered.";
        case 0xA01C8041:
            return "ERR_ISP_MEM_NOT_INIT: The external registers are "
                "not initialized.";
        case 0xA01C8040:
            return "ERR_ISP_NOT_INIT: The ISP is not initialized.";
        case 0xA01C8044:
            return "ERR_ISP_INVALID_ADDR: The address is invalid.";
        case 0xA01C8042:
            return "ERR_ISP_ATTR_NOT_CFG: The attribute is not "
                "configured.";

        case 0xA0108001:
            return "ERR_VI_INVALID_DEVID: The VI device ID is "
                "invalid.";
        case 0xA0108002:
            return "ERR_VI_INVALID_CHNID: The VI channel ID is "
                "invalid.";
        case 0xA0108003:
            return "ERR_VI_INVALID_PARA: The VI parameter is invalid.";
        case 0xA0108006:
            return "ERR_VI_INVALID_NULL_PTR: The pointer of the input "
                "parameter is null.";
        case 0xA0108007:
            return "ERR_VI_FAILED_NOTCONFIG: The attributes of the "
                "video device are not set.";
        case 0xA0108008:
            return "ERR_VI_NOT_SUPPORT: The operation is not "
                "supported.";
        case 0xA0108009:
            return "ERR_VI_NOT_PERM: The operation is forbidden.";
        case 0xA010800C:
            return "ERR_VI_NOMEM: The memory fails to be allocated.";
        case 0xA010800E:
            return "ERR_VI_BUF_EMPTY: The VI buffer is empty.";
        case 0xA010800F:
            return "ERR_VI_BUF_FULL: The VI buffer is full.";
        case 0xA0108010:
            return "ERR_VI_SYS_NOTREADY: The VI system is not "
                "initialized.";
        case 0xA0108012:
            return "ERR_VI_BUSY: The VI system is busy.";
        case 0xA0108040:
            return "ERR_VI_FAILED_NOTENABLE: The VI device or VI "
                "channel is not enabled.";
        case 0xA0108041:
            return "ERR_VI_FAILED_NOTDISABLE: The VI device or VI "
                "channel is not disabled.";
        case 0xA0108042:
            return "ERR_VI_FAILED_CHNOTDISABLE: The VI channel is not "
                "disabled.";
        case 0xA0108043:
            return "ERR_VI_CFG_TIMEOUT: The video attribute "
                "configuration times out.";
        case 0xA0108044:
            return "ERR_VI_NORM_UNMATCH: Mismatch occurs.";
        case 0xA0108045:
            return "ERR_VI_INVALID_WAYID: The video channel ID is "
                "invalid.";
        case 0xA0108046:
            return "ERR_VI_INVALID_PHYCHNID: The physical video "
                "channel ID is invalid.";
        case 0xA0108047:
            return "ERR_VI_FAILED_NOTBIND: The video channel is not "
                "bound.";
        case 0xA0108048:
            return "ERR_VI_FAILED_BINDED: The video channel is bound.";
        case 0xA0108049:
            return "ERR_VI_DIS_PROCESS_FAIL: The DIS fails to run.";

        case 0xA0078001:
            return "ERR_VPSS_INVALID_DEVID: The VPSS group ID is "
                "invalid.";
        case 0xA0078002:
            return "ERR_VPSS_INVALID_CHNID: The VPSS channel ID is "
                "invalid.";
        case 0xA0078003:
            return "ERR_VPSS_ILLEGAL_PARAM: The VPSS parameter is "
                "invalid.";
        case 0xA0078004:
            return "ERR_VPSS_EXIST: A VPSS group is created.";
        case 0xA0078005:
            return "ERR_VPSS_UNEXIST: No VPSS group is created.";
        case 0xA0078006:
            return "ERR_VPSS_NULL_PTR: The pointer of the input "
                "parameter is null.";
        case 0xA0078008:
            return "ERR_VPSS_NOT_SUPPORT: The operation is not "
                "supported.";
        case 0xA0078009:
            return "ERR_VPSS_NOT_PERM: The operation is forbidden.";
        case 0xA007800C:
            return "ERR_VPSS_NOMEM: The memory fails to be allocated.";
        case 0xA007800D:
            return "ERR_VPSS_NOBUF: The buffer pool fails to be "
                "allocated.";
        case 0xA007800E:
            return "ERR_VPSS_BUF_EMPTY: The picture queue is empty.";
        case 0xA0078010:
            return "ERR_VPSS_NOTREADY: The VPSS is not initialized.";
        case 0xA0078012:
            return "ERR_VPSS_BUSY: The VPSS is busy.";

        case 0xA0088002:
            return "ERR_VENC_INVALID_CHNID: The channel ID is "
                "invalid.";
        case 0xA0088003:
            return "ERR_VENC_ILLEGAL_PARAM: The parameter is invalid.";
        case 0xA0088004:
            return "ERR_VENC_EXIST: The device, channel or resource "
                "to be created or applied for exists.";
        case 0xA0088005:
            return "ERR_VENC_UNEXIST: The device, channel or resource "
                "to be used or destroyed does not exist.";
        case 0xA0088006:
            return "ERR_VENC_NULL_PTR: The parameter pointer is null.";
        case 0xA0088007:
            return "ERR_VENC_NOT_CONFIG: No parameter is set before "
                "use.";
        case 0xA0088008:
            return "ERR_VENC_NOT_SUPPORT: The parameter or function "
                "is not supported.";
        case 0xA0088009:
            return "ERR_VENC_NOT_PERM: The operation, for example, "
                "modifying static parameters, is forbidden.";
        case 0xA008800C:
            return "ERR_VENC_NOMEM: The memory fails to be allocated "
                "due to some causes such as insufficient system memory.";
        case 0xA008800D:
            return "ERR_VENC_NOBUF: The buffer fails to be allocated "
                "due to some causes such as oversize of the data buffer applied "
                "for.";
        case 0xA008800E:
            return "ERR_VENC_BUF_EMPTY: The buffer is empty.";
        case 0xA008800F:
            return "ERR_VENC_BUF_FULL: The buffer is full.";
        case 0xA0088010:
            return "ERR_VENC_SYS_NOTREADY: The system is not "
                "initialized or the corresponding module is not loaded.";
        case 0xA0088012:
            return "ERR_VENC_BUSY: The VENC system is busy.";

        case 0xA0038001:
            return "ERR_RGN_INVALID_DEVID: The device ID exceeds the "
                "valid range.";
        case 0xA0038002:
            return "ERR_RGN_INVALID_CHNID: The channel ID is "
                "incorrect or the region handle is invalid.";
        case 0xA0038003:
            return "ERR_RGN_ILLEGAL_PARAM: The parameter value "
                "exceeds its valid range.";
        case 0xA0038004:
            return "ERR_RGN_EXIST: The device, channel, or resource "
                "to be created already exists.";
        case 0xA0038005:
            return "ERR_RGN_UNEXIST: The device, channel, or resource "
                "to be used or destroyed does not exist.";
        case 0xA0038006:
            return "ERR_RGN_NULL_PTR: The pointer is null.";
        case 0xA0038007:
            return "ERR_RGN_NOT_CONFIG: The module is not configured.";
        case 0xA0038008:
            return "ERR_RGN_NOT_SUPPORT: The parameter or function is "
                "not supported.";
        case 0xA0038009:
            return "ERR_RGN_NOT_PERM: The operation, for example, "
                "attempting to modify the value of a static parameter, is "
                "forbidden.";
        case 0xA003800C:
            return "ERR_RGN_NOMEM: The memory fails to be allocated "
                "due to some causes such as insufficient system memory.";
        case 0xA003800D:
            return "ERR_RGN_NOBUF: The buffer fails to be allocated "
                "due to some causes such as oversize of the data buffer applied "
                "for.";
        case 0xA003800E:
            return "ERR_RGN_BUF_EMPTY: The buffer is empty.";
        case 0xA003800F:
            return "ERR_RGN_BUF_FULL: The buffer is full.";
        case 0xA0038010:
            return "ERR_RGN_NOTREADY: The system is not initialized "
                "or the corresponding module is not loaded.";
        case 0xA0038011:
            return "ERR_RGN_BADADDR: The address is invalid.";
        case 0xA0038012:
            return "ERR_RGN_BUSY: The system is busy.";

        case 0xA0158001:
            return "ERR_AI_INVALID_DEVID: The AI device ID is "
                "invalid.";
        case 0xA0158002:
            return "ERR_AI_INVALID_CHNID: The AI channel ID is "
                "invalid.";
        case 0xA0158003:
            return "ERR_AI_ILLEGAL_PARAM: The settings of the AI "
                "parameters are invalid.";
        case 0xA0158005:
            return "ERR_AI_NOT_ENABLED: The AI device or AI channel "
                "is not enabled.";
        case 0xA0158006:
            return "ERR_AI_NULL_PTR: The input parameter pointer is "
                "null.";
        case 0xA0158007:
            return "ERR_AI_NOT_CONFIG: The attributes of an AI device "
                "are not set.";
        case 0xA0158008:
            return "ERR_AI_NOT_SUPPORT: The operation is not "
                "supported.";
        case 0xA0158009:
            return "ERR_AI_NOT_PERM: The operation is forbidden.";
        case 0xA015800C:
            return "ERR_AI_NOMEM: The memory fails to be allocated.";
        case 0xA015800D:
            return "ERR_AI_NOBUF: The AI buffer is insufficient.";
        case 0xA015800E:
            return "ERR_AI_BUF_EMPTY: The AI buffer is empty.";
        case 0xA015800F:
            return "ERR_AI_BUF_FULL: The AI buffer is full.";
        case 0xA0158010:
            return "ERR_AI_SYS_NOTREADY: The AI system is not "
                "initialized.";
        case 0xA0158012:
            return "ERR_AI_BUSY: The AI system is busy.";
        case 0xA0158041:
            return "ERR_AI_VQE_ERR: A VQE processing error occurs in "
                "the AI channel.";

        default:
            return "Unknown error code.";
    }
}
