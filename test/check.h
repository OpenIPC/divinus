#pragma once

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int checkFailures = 0;

#define CHECK(cond) \
    do { \
        if (!(cond)) { \
            fprintf(stderr, "%s:%d: CHECK failed: %s\n", __FILE__, __LINE__, #cond); \
            checkFailures++; \
        } \
    } while (0)

#define CHECK_DONE() \
    do { \
        if (checkFailures) { \
            fprintf(stderr, "%d check(s) failed\n", checkFailures); \
            return EXIT_FAILURE; \
        } \
        printf("%s: ok\n", __FILE__); \
        return EXIT_SUCCESS; \
    } while (0)
