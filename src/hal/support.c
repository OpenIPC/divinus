#include "support.h"

void *aud_thread = NULL;
void *isp_thread = NULL;
void *vid_thread = NULL;

char chnCount = 0;
hal_chnstate *chnState = NULL;
char chipId[16] = "unknown";
hal_platform plat = HAL_PLATFORM_UNK;
int series = 0;

bool hal_registry(unsigned int addr, unsigned int *data, hal_register_op op) {
    static int mem_fd;
    static char *loaded_area;
    static unsigned int loaded_offset;
    static unsigned int loaded_size;

    unsigned int offset = addr & 0xffff0000;
    unsigned int size = 0xffff;
    if (!addr || (loaded_area && offset != loaded_offset))
        if (munmap(loaded_area, loaded_size))
            fprintf(stderr, "hal_registry munmap error: %s (%d)\n",
                strerror(errno), errno);

    if (!addr) {
        close(mem_fd);
        return true;
    }

    if (!mem_fd && (mem_fd = open("/dev/mem", O_RDWR | O_SYNC)) < 0) {
        fprintf(stderr, "can't open /dev/mem\n");
        return false;
    }

    volatile char *mapped_area;
    if (offset != loaded_offset) {
        mapped_area = mmap64(NULL, size, PROT_READ | PROT_WRITE, MAP_SHARED, mem_fd, offset);
        if (mapped_area == MAP_FAILED) {
            fprintf(stderr, "hal_registry mmap error: %s (%d)\n",
                    strerror(errno), errno);
            return false;
        }
        loaded_area = (char *)mapped_area;
        loaded_size = size;
        loaded_offset = offset;
    } else
        mapped_area = loaded_area;

    if (op & OP_READ)
        *data = *(volatile unsigned int *)(mapped_area + (addr - offset));
    if (op & OP_WRITE)
        *(volatile unsigned int *)(mapped_area + (addr - offset)) = *data;

    return true;
}

void hal_identify(void) {
    unsigned int val = 0;
    FILE *file;
    char *endMark;
    char line[200] = {0};

#ifdef __arm__
    if (!access("/proc/mi_modules", 0) && 
        hal_registry(0x1F003C00, &series, OP_READ))
        switch (series) {
            case 0xEF: // Macaron (6)
            case 0xF1: // Pudding (6E)
            case 0xF2: // Ispahan (6B0)
                plat = HAL_PLATFORM_I6;
                strcpy(chipId, val == 0xEF ? 
                    "SSC32x" : "SSC33x");
                chnCount = I6_VENC_CHN_NUM;
                chnState = (hal_chnstate*)i6_state;
                aud_thread = i6_audio_thread;
                vid_thread = i6_video_thread;
                return;
            case 0xF9:
                plat = HAL_PLATFORM_I6C;
                strcpy(chipId, "SSC37x");
                chnCount = I6C_VENC_CHN_NUM;
                chnState = (hal_chnstate*)i6c_state;
                aud_thread = i6c_audio_thread;
                vid_thread = i6c_video_thread;
                return;
            case 0xFB:
                plat = HAL_PLATFORM_I6F;
                strcpy(chipId, "SSC37x");
                chnCount = I6F_VENC_CHN_NUM;
                chnState = (hal_chnstate*)i6f_state;
                aud_thread = i6f_audio_thread;
                vid_thread = i6f_video_thread;
                return;
        }
    
    if (!access("/dev/vpd", 0)) {
        plat = HAL_PLATFORM_GM;
        strcpy(chipId, "GM813x");
        if (file = fopen("/proc/pmu/chipver", "r")) {
            fgets(line, 200, file);
            sscanf(line, "%4s", chipId + 2);
            fclose(file);
        }
        chnCount = GM_VENC_CHN_NUM;
        chnState = (hal_chnstate*)gm_state;
        vid_thread = gm_video_thread;
        return;
    }
#endif

#ifdef __mips__
    if (!access("/proc/jz", 0) && 
        hal_registry(0x1300002C, &val, OP_READ)) {
        unsigned int type;
        hal_registry(0x13540238, &type, OP_READ);
        char gen = (val >> 12) & 0xFF;
        switch (gen) {
            case 0x31:
                plat = HAL_PLATFORM_T31;
                switch (type >> 16) {
                    case 0x2222: sprintf(chipId, "T31X");  break;
                    case 0x3333: sprintf(chipId, "T31L");  break;
                    case 0x4444: sprintf(chipId, "T31A");  break;
                    case 0x5555: sprintf(chipId, "T31ZL"); break;
                    case 0x6666: sprintf(chipId, "T31ZX"); break;
                    case 0xcccc: sprintf(chipId, "T31AL"); break;
                    case 0xdddd: sprintf(chipId, "T31ZC"); break;
                    case 0xeeee: sprintf(chipId, "T31LC"); break;
                    default:     sprintf(chipId, "T31N");  break;
                }
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

    char v3series = 0;

    switch (val) {
        case 0x12040000:
        case 0x120a0000: val = 0x12020000; break;
        case 0x12100000:
            val = 0x12020000;
            v3series = 1;
            break;
        case 0x20080000: val = 0x20050000; break;

        default: return;
    }

    unsigned SCSYSID[4] = {0};
    for (int i = 0; i < 4; i++) {
        if (!hal_registry(val + 0xEE0 + i * 4, (unsigned*)&SCSYSID[i], OP_READ)) break;
        if (!i && (SCSYSID[i] >> 16 & 0xFF)) { val = SCSYSID[i]; break; }
        val |= (SCSYSID[i] & 0xFF) << i * 8;
    }

    sprintf(chipId, "%s%X", 
        ((val >> 28) == 0x7) ? "GK" : "Hi", val);
    if (chipId[6] == '0') {
        chipId[6] = 'V';
    } else {
        chipId[8] = chipId[7];
        chipId[7] = 'V';
        chipId[9] = chipId[8];
        chipId[10] = chipId[9];
        chipId[11] = '\0';
    }

    if (v3series) {
        plat = HAL_PLATFORM_V3;
        chnCount = V3_VENC_CHN_NUM;
        chnState = (hal_chnstate*)v3_state;
        aud_thread = v3_audio_thread;
        isp_thread = v3_image_thread;
        vid_thread = v3_video_thread;
        return;
    }

    plat = HAL_PLATFORM_V4;
    chnCount = V4_VENC_CHN_NUM;
    chnState = (hal_chnstate*)v4_state;
    aud_thread = v4_audio_thread;
    isp_thread = v4_image_thread;
    vid_thread = v4_video_thread;
#endif
}
