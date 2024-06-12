#pragma once

#include <dlfcn.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/select.h>

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

#define GM_ERROR(x, ...) \
    do { \
        fprintf(stderr, "[gm_hal] \033[31m"); \
        fprintf(stderr, (x), ##__VA_ARGS__); \
        fprintf(stderr, "\033[0m"); \
        return EXIT_FAILURE; \
    } while (0)

typedef struct {
    int width;
    int height;
} gm_common_dim;

typedef struct {
    unsigned int x;
    unsigned int y;
    unsigned int width;
    unsigned int height;
} gm_common_rect;