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

void hal_identify(void) {
    unsigned int val = 0;
    FILE *file;
    char *endMark;
    char line[200] = {0};

    char *sensorlocal = getenv("SENSOR");
    if (*sensorlocal) strncpy(sensor, sensorlocal, sizeof(sensor));

#ifdef __arm__
    if (!access("/proc/mi_modules", F_OK) && 
        hal_registry(0x1F003C00, &series, OP_READ)) {
        char package[4] = {0};
        short memory = 0;

        plat = HAL_PLATFORM_I6;
        chnCount = I6_VENC_CHN_NUM;
        chnState = (hal_chnstate*)i6_state;
        aud_thread = i6_audio_thread;
        vid_thread = i6_video_thread;

        if (file = fopen("/proc/cmdline", "r")) {
            fgets(line, 200, file);
            char *remain, *capacity = strstr(line, "LX_MEM=");
            memory = (short)(strtol(capacity + 7, &remain, 16) >> 20);
            fclose(file);
        }

        if (file = fopen("/sys/devices/soc0/machine", "r")) {
            fgets(line, 200, file);
            char *board = strstr(line, "SSC");
            strncpy(package, board + 4, 3);
            fclose(file);
        }

        switch (series) {
            case 0xEF:
                if (memory > 128)
                    strcpy(chip, "SSC327Q");
                else if (memory > 64) 
                    strcpy(chip, "SSC32[5/7]D");
                else 
                    strcpy(chip, "SSC32[3/5/7]");
                if (package[2] == 'B' && chip[strlen(chip) - 1] != 'Q')
                    strcat(chip, "E");
                strcpy(family, "infinity6");
                break;
            case 0xF1:
                if (package[2] == 'A')
                    strcpy(chip, "SSC33[8/9]G");
                else {
                    if (sysconf(_SC_NPROCESSORS_ONLN) == 1)
                        strcpy(chip, "SSC30K");
                    else
                        strcpy(chip, "SSC33[6/8]");
                    if (memory > 128)
                        strcat(chip, "Q");
                    else if (memory > 64)
                        strcat(chip, "D");
                }
                strcpy(family, "infinity6e");
                break;
            case 0xF2: 
                strcpy(chip, "SSC33[3/5/7]");
                if (memory > 64)
                    strcat(chip, "D");
                if (package[2] == 'B')
                    strcat(chip, "E");
                strcpy(family, "infinity6b0");
                break;
            case 0xF9:
                plat = HAL_PLATFORM_I6C;
                strcpy(chip, "SSC37[7/8]");
                if (memory > 128)
                    strcat(chip, "Q");
                else if (memory > 64)
                    strcat(chip, "D");
                if (package[2] == 'D')
                    strcat(chip, "E");
                strcpy(family, "infinity6c");
                chnCount = I6C_VENC_CHN_NUM;
                chnState = (hal_chnstate*)i6c_state;
                aud_thread = i6c_audio_thread;
                vid_thread = i6c_video_thread;
                break;
            case 0xFB:
                plat = HAL_PLATFORM_M6;
                strcpy(chip, "SSC359G");
                strcpy(family, "mercury6");
                chnCount = M6_VENC_CHN_NUM;
                chnState = (hal_chnstate*)m6_state;
                aud_thread = m6_audio_thread;
                vid_thread = m6_video_thread;
                break;
            default:
                plat = HAL_PLATFORM_UNK;
                break;
        }
    } else if (!access("/proc/mstar/kernel", 0) && 
        hal_registry(0x1F2025A4, &val, OP_READ)) {
        plat = HAL_PLATFORM_I3;
        switch (val & 0xF000) {
            case 0x6000: strcpy(chip, "MSC313E"); break;
            case 0x7000: strcpy(chip, "MSC316DC"); break;
            case 0x8000: strcpy(chip, "MSC318"); break;
        }
        strcpy(family, "infinity3");
        chnCount = I3_VENC_CHN_NUM;
        chnState = (hal_chnstate*)i3_state;
        //aud_thread = i3_audio_thread;
        //vid_thread = i3_video_thread;
        return;
    }
    
    if (!access("/dev/vpd", F_OK)) {
        plat = HAL_PLATFORM_GM;
        strcpy(chip, "GM813x");
        if (file = fopen("/proc/pmu/chipver", "r")) {
            fgets(line, 200, file);
            sscanf(line, "%4s", chip + 2);
            fclose(file);
        }
        strcpy(family, "grainmedia");
        chnCount = GM_VENC_CHN_NUM;
        chnState = (hal_chnstate*)gm_state;
        aud_thread = gm_audio_thread;
        vid_thread = gm_video_thread;
        return;
    }

    if (!access("/sys/devices/platform/ak39-uart.0", F_OK) &&
        hal_registry(0x08000000, &val, OP_READ)) {
        plat = HAL_PLATFORM_AK;
        strcpy(chip, "AK3918E");
        switch(val) {
            case 0x20120100: strcat(chip, "V100"); break;
            case 0x20150200: strcat(chip, "V200"); break;
            case 0x20160100: strcat(chip, "V300"); break;
            case 0x20160101: strcat(chip, "V330"); break;
        }
        strcpy(family, "anyka");
        chnCount = AK_VENC_CHN_NUM;
        chnState = (hal_chnstate*)ak_state;
        //aud_thread = ak_audio_thread;
        vid_thread = ak_video_thread;
        return;
    }
#endif

#ifdef __mips__
    if (!access("/proc/jz", F_OK) && 
        hal_registry(0x1300002C, &val, OP_READ)) {
        unsigned int type;
        hal_registry(0x13540238, &type, OP_READ);
        char gen = (val >> 12) & 0xFF;
        switch (gen) {
            case 0x31:
                plat = HAL_PLATFORM_T31;
                switch (type >> 16) {
                    case 0x2222: sprintf(chip, "T31X");  break;
                    case 0x3333: sprintf(chip, "T31L");  break;
                    case 0x4444: sprintf(chip, "T31A");  break;
                    case 0x5555: sprintf(chip, "T31ZL"); break;
                    case 0x6666: sprintf(chip, "T31ZX"); break;
                    case 0xcccc: sprintf(chip, "T31AL"); break;
                    case 0xdddd: sprintf(chip, "T31ZC"); break;
                    case 0xeeee: sprintf(chip, "T31LC"); break;
                    default:     sprintf(chip, "T31N");  break;
                }
                strcpy(family, "ingenic t31");
                chnCount = T31_VENC_CHN_NUM;
                chnState = (hal_chnstate*)t31_state;
                aud_thread = t31_audio_thread;
                vid_thread = t31_video_thread;
                return;
        }
    }
#endif

#ifdef __arm__
    if (file = fopen("/proc/iomem", "r")) {
        while (fgets(line, 200, file))
            if (strstr(line, "uart")) {
                val = strtol(line, &endMark, 16);
                break;
            }
        fclose(file);
    }

    switch (val) {
        case 0x12040000:
        case 0x120a0000: val = 0x12020000; break;
        case 0x12100000:
            val = 0x12020000;
            series = 3;
            break;
        case 0x20080000:
            val = 0x20050000;
            series = 2;
            break;

        default: return;
    }

    unsigned SCSYSID[4] = {0};
    unsigned int out = 0;
    for (int i = 0; i < 4; i++) {
        if (!hal_registry(val + 0xEE0 + (i * 4), (unsigned*)&SCSYSID[i], OP_READ)) break;
        if (!i && (SCSYSID[i] >> 16 & 0xFF)) { out = SCSYSID[i]; break; }
        out |= (SCSYSID[i] & 0xFF) << i * 8;
    }

    if (out == 0x35180100) {
        series = 1;
        if (hal_registry(0x2005008C, &val, OP_READ))
            switch (val >> 8 & 0x7f) {
                case 0x10: out = 0x3518C100; break;
                case 0x57: out = 0x3518E100; break;
                default:
                    val = 3;
                    if (hal_registry(0x20050088, &val, OP_WRITE) &&
                        hal_registry(0x20050088, &val, OP_READ))
                        switch (val) {
                            case 1: out = 0x3516C100; break;
                            case 2: out = 0x3518E100; break;
                            case 3: out = 0x3518A100; break;
                        }
            }
    }

    sprintf(chip, "%s%X", 
        ((out >> 28) == 0x7) ? "GK" : "Hi", out);
    if (chip[6] == '0') {
        chip[6] = 'V';
    } else {
        chip[11] = '\0';
        chip[10] = chip[9];
        chip[9] = chip[8];
        chip[8] = chip[7];
        chip[7] = 'V';
    }

    if (series == 1) {
        plat = HAL_PLATFORM_V1;
        strcpy(family, "hisi-gen1");
        chnCount = V1_VENC_CHN_NUM;
        chnState = (hal_chnstate*)v1_state;
        aud_thread = v1_audio_thread;
        isp_thread = v1_image_thread;
        vid_thread = v1_video_thread;
        return;    
    } else if (series == 2) {
        plat = HAL_PLATFORM_V2;
        strcpy(family, "hisi-gen2");
        chnCount = V2_VENC_CHN_NUM;
        chnState = (hal_chnstate*)v2_state;
        aud_thread = v2_audio_thread;
        isp_thread = v2_image_thread;
        vid_thread = v2_video_thread;
        return;    
    } else if (series == 3) {
        plat = HAL_PLATFORM_V3;
        strcpy(family, "hisi-gen3");
        chnCount = V3_VENC_CHN_NUM;
        chnState = (hal_chnstate*)v3_state;
        aud_thread = v3_audio_thread;
        isp_thread = v3_image_thread;
        vid_thread = v3_video_thread;
        return;
    }

    series = 4;
    plat = HAL_PLATFORM_V4;
    strcpy(family, "hisi-gen4");
    chnCount = V4_VENC_CHN_NUM;
    chnState = (hal_chnstate*)v4_state;
    aud_thread = v4_audio_thread;
    isp_thread = v4_image_thread;
    vid_thread = v4_video_thread;
#endif

#if defined(__riscv) || defined(__riscv__)
    if (!access("/proc/cvi", F_OK)) {
        plat = HAL_PLATFORM_CVI;
        strcpy(family, "CV181x");
        chnCount = CVI_VENC_CHN_NUM;
        chnState = (hal_chnstate*)cvi_state;
        aud_thread = cvi_audio_thread;
        vid_thread = cvi_video_thread;
    }
#endif
}
