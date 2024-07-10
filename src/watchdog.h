#pragma once

#include "common.h"

#include <fcntl.h>
#include <linux/watchdog.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/ioctl.h>
#include <unistd.h>

void watchdog_reset(void);
int watchdog_start(int timeout);
void watchdog_stop(void);