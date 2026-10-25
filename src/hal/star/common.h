#pragma once

#include "../globals.h"
#include "../tools.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define STAR_SYS_REG_SERIES 0x1F003C00
#define STAR_SYS_TEMP_PATH "/sys/class/mstar/msys/TEMP_R"

#define STAR_SERIES_I6   0xEF
#define STAR_SERIES_I6E  0xF1
#define STAR_SERIES_I6B0 0xF2
#define STAR_SERIES_I6C  0xF9
#define STAR_SERIES_M6   0xFB

static int star_identify(unsigned int *value, short *memory, char package[4]) {
    if (access("/proc/mi_modules", F_OK))
        return 0;
    if (!hal_registry(STAR_SYS_REG_SERIES, value, OP_READ))
        return 0;

    series = *value;
    *memory = 0;
    package[0] = package[1] = package[2] = package[3] = 0;

    FILE *file;
    if ((file = fopen("/proc/cmdline", "r"))) {
        char line[200];
        char *remain, *capacity = NULL;
        if (fgets(line, sizeof(line), file))
            capacity = strstr(line, "LX_MEM=");
        if (capacity)
            *memory = (short)(strtol(capacity + 7, &remain, 16) >> 20);
        fclose(file);
    }

    if ((file = fopen("/sys/devices/soc0/machine", "r"))) {
        char line[200];
        char *board = NULL;
        if (fgets(line, sizeof(line), file))
            board = strstr(line, "SSC");
        if (board && strlen(board) >= 7)
            strncpy(package, board + 4, 3);
        fclose(file);
    }

    return 1;
}

static float star_system_readtemp(void) {
    FILE *file;
    char line[20] = {0};

    if ((file = fopen(STAR_SYS_TEMP_PATH, "r"))) {
        char *remain, *parsed = NULL;
        if (fgets(line, sizeof(line), file))
            parsed = strstr(line, "Temperature ");
        fclose(file);
        if (parsed)
            return strtof(parsed + 12, &remain);
    }

    return 0.0 / 0.0;
}
