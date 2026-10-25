#pragma once

#include "f1_common.h"

#define F1_OSD_MAX 8

typedef struct {
    char used;
    hal_rect rect;
    unsigned char opal;
} f1_osd_rgn;
