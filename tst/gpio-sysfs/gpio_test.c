/* The sysfs GPIO path (kernels older than 4.8) against a fake sysfs in a
 * temp directory. A write to "export" makes the pin's node, one to
 * "unexport" removes it; the node is "gpio<n>" or, as on Fullhan kernels,
 * "GPIO<n>". */
#define _GNU_SOURCE
#define FAKE_ROOT "/tmp/gpio-sysfs-test"
#define GPIO_SYSFS FAKE_ROOT
#include "check.h"

#include <dirent.h>
#include <sys/stat.h>

#include "hal/gpio.h"
#include "hal/macros.h"

static const char *root = FAKE_ROOT;
static int upperNodes;          /* the fake kernel names nodes GPIO<n> */
static int specialFd = -1;      /* the open export/unexport file */
static int specialCreates;
static int exportCalls, unexportCalls;

static int fake_close(int fd) {
    if (fd == specialFd) specialFd = -1;      /* the number is reused by later opens */
    return close(fd);
}

static int fake_open(const char *path, int flags, ...) {
    int create = !strcmp(path, GPIO_SYSFS "/export");
    int destroy = !strcmp(path, GPIO_SYSFS "/unexport");
    if (create || destroy) {
        int fd = open("/dev/null", O_WRONLY);
        specialFd = fd;
        specialCreates = create;
        return fd;
    }
    return open(path, flags);
}

static void make_node(int pin) {
    char dir[96], file[112];
    snprintf(dir, sizeof(dir), "%s/%s%d", root, upperNodes ? "GPIO" : "gpio", pin);
    mkdir(dir, 0755);
    snprintf(file, sizeof(file), "%s/direction", dir);
    close(open(file, O_WRONLY | O_CREAT, 0644));
    snprintf(file, sizeof(file), "%s/value", dir);
    close(open(file, O_WRONLY | O_CREAT, 0644));
}

static ssize_t fake_write(int fd, const void *buf, size_t len) {
    if (fd != specialFd) return write(fd, buf, len);
    int pin = atoi(buf);
    char dir[96], file[112];
    snprintf(dir, sizeof(dir), "%s/%s%d", root, upperNodes ? "GPIO" : "gpio", pin);
    if (specialCreates) {
        exportCalls++;
        mkdir(dir, 0755);
        snprintf(file, sizeof(file), "%s/direction", dir);
        close(open(file, O_WRONLY | O_CREAT, 0644));
        snprintf(file, sizeof(file), "%s/value", dir);
        close(open(file, O_WRONLY | O_CREAT, 0644));
    } else {
        unexportCalls++;
        snprintf(file, sizeof(file), "%s/direction", dir);
        unlink(file);
        snprintf(file, sizeof(file), "%s/value", dir);
        unlink(file);
        rmdir(dir);
    }
    return len;
}

/* The sysfs path is the one built for kernels older than 4.8; gpio.h pulls in
 * <linux/version.h> itself, so the version is set after it */
#undef LINUX_VERSION_CODE
#define LINUX_VERSION_CODE KERNEL_VERSION(3, 10, 104)
#define open fake_open
#define write fake_write
#define close fake_close
#include "hal/gpio.c"
#undef open
#undef write
#undef close

static int exists(const char *name, int pin) {
    char path[96];
    snprintf(path, sizeof(path), "%s/%s%d", root, name, pin);
    return access(path, F_OK) == 0;
}

static void run(int upper) {
    char cmd[96];
    upperNodes = upper;
    exportCalls = unexportCalls = 0;
    const char *node = upper ? "GPIO" : "gpio";

    /* First write to a pin that isn't exported yet: this is where a probe
     * that cached the wrong name used to break the Fullhan case */
    CHECK(gpio_write(5, true) == EXIT_SUCCESS);
    CHECK(exists(node, 5));
    CHECK(exportCalls == 1 && unexportCalls == 0);

    /* The pin stays exported, and later writes don't export again */
    CHECK(gpio_write(5, false) == EXIT_SUCCESS);
    CHECK(gpio_write(5, true) == EXIT_SUCCESS);
    CHECK(exists(node, 5));
    CHECK(exportCalls == 1 && unexportCalls == 0);

    /* Letting go of it, and of one that was never exported */
    gpio_release(5);
    CHECK(!exists(node, 5));
    CHECK(unexportCalls == 1);
    gpio_release(5);
    gpio_release(9);
    CHECK(unexportCalls == 1);

    /* A pin somebody else exported is never ours to unexport: writing to it works, but releasing it, by name or
     * with everything-unused, leaves it alone */
    make_node(9);
    CHECK(gpio_write(9, true) == EXIT_SUCCESS);
    CHECK(exportCalls == 1);
    gpio_release(9);
    gpio_release_unused(NULL, 0);
    CHECK(exists(node, 9));
    CHECK(unexportCalls == 1);

    /* Everything this process exported goes, except what is still in use. This is also the case of a pin assigned
     * and driven in one request and not kept: it was exported here, so it is let go of */
    CHECK(gpio_write(5, true) == EXIT_SUCCESS);
    CHECK(gpio_write(6, true) == EXIT_SUCCESS);
    CHECK(gpio_write(7, true) == EXIT_SUCCESS);
    short keep[] = { 6, 999 };
    gpio_release_unused(keep, 2);
    CHECK(!exists(node, 5) && exists(node, 6) && !exists(node, 7));
    CHECK(exists(node, 9));
    gpio_release_unused(NULL, 0);
    CHECK(!exists(node, 6));
    CHECK(exists(node, 9));
    int after = unexportCalls;
    gpio_release_unused(NULL, 0);
    CHECK(unexportCalls == after);

    /* A pin let go of and exported again later is ours again */
    CHECK(gpio_write(5, true) == EXIT_SUCCESS);
    gpio_release_unused(NULL, 0);
    CHECK(!exists(node, 5));

    snprintf(cmd, sizeof(cmd), "rm -rf '%s'/*", root);
    CHECK(system(cmd) == 0);
}

int main(void) {
    if (system("rm -rf " FAKE_ROOT)) {}
    CHECK(mkdir(root, 0755) == 0);

    /* The node-name format is cached for good once one has been seen, so
     * each kernel flavour needs its own process */
    int upper = getenv("GPIO_TEST_UPPER") != NULL;
    run(upper);

    char cmd[96];
    snprintf(cmd, sizeof(cmd), "rm -rf '%s'", root);
    if (system(cmd)) {}
    CHECK_DONE();
}
