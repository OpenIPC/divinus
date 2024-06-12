#pragma once

#include <fcntl.h>
#include <linux/watchdog.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/ioctl.h>
#include <unistd.h>

#define WATCHDOG_ERROR(x, ...) \
    do { \
        fprintf(stderr, "[watchdog] \033[31m"); \
        fprintf(stderr, (x), ##__VA_ARGS__); \
        fprintf(stderr, "\033[0m"); \
        return EXIT_FAILURE; \
    } while (0)

void watchdog_reset(void);
int watchdog_start(int timeout);
void watchdog_stop(void);