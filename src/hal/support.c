#include "support.h"

void *isp_thread = NULL;
void *venc_thread = NULL;

char chnCount = 0;
hal_chnstate *chnState = NULL;
hal_platform plat = HAL_PLATFORM_UNK;

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
        mapped_area = mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_SHARED, mem_fd, offset);
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
        *data = *(volatile uint32_t *)(mapped_area + (addr - offset));
    if (op & OP_WRITE)
        *(volatile uint32_t *)(mapped_area + (addr - offset)) = *data;

    return true;
}

void hal_identify(void) {
    unsigned int val = 0;
    FILE *file;
    char *endMark;
    char line[200] = {0};

    if (!access("/proc/mi_modules", 0) && 
        hal_registry(0x1F003C00, &val, OP_READ))
        switch (val) {
            case 0xEF: // Macaron (6)
            case 0xF1: // Pudding (6E)
            case 0xF2: // Ispahan (6B0)
                plat = val == 0xF1 ? HAL_PLATFORM_I6E : 
                    val == 0xF2 ? HAL_PLATFORM_I6B0 : 
                    HAL_PLATFORM_I6;
                chnCount = I6_VENC_CHN_NUM;
                chnState = (hal_chnstate*)i6_state;
                venc_thread = i6_video_thread;
                return;
            case 0xF9:
                plat = HAL_PLATFORM_I6C;
                chnCount = I6C_VENC_CHN_NUM;
                chnState = (hal_chnstate*)i6c_state;
                venc_thread = i6c_video_thread;
                return;
            case 0xFB:
                plat = HAL_PLATFORM_I6F;
                chnCount = I6F_VENC_CHN_NUM;
                chnState = (hal_chnstate*)i6f_state;
                venc_thread = i6f_video_thread;
                return;
        }

    if (file = fopen("/proc/iomem", "r"))
        while (fgets(line, 200, file))
            if (strstr(line, "uart")) {
                val = strtol(line, &endMark, 16);
                break;
            }
    fclose(file);

    switch (val) {
        case 0x12040000:
        case 0x120a0000:
        case 0x12100000: val = 0x12020000; break;
        case 0x12080000: val = 0x12050000; break;
        case 0x20080000: val = 0x20050000; break;
        default: return;
    }

    unsigned SCSYSID[4] = {0};
    for (int i = 0; i < 4; i++) {
        if (!hal_registry(val + 0xEE0 + i * 4, (unsigned*)&SCSYSID[i], OP_READ)) break;
        if (!i && (SCSYSID[i] >> 16 & 0xFF)) { val = SCSYSID[i]; break; }
        val |= (SCSYSID[i] & 0xFF) << i * 8;
    }

    plat = HAL_PLATFORM_V4;
    chnCount = V4_VENC_CHN_NUM;
    chnState = (hal_chnstate*)v4_state;
    isp_thread = v4_image_thread;
    venc_thread = v4_video_thread;
}