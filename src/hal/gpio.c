#include "gpio.h"

#ifndef GPIO_SYSFS
#define GPIO_SYSFS "/sys/class/gpio"
#endif

#if LINUX_VERSION_CODE >= KERNEL_VERSION(4, 8, 0)
int fd_gpio = 0;

char gpio_count = 0;

void gpio_release(char pin) { (void)pin; }

void gpio_release_unused(const short *keep, int count) { (void)keep; (void)count; }

void gpio_deinit(void) {
    if (!fd_gpio) return;
    close(fd_gpio);
    fd_gpio = 0;
}

int gpio_init(void) {
    const char *paths[] = {"/dev/gpiochip0", "/sys/class/gpio/gpiochip0", NULL};
    const char **path = paths;
    while (*path) {
        if (access(*path++, F_OK)) continue;
        if ((fd_gpio = open(*(path - 1), O_RDWR)) < 0)
            HAL_ERROR("gpio", "Unable to open the device %s!\n", *(path - 1));
        else break;
    }

    if (!fd_gpio) return EXIT_FAILURE;

    struct gpiochip_info info;
    int ret = ioctl(fd_gpio, GPIO_GET_CHIPINFO_IOCTL, &info);
    if (ret == -1) {
        gpio_deinit();
        HAL_ERROR("gpio", "Unable to enumerate the GPIO lines!\n");
    } else gpio_count = info.lines;

    return EXIT_SUCCESS;
}

int gpio_read(char pin, bool *value) {
    struct gpiohandle_request req = { .lineoffsets[0] = pin, .lines = 1, 
        .flags = GPIOHANDLE_REQUEST_INPUT };

    int ret = ioctl(fd_gpio, GPIO_GET_LINEHANDLE_IOCTL, &req);
    if (ret == -1)
        HAL_ERROR("gpio", "Unable to request a read on GPIO pin %d (error #%d)!\n", pin, errno);

    struct gpiohandle_data data;
    ret = ioctl(req.fd, GPIOHANDLE_GET_LINE_VALUES_IOCTL, &data);
    if (ret == -1) {
        close(req.fd);
        HAL_ERROR("gpio", "Unable to read the value of GPIO pin %d (error #%d)!\n", pin, errno);
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
        HAL_ERROR("gpio", "Unable to request a write on GPIO pin %d (error #%d)!\n", pin, errno);

    close(req.fd);

    return EXIT_SUCCESS;
}
#else
void gpio_deinit(void) {}

int gpio_init(void) {
    return EXIT_SUCCESS;
}

/* The exported line's sysfs directory is "gpio<n>" on most kernels but
 * "GPIO<n>" on Fullhan kernels. The format is cached once a node proves it;
 * a pin that isn't exported yet says nothing, so it is not cached then. */
static const char *gpio_node(char pin) {
    static const char *fmt;
    static char dir[40];
    if (!fmt) {
        static const char *fmts[] = { GPIO_SYSFS "/GPIO%d", GPIO_SYSFS "/gpio%d" };
        for (int i = 0; i < 2 && !fmt; i++) {
            sprintf(dir, fmts[i], pin);
            if (access(dir, F_OK) == 0) fmt = fmts[i];
        }
        if (!fmt) {
            sprintf(dir, GPIO_SYSFS "/gpio%d", pin);
            return dir;
        }
    }
    sprintf(dir, fmt, pin);
    return dir;
}

static inline int gpio_direction(char pin, char *mode) {
    char path[48];
    sprintf(path, "%s/direction", gpio_node(pin));
    int fd = open(path, O_WRONLY);
    if (fd < 0)
        HAL_ERROR("gpio", "Unable to control the direction of GPIO pin %d!\n", pin);
    if (write(fd, mode, strlen(mode)) < 0) {
        close(fd);
        HAL_ERROR("gpio", "Unable to set the direction of GPIO pin %d!\n", pin);
    }

    close(fd);
    return EXIT_SUCCESS;
}

static inline int gpio_export(char pin, bool create) {
    char path[40];
    int fd = open(create ? GPIO_SYSFS "/export" :
       GPIO_SYSFS "/unexport", O_WRONLY);
    if (fd < 0)
        HAL_ERROR("gpio", "Unable to (un)export a GPIO pin!\n");

    char val[4];
    sprintf(val, "%d", pin);
    if (write(fd, val, strlen(val)) < 0) {
        close(fd);
        HAL_ERROR("gpio", "Unable to %s GPIO pin %d!\n", 
            create ? "export" : "unexport", pin);
    }

    close(fd);
    return EXIT_SUCCESS;
}

/* The pins this process exported itself. gpio_write() leaves its pin exported (freeing the line can turn it back
 * into an input and drop the level), so something has to let it go later; only pins listed here are ever
 * unexported, never one that was already exported when we got to it, which belongs to somebody else. */
static unsigned char gpio_owned[32];
#define GPIO_BIT(p)      (1 << ((unsigned char)(p) & 7))
#define GPIO_BYTE(p)     gpio_owned[(unsigned char)(p) >> 3]
#define GPIO_OWNED(p)    (GPIO_BYTE(p) & GPIO_BIT(p))
#define GPIO_OWN(p)      __atomic_fetch_or(&GPIO_BYTE(p), GPIO_BIT(p), __ATOMIC_RELAXED)
#define GPIO_DISOWN(p)   __atomic_fetch_and(&GPIO_BYTE(p), ~GPIO_BIT(p), __ATOMIC_RELAXED)

/* Let go of a pin gpio_write() exported. A pin this process did not export is left alone. */
void gpio_release(char pin) {
    if (!GPIO_OWNED(pin)) return;
    if (access(gpio_node(pin), F_OK) == 0) gpio_export(pin, false);
    GPIO_DISOWN(pin);
}

/* Let go of every pin this process exported, except the ones listed as still in use */
void gpio_release_unused(const short *keep, int count) {
    for (int pin = 0; pin < 128; pin++) {
        if (!GPIO_OWNED(pin)) continue;
        bool used = false;
        for (int i = 0; i < count; i++) used |= keep[i] == pin;
        if (!used) gpio_release((char)pin);
    }
}

int gpio_read(char pin, bool *value) {
    gpio_export(pin, true);
    if (gpio_direction(pin, "in")) return EXIT_FAILURE;

    char path[48];
    sprintf(path, "%s/value", gpio_node(pin));
    int fd = open(path, O_RDONLY);
    if (fd < 0)
        HAL_ERROR("gpio", "Unable to read from GPIO pin %d!\n", pin);

    char val = 0;
    lseek(fd, 0, SEEK_SET);
    read(fd, &val, 0);
    if (!val) {
        close(fd);
        HAL_ERROR("gpio", "Unable to read from GPIO pin %d!\n", pin);
    }
    *value = val - 0x30;
    close(fd);

    if (gpio_direction(pin, "out")) return EXIT_FAILURE;
    if (gpio_export(pin, false)) return EXIT_FAILURE;
    GPIO_DISOWN(pin);

    return EXIT_SUCCESS;
}

int gpio_write(char pin, bool value) {
    if (access(gpio_node(pin), F_OK) && gpio_export(pin, true) == EXIT_SUCCESS) GPIO_OWN(pin);
    if (gpio_direction(pin, "out")) return EXIT_FAILURE;

    char path[48];
    sprintf(path, "%s/value", gpio_node(pin));
    int fd = open(path, O_WRONLY);
    if (fd < 0)
        HAL_ERROR("gpio", "Unable to write to GPIO pin %d!\n", pin);

    char val = value ? '1' : '0';
    if (write(fd, &val, 1) < 0) {
        close(fd);
        HAL_ERROR("gpio", "Unable to write to GPIO pin %d!\n", pin);
    }
    close(fd);

    /* Stay exported: freeing the line can turn it back into an input
     * (the pin controller does this on some kernels), dropping the level. */
    return EXIT_SUCCESS;
}
#endif