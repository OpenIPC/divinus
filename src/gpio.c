#include "gpio.h"

#if LINUX_VERSION_CODE >= KERNEL_VERSION(4, 8, 0)
const char *paths[] = {"/dev/gpiochip0", "/sys/class/gpio/gpiochip0"};
const char **path = paths;

int fd_gpio = 0;

char gpio_count = 0;

void gpio_deinit(void) {
    if (!fd_gpio) return;
    close(fd_gpio);
    fd_gpio = 0;
}

int gpio_init(void) {
    while (*path) {
        if (access(*path++, F_OK)) continue;
        if ((fd_gpio = open(*(path - 1), O_RDWR)) < 0)
            GPIO_ERROR("Unable to open the device %s!\n", *(path - 1));
        else break;
    }

    if (!fd_gpio) return EXIT_FAILURE;

    struct gpiochip_info info;
    int ret = ioctl(fd_gpio, GPIO_GET_CHIPINFO_IOCTL, &info);
    if (ret == -1) {
        gpio_deinit();
        GPIO_ERROR("Unable to enumerate the GPIO lines!\n");
    } else gpio_count = info.lines;

    return EXIT_SUCCESS;
}

int gpio_read(char pin, bool *value) {
    struct gpiohandle_request req = { .lineoffsets[0] = pin, .lines = 1, 
        .flags = GPIOHANDLE_REQUEST_INPUT };

    int ret = ioctl(fd_gpio, GPIO_GET_LINEHANDLE_IOCTL, &req);
    if (ret == -1)
        GPIO_ERROR("Unable to request a read on GPIO pin %d (error #%d)!\n", pin, errno);

    struct gpiohandle_data data;
    ret = ioctl(req.fd, GPIOHANDLE_GET_LINE_VALUES_IOCTL, &data);
    if (ret == -1) {
        close(req.fd);
        GPIO_ERROR("Unable to read the value of GPIO pin %d (error #%d)!\n", pin, errno);
    }

    *value = data.values[0];
    close(req.fd);
    return EXIT_SUCCESS;
}

int gpio_write(char pin, bool value) {
    struct gpiohandle_request req = { .default_values[0] = value, .lineoffsets[0] = pin,
        .lines = 1, .flags = GPIOHANDLE_REQUEST_OUTPUT };

    int ret = ioctl(fd_gpio, GPIO_GET_LINEHANDLE_IOCTL, &req);
    if (ret == -1)
        GPIO_ERROR("Unable to request a write on GPIO pin %d (error #%d)!\n", pin, errno);

    close(req.fd);

    return EXIT_SUCCESS;
}
#else
void gpio_deinit(void) {}

int gpio_init(void) {
    return EXIT_SUCCESS;
}

static inline int gpio_direction(char pin, char *mode) {
    char path[40];
    sprintf(path, "/sys/class/gpio/gpio%d/direction", pin);
    int fd = open(path, O_WRONLY);
    if (!fd)
        GPIO_ERROR("Unable to control the direction of GPIO pin %d!\n", pin);
    if (write(fd, mode, strlen(mode)) < 0) {
        close(fd);
        GPIO_ERROR("Unable to set the direction of GPIO pin %d!\n", pin);
    }

    close(fd);
    return EXIT_SUCCESS;
}

static inline int gpio_export(char pin, bool create) {
    char path[40];
    int fd = open(create ? "/sys/class/gpio/export" :
       "/sys/class/gpio/unexport", O_WRONLY);
    if (!fd)
        GPIO_ERROR("Unable to (un)export a GPIO pin!\n");

    char val[4];
    sprintf(val, "%d", pin);
    if (write(fd, val, strlen(val)) < 0) {
        close(fd);
        GPIO_ERROR("Unable to %s GPIO pin %d!\n", 
            create ? "export" : "unexport", pin);
    }

    close(fd);
    return EXIT_SUCCESS;
}

int gpio_read(char pin, bool *value) {
    gpio_export(pin, true);
    if (gpio_direction(pin, "in")) return EXIT_FAILURE;

    char path[40];
    sprintf(path, "/sys/class/gpio/gpio%d/value", pin);
    int fd = open(path, O_RDONLY);
    if (!fd)
        GPIO_ERROR("Unable to read from GPIO pin %d!\n", pin);

    char val = 0;
    lseek(fd, 0, SEEK_SET);
    read(fd, &val, 0);
    if (!val) {
        close(fd);
        GPIO_ERROR("Unable to read from GPIO pin %d!\n", pin);
    }
    *value = val - 0x30;
    close(fd);

    if (gpio_direction(pin, "out")) return EXIT_FAILURE;
    if (gpio_export(pin, false)) return EXIT_FAILURE;

    return EXIT_SUCCESS;
}

int gpio_write(char pin, bool value) {
    gpio_export(pin, true);
    if (gpio_direction(pin, "out")) return EXIT_FAILURE;

    char path[40];
    sprintf(path, "/sys/class/gpio/gpio%d/value", pin);
    int fd = open(path, O_WRONLY);
    if (!fd)
        GPIO_ERROR("Unable to write to GPIO pin %d!\n", pin);

    char val = value ? '1' : '0';
    if (write(fd, &val, 1) < 0) {
        close(fd);
        GPIO_ERROR("Unable to write to GPIO pin %d!\n", pin);
    }
    close(fd);

    if (gpio_export(pin, false)) return EXIT_FAILURE;

    return EXIT_SUCCESS;
}
#endif