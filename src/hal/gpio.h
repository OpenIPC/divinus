#pragma once

#include <errno.h>
#include <fcntl.h>
#include <linux/version.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#if LINUX_VERSION_CODE >= KERNEL_VERSION(4, 8, 0)
#include <linux/gpio.h>
#include <sys/ioctl.h>
#else
#include <string.h>
#endif

#include "macros.h"

void gpio_deinit(void);
int gpio_init(void);
void gpio_release(char pin);
void gpio_release_unused(const short *keep, int count);
int gpio_read(char pin, bool *value);
int gpio_write(char pin, bool value);
