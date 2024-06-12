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
#include <pthread.h>
#include <sys/sysinfo.h>
#include <time.h>

#define REGION_ERROR(x, ...) \
    do { \
        fprintf(stderr, "[region] \033[31m"); \
        fprintf(stderr, (x), ##__VA_ARGS__); \
        fprintf(stderr, "\033[0m"); \
        return EXIT_FAILURE; \
    } while (0)

extern char keepRunning;

extern int sysinfo (struct sysinfo *__info);

typedef struct {
    unsigned int size;
    unsigned short reserved1;
    unsigned short reserved2;
    unsigned int offBits;
} bitmapfile;

typedef struct {
    unsigned short size;
    unsigned int width;
    int height;
    unsigned short planes;
    unsigned short bitCount;
    unsigned int compression;
    unsigned int sizeImage;
    unsigned int xPerMeter;
    unsigned int yPerMeter;
    unsigned int clrUsed;
    unsigned int clrImportant;
} bitmapinfo;

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