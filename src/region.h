#pragma once

#include "common.h"
#include "hal/support.h"
#include "text.h"

#define DEF_COLOR 0xFFFF
#define DEF_FONT "UbuntuMono-Regular"
#define DEF_OPAL 255
#define DEF_POSX 16
#define DEF_POSY 16
#define DEF_SIZE 32.0f
#define DEF_TIMEFMT "%Y/%m/%d %H:%M:%S"
#define MAX_OSD 8

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include <ifaddrs.h>
#include <linux/if_link.h>
#include <linux/sysinfo.h>
#include <pthread.h>
#include <time.h>

extern int sysinfo (struct sysinfo *__info);

typedef struct {
    double size;
    int hand, color;
    short opal, posx, posy;
    char updt;
    char font[32];
    char text[80];
} osd;

extern osd osds[MAX_OSD];
extern char timefmt[32];

int start_region_handler();
void stop_region_handler();