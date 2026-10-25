#include "support.h"

void *aud_thread = NULL;
void *isp_thread = NULL;
void *vid_thread = NULL;

char chnCount = 0;
hal_chnstate *chnState = NULL;

char chip[16] = "unknown";
char family[32] = {0};
hal_platform plat = HAL_PLATFORM_UNK;
char sensor[16] = "unidentified";
int series = 0;

unsigned long long lastMillisTemp = 0;
float lastReadTemp = 0.0 / 0.0;

void hal_identify(void) {
#if defined(__ARM_PCS_VFP)
    if (i6_system_identify()) return;
    if (i6c_system_identify()) return;
    if (m6_system_identify()) return;
    if (i3_system_identify()) return;
    if (rk_system_identify()) return;
#elif defined(__arm__) && !defined(__ARM_PCS_VFP)
#if __ARM_ARCH == 6
    if (f1_system_identify()) return;
    if (f2_system_identify()) return;
#endif
    if (gm_system_identify()) return;
    if (ak_system_identify()) return;
    if (v1_system_identify()) return;
    if (v2_system_identify()) return;
    if (v3_system_identify()) return;
    if (v4_system_identify()) return;
#elif defined(__mips__)
    if (t31_system_identify()) return;
#elif defined(__riscv) || defined(__riscv__)
    if (cvi_system_identify()) return;
#endif
}

float hal_temperature_read(void) {
    if (lastReadTemp != (0.0 / 0.0) && (millis() - lastMillisTemp < 5000))
        return lastReadTemp;

    lastMillisTemp = millis();

    switch (plat) {
#if defined(__ARM_PCS_VFP)
        case HAL_PLATFORM_I6:  lastReadTemp = i6_system_readtemp(); break;
        case HAL_PLATFORM_I6C: lastReadTemp = i6c_system_readtemp(); break;
        case HAL_PLATFORM_M6:  lastReadTemp = m6_system_readtemp(); break;
#elif defined(__arm__) && !defined(__ARM_PCS_VFP)
        case HAL_PLATFORM_V2: lastReadTemp = v2_system_readtemp(); break;
        case HAL_PLATFORM_V3: lastReadTemp = v3_system_readtemp(); break;
        case HAL_PLATFORM_V4: lastReadTemp = v4_system_readtemp(); break;
#endif
        default:
            if (!access("/sys/class/thermal/thermal_zone0/temp", F_OK)) {
                FILE* file;
                char line[10] = {0};
                if (file = fopen("/sys/class/thermal/thermal_zone0/temp", "r")) {
                    fgets(line, 10, file);
                    lastReadTemp = strtof(line, NULL) / 1000.0;
                    fclose(file);
                }
            } else lastMillisTemp = UINT64_MAX;
            break;
    }

    return lastReadTemp;
}
