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
#endif

#define GPIO_ERROR(x, ...) fprintf(stderr, "%s \033[31m%s\033[0m\n", "[gpio] (x)", ##__VA_ARGS__)

void gpio_deinit(void);
int gpio_init(void);
int gpio_read(char pin, bool *value);
int gpio_write(char pin, bool value);