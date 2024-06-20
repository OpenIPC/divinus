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
    while (*path++) {
        if (access(*path, 0)) continue;
        fd_gpio = open(*path, O_RDWR);
        if (fd_gpio < 0)
            GPIO_ERROR("Unable to open the GPIO device!\n");
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
    struct gpiohandle_request req = { .lineoffsets = { pin }, .lines = 1, 
        .flags = GPIOHANDLE_REQUEST_INPUT };

    int ret = ioctl(fd_gpio, GPIO_GET_LINEHANDLE_IOCTL, &req);
    if (ret == -1)
        GPIO_ERROR("Unable to request a read on GPIO pin %d!\n", pin);

    struct gpiohandle_data data;
    ret = ioctl(fd_gpio, GPIOHANDLE_GET_LINE_VALUES_IOCTL, &data);
    if (ret == -1) {
        close(req.fd);
        GPIO_ERROR("Unable to read the value of GPIO pin %d!\n", pin);
    }

    *value = data.values[0];
    close(req.fd);
    return EXIT_SUCCESS;
}

int gpio_write(char pin, bool value) {
    struct gpiohandle_request req = { .lineoffsets = { pin }, .lines = 1, 
        .flags = GPIOHANDLE_REQUEST_OUTPUT };

    int ret = ioctl(fd_gpio, GPIO_GET_LINEHANDLE_IOCTL, &req);
    if (ret == -1)
        GPIO_ERROR("Unable to request a write on GPIO pin %d!\n", pin);

    struct gpiohandle_data data = { .values = { value } };
    ret = ioctl(fd_gpio, GPIOHANDLE_SET_LINE_VALUES_IOCTL, &data);
    if (ret == -1) {
        close(req.fd);
        GPIO_ERROR("Unable to write a value to GPIO pin %d!\n", pin);
    }

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
    FILE *fd = fopen(path, "w");
    if (!fd)
        GPIO_ERROR("Unable to control the direction of GPIO pin %d!\n", pin);
    fwrite(mode, 1, sizeof(mode), fd);
    fclose(fd);
}

static inline int gpio_export(char pin, bool create) {
    char path[40];
    FILE *fd = fopen(create ? "/sys/class/gpio/export" :
       "/sys/class/gpio/unexport" , "w");
    if (!fd)
        GPIO_ERROR("Unable to %sexport GPIO pin %d!\n", 
            create ? "export" : "unexport", pin);
    fprintf(fd, "%d", pin);
    fclose(fd);
}

int gpio_read(char pin, bool *value) {
    if (!gpio_export(pin, true)) return EXIT_FAILURE;
    if (!gpio_direction(pin, "in")) return EXIT_FAILURE;

    char path[40];
    sprintf(path, "/sys/class/gpio/gpio%d/value", pin);
    FILE *fd = fopen(path, "r");
    if (!fd)
        GPIO_ERROR("Unable to read from GPIO pin %d!\n", pin);
    *value = fgetc(fd) == '1';
    fclose(fd);

    if (!gpio_direction(pin, "out")) return EXIT_FAILURE;
    if (!gpio_export(pin, false)) return EXIT_FAILURE;

    return EXIT_SUCCESS;
}

int gpio_write(char pin, bool value) {
    if (!gpio_export(pin, true)) return EXIT_FAILURE;
    if (!gpio_direction(pin, "out")) return EXIT_FAILURE;

    char path[40];
    sprintf(path, "/sys/class/gpio/gpio%d/value", pin);
    FILE *fd = fopen(path, "w");
    if (!fd)
        GPIO_ERROR("Unable to write to GPIO pin %d!\n", pin);
    fprintf(fd, "%c", value ? "1" : "0");
    fclose(fd);

    if (!gpio_export(pin, false)) return EXIT_FAILURE;

    return EXIT_SUCCESS;
}
#endif