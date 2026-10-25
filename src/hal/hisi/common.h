#pragma once

#include "../globals.h"
#include "../tools.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define HISI_UART_V4A       0x12040000
#define HISI_UART_V4B       0x120a0000
#define HISI_UART_V3        0x12100000
#define HISI_UART_V2        0x20080000
#define HISI_SCSYSID_V4     0x12020000
#define HISI_SCSYSID_V2     0x20050000
#define HISI_SCSYSID_OFFSET 0xEE0
#define HISI_CHIPID_REG     0x2005008C
#define HISI_CHIPID_ALT_REG 0x20050088
#define HISI_CHIPID_3518    0x35180100
#define HISI_CHIPID_3518C   0x3518C100
#define HISI_CHIPID_3518E   0x3518E100
#define HISI_CHIPID_3516C   0x3516C100
#define HISI_CHIPID_3518A   0x3518A100
#define HISI_CHIPID_SUB_MASK 0x7f
#define HISI_CHIPID_3518C_SUB 0x10
#define HISI_CHIPID_3518E_SUB 0x57
#define HISI_CHIPID_ALT_3516C 1
#define HISI_CHIPID_ALT_3518E 2
#define HISI_CHIPID_ALT_3518A 3

static int hisi_identify(void) {
    unsigned int val = 0;
    char *endMark, line[200] = {0};

    FILE *file = fopen("/proc/iomem", "r");
    if (!file)
        return 0;

    while (fgets(line, sizeof(line), file))
        if (strstr(line, "uart")) {
            val = strtol(line, &endMark, 16);
            break;
        }
    fclose(file);

    int detected = 0;
    switch (val) {
        case HISI_UART_V4A:
        case HISI_UART_V4B: val = HISI_SCSYSID_V4; break;
        case HISI_UART_V3:
            val = HISI_SCSYSID_V4;
            detected = 3;
            break;
        case HISI_UART_V2:
            val = HISI_SCSYSID_V2;
            detected = 2;
            break;
        default: return 0;
    }

    unsigned SCSYSID[4] = {0};
    unsigned int out = 0;
    for (int i = 0; i < 4; i++) {
        if (!hal_registry(val + HISI_SCSYSID_OFFSET + (i * 4),
                (unsigned*)&SCSYSID[i], OP_READ))
            break;
        if (!i && (SCSYSID[i] >> 16 & 0xFF)) {
            out = SCSYSID[i];
            break;
        }
        out |= (SCSYSID[i] & 0xFF) << i * 8;
    }

    if (out == HISI_CHIPID_3518) {
        detected = 1;
        if (hal_registry(HISI_CHIPID_REG, &val, OP_READ))
            switch (val >> 8 & HISI_CHIPID_SUB_MASK) {
                case HISI_CHIPID_3518C_SUB: out = HISI_CHIPID_3518C; break;
                case HISI_CHIPID_3518E_SUB: out = HISI_CHIPID_3518E; break;
                default:
                    val = 3;
                    if (hal_registry(HISI_CHIPID_ALT_REG, &val, OP_WRITE) &&
                        hal_registry(HISI_CHIPID_ALT_REG, &val, OP_READ))
                        switch (val) {
                            case HISI_CHIPID_ALT_3516C: out = HISI_CHIPID_3516C; break;
                            case HISI_CHIPID_ALT_3518E: out = HISI_CHIPID_3518E; break;
                            case HISI_CHIPID_ALT_3518A: out = HISI_CHIPID_3518A; break;
                        }
            }
    }

    sprintf(chip, "%s%X", ((out >> 28) == 0x7) ? "GK" : "Hi", out);
    if (chip[6] == '0') {
        chip[6] = 'V';
    } else {
        chip[11] = '\0';
        chip[10] = chip[9];
        chip[9] = chip[8];
        chip[8] = chip[7];
        chip[7] = 'V';
    }

    series = detected ? detected : 4;
    return series;
}
