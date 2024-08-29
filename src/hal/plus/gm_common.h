#pragma once

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../symbols.h"
#include "../types.h"

#define GM_LIB_VER 0x41

typedef enum {
    GM_LIB_DEV_CAPTURE = 0xFEFE0001,
    GM_LIB_DEV_VIDENC,
    GM_LIB_DEV_WINDOW,
    GM_LIB_DEV_FILE,
    GM_LIB_DEV_AUDIN,
    GM_LIB_DEV_AUDENC,
    GM_LIB_DEV_AUDOUT
} gm_lib_dev;

#define GM_DECLARE(hal, var, type, real) \
    type var = ({hal.fnDeclareStruct(&var, real, sizeof(type), GM_LIB_VER); var;})

typedef struct {
    int width;
    int height;
} gm_common_dim;

typedef struct {
    unsigned int type;
    unsigned int bsLength;
    unsigned int mvLength;
    unsigned int isKeyFrame;
} gm_common_pollevt;

typedef struct {
    void *bind;
    unsigned int evType;
    gm_common_pollevt event;
    int internal[4];
} gm_common_pollfd;

typedef struct {
    unsigned int x;
    unsigned int y;
    unsigned int width;
    unsigned int height;
} gm_common_rect;

typedef struct {
    char *bsData;
    unsigned int bsLength;
    char *mdData;
    unsigned int mdLength;
    unsigned int bsSize;
    unsigned int mdSize;
    int isKeyFrame;
    unsigned int timestamp;
    unsigned int bsChanged;
    unsigned int checksum;
    int isRefFrame;
    unsigned int sliceOff[3];
    int reserved[5];
} gm_common_pack;

typedef struct {
    void *bind;
    gm_common_pack pack;
    int ret;
    int reserved[6];
    int internal[28];
} gm_common_strm;