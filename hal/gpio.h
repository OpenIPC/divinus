#include <linux/gpio.h>
#include <sys/ioctl.h>

#define GPIO_ERROR(x, ...) fprintf(stderr, "%s \033[31m%s\033[0m\n", "[gpio] (x)", ##__VA_ARGS__)

const char *paths[] = {"/dev/gpiochip0", "/sys/class/gpio/gpiochip0"};
char **path = paths;
int fd_gpio = NULL;

char gpio_count = 0;

void gpio_deinit(void) {
    if (!fd_gpio) return;
    close(fd_gpio);
    fd_gpio = NULL;
}

int gpio_init(void) {
    while (*path++) {
        if (access(*path, F_OK)) continue;
        fd_gpio = open(*path, O_RDWR);
        if (fd_gpio < 0) {
            GPIO_ERROR("Unable to open the GPIO device!\n");
            return EXIT_FAILURE;
        } else break;
    }

    if (!fd_gpio) return EXIT_FAILURE;

    struct gpiochip_info info;
    int ret = ioctl(fd_gpio, GPIO_GET_CHIPINFO_IOCTL, &info);
    if (ret == MAP_FAILED) {
        GPIO_ERROR("Unable to enumerate the GPIO lines!\n");
        gpio_deinit();
        return EXIT_FAILURE;
    } else gpio_count = info.lines;

    return EXIT_SUCCESS;
}

int gpio_read(char pin, bool *value) {
    struct gpiohandle_request req = { .lineoffsets = { pin }, .lines = 1, 
        .flags = GPIOHANDLE_REQUEST_INPUT };

    int ret = ioctl(fd_gpio, GPIO_GET_LINEHANDLE_IOCTL, &req);
    if (ret == MAP_FAILED) {
        GPIO_ERROR("Unable to request a read on GPIO pin %d!\n", pin);
        return EXIT_FAILURE;
    }

    struct gpiohandle_data data;
    int ret = ioctl(fd_gpio, GPIOHANDLE_GET_LINE_VALUES_IOCTL, &data);
    if (ret == MAP_FAILED) {
        GPIO_ERROR("Unable to read the value of GPIO pin %d!\n", pin);
        close(req.fd);
        return EXIT_FAILURE;
    }

    *value = data.values[0];
    close(req.fd);
    return EXIT_SUCCESS;
}

int gpio_write(char pin, bool value) {
    struct gpiohandle_request req = { .lineoffsets = { pin }, .lines = 1, 
        .flags = GPIOHANDLE_REQUEST_OUTPUT };

    int ret = ioctl(fd_gpio, GPIO_GET_LINEHANDLE_IOCTL, &req);
    if (ret == MAP_FAILED) {
        GPIO_ERROR("Unable to request a write on GPIO pin %d!\n", pin);
        return EXIT_FAILURE;
    }

    struct gpiohandle_data data = { .values = { value } };
    int ret = ioctl(fd_gpio, GPIOHANDLE_SET_LINE_VALUES_IOCTL, &data);
    if (ret == MAP_FAILED) {
        GPIO_ERROR("Unable to write a value to GPIO pin %d!\n", pin);
        close(req.fd);
        return EXIT_FAILURE;
    }

    close(req.fd);
    return EXIT_SUCCESS;
}