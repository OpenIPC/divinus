#include <fcntl.h>
#include <linux/gpio.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/ioctl.h>

#define GPIO_ERROR(x, ...) fprintf(stderr, "%s \033[31m%s\033[0m\n", "[gpio] (x)", ##__VA_ARGS__)