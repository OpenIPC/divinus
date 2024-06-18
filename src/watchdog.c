#include "watchdog.h"

int fd = 0;

void watchdog_reset(void) {
    if (!fd) return;
    write(fd, "", 1);
}

int watchdog_start(int timeout) {
    if (fd) return EXIT_SUCCESS;
    char* paths[] = {"/dev/watchdog0", "/dev/watchdog"};
    char **path = paths;

    while (*path) {
        if (access(*path++, F_OK)) continue;
        if ((fd = open(*(path - 1), O_WRONLY)) == -1)
            WATCHDOG_ERROR("%s could not be opened!\n", *(path - 1), fd--);
        break;
    } if (!fd) WATCHDOG_ERROR("No matching device has been found!\n");

    ioctl(fd, WDIOC_SETTIMEOUT, &timeout);

    fprintf(stderr, "[watchdog] Watchdog started!\n");
    return EXIT_SUCCESS;
}

void watchdog_stop(void) {
    if (!fd) return;
    write(fd, "V", 1);
    close(fd);
    fd = 0;

    fprintf(stderr, "[watchdog] Watchdog stopped!\n");
}