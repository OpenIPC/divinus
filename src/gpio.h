#include "hal/macros.h"

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

void gpio_deinit(void);
int gpio_init(void);
int gpio_read(char pin, bool *value);
int gpio_write(char pin, bool value);