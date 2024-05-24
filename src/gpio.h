#include <fcntl.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/ioctl.h>
#include <unistd.h>

#ifdef __arm__
#include <linux/gpio.h>
#endif

#define GPIO_ERROR(x, ...) fprintf(stderr, "%s \033[31m%s\033[0m\n", "[gpio] (x)", ##__VA_ARGS__)

void gpio_deinit(void);
int gpio_init(void);
int gpio_read(char pin, bool *value);
int gpio_write(char pin, bool value);
