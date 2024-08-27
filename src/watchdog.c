#include "watchdog.h"

int fd = 0;

void watchdog_reset(void) {
    if (!fd) return;
    write(fd, "", 1);
}

int watchdog_start(int timeout) {
    if (fd) return EXIT_SUCCESS;
    const char* paths[] = {"/dev/watchdog0", "/dev/watchdog"};
    const char **path = paths;

    while (*path) {
        if (access(*path++, F_OK)) continue;
        if ((fd = open(*(path - 1), O_WRONLY)) == -1)
            HAL_ERROR("watchdog", "%s could not be opened!\n", *(path - 1));
        break;
    } if (!fd) HAL_ERROR("watchdog", "No matching device has been found!\n");

    ioctl(fd, WDIOC_SETTIMEOUT, &timeout);

    HAL_INFO("watchdog", "Watchdog started!\n");
    return EXIT_SUCCESS;
}

void watchdog_stop(void) {
    if (!fd) return;
    write(fd, "V", 1);
    close(fd);
    fd = 0;

    HAL_INFO("watchdog", "Watchdog stopped!\n");
}