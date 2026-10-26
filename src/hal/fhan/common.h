#pragma once

#include "../gpio.h"
#include "../globals.h"
#include "../tools.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define FH_PMU_BASE     0xf0000000
#define FH_PMU_CHIP_ID  (FH_PMU_BASE + 0x00)
#define FH_PMU_IP_VER   (FH_PMU_BASE + 0x04)
#define FH_PMU_SYS_CTRL (FH_PMU_BASE + 0x0c)

#define FH_PMU_CHIP_ID_V100 0x17092901
#define FH_PMU_CHIP_ID_V200 0x19112201
#define FH_PMU_IP_VER_MASK  0xf
#define FH_PMU_IP_VER_8852  0xc
#define FH_PMU_IP_VER_8856  0xd
#define FH_PMU_PKG_MASK     0xffffff
#define FH_PKG_8852V200     0x00000001
#define FH_PKG_8856V200     0x00100001
#define FH_PKG_8858V200     0x00410001
#define FH_PKG_8852V210     0x00000002
#define FH_PKG_8856V210     0x00100002
#define FH_PKG_8858V210     0x00410002
#define FH_PKG_8856V201     0x00200001
#define FH_PKG_8858V201     0x00300001

/* Illuminators: /proc/driver/pwm takes "id,output_mask,duty_ns,period_ns,...",
 * the mask written raw to GLOBAL_CTRL2 (so 1 << id). IR = PWM3 (GPIO7), white
 * = PWM7 + the white-lamp GPIO */
#define FH_PWM_ON(id, mask)  id "," mask ",60,200,0,0,0,0"
#define FH_PWM_OFF(id, mask) id "," mask ",0,200,0,0,0,0"
#define FH_LAMP_WHITE_GPIO 11

static void fh_proc_write(const char *path, const char *value) {
    FILE *file = fopen(path, "w");
    if (!file) {
        HAL_WARNING("fh_hal", "Cannot write %s to %s!\n", value, path);
        return;
    }
    fputs(value, file);
    fclose(file);
}

static int fh_lamp_enable(int flags) {
    fh_proc_write("/proc/driver/pwm",
        (flags & HAL_LIGHT_IR) ? FH_PWM_ON("3", "8") : FH_PWM_OFF("3", "8"));
    fh_proc_write("/proc/driver/pwm",
        (flags & HAL_LIGHT_WHITE) ? FH_PWM_ON("7", "128") : FH_PWM_OFF("7", "128"));
    gpio_write(FH_LAMP_WHITE_GPIO, (flags & HAL_LIGHT_WHITE) ? 1 : 0);
    return EXIT_SUCCESS;
}

static hal_platform fh_identify(void) {
    unsigned int chip_id = 0, ip_ver = 0, sys_ctrl = 0;

    if (hal_registry(FH_PMU_CHIP_ID, &chip_id, OP_READ) &&
        hal_registry(FH_PMU_IP_VER, &ip_ver, OP_READ) &&
        hal_registry(FH_PMU_SYS_CTRL, &sys_ctrl, OP_READ)) {
        if (chip_id == FH_PMU_CHIP_ID_V100 &&
            ((ip_ver & FH_PMU_IP_VER_MASK) == FH_PMU_IP_VER_8852 ||
             (ip_ver & FH_PMU_IP_VER_MASK) == FH_PMU_IP_VER_8856)) {
            strcpy(chip, (ip_ver & FH_PMU_IP_VER_MASK) == FH_PMU_IP_VER_8852 ?
                "FH8852V100" : "FH8856V100");
            return HAL_PLATFORM_F1;
        }
        if (chip_id == FH_PMU_CHIP_ID_V200) {
            switch (sys_ctrl & FH_PMU_PKG_MASK) {
                case FH_PKG_8852V200: strcpy(chip, "FH8852V200"); return HAL_PLATFORM_F2;
                case FH_PKG_8856V200: strcpy(chip, "FH8856V200"); return HAL_PLATFORM_F2;
                case FH_PKG_8858V200: strcpy(chip, "FH8858V200"); return HAL_PLATFORM_F2;
                case FH_PKG_8852V210: strcpy(chip, "FH8852V210"); return HAL_PLATFORM_F2;
                case FH_PKG_8856V210: strcpy(chip, "FH8856V210"); return HAL_PLATFORM_F2;
                case FH_PKG_8858V210: strcpy(chip, "FH8858V210"); return HAL_PLATFORM_F2;
                case FH_PKG_8856V201: strcpy(chip, "FH8856V201"); return HAL_PLATFORM_F2;
                case FH_PKG_8858V201: strcpy(chip, "FH8858V201"); return HAL_PLATFORM_F2;
                default: break;
            }
        }
    }

    {
        char name[16] = {0};
        const char *sources[] = {"/proc/cpuinfo", "/proc/driver/chip"};
        for (int i = 0; i < 2 && !name[0]; i++) {
            FILE *file = fopen(sources[i], "r");
            if (!file) continue;
            char line[200];
            while (fgets(line, sizeof(line), file)) {
                char *part = strstr(line, "FH8");
                if (part) {
                    sscanf(part, "%15s", name);
                    break;
                }
            }
            fclose(file);
        }
        if (name[0]) {
            strncpy(chip, name, sizeof(chip) - 1);
            if (strstr(name, "V2"))
                return HAL_PLATFORM_F2;
            if (!strncmp(name, "FH8852", 6) || !strncmp(name, "FH8856", 6))
                return HAL_PLATFORM_F1;
            if (strstr(name, "FH8"))
                return HAL_PLATFORM_F2;
        }
    }

    return HAL_PLATFORM_UNK;
}
