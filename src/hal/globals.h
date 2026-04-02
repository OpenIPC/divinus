#pragma once

#include "types.h"

extern char audioOn, keepRunning;

extern void *aud_thread;
extern void *isp_thread;
extern void *vid_thread;

extern char chnCount;
extern hal_chnstate *chnState;

extern char chip[16];
extern char family[32];
extern hal_platform plat;
extern char sensor[16];
extern int series;
