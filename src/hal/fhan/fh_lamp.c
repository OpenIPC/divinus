#if defined(__arm__) && !defined(__ARM_PCS_VFP) && __ARM_ARCH == 6

#include "fh_lamp.h"

static void fh_lamp_proc_write(const char *path, const char *value) {
    FILE *file = fopen(path, "w");
    if (!file) {
        HAL_WARNING("fh_lamp", "Cannot write %s to %s!\n", value, path);
        return;
    }
    fputs(value, file);
    fclose(file);
}

int fh_lamp_enable(int flags) {
    fh_lamp_proc_write("/proc/driver/pwm",
        (flags & HAL_LIGHT_IR) ? FH_PWM_ON("3", "8") : FH_PWM_OFF("3", "8"));
    fh_lamp_proc_write("/proc/driver/pwm",
        (flags & HAL_LIGHT_WHITE) ? FH_PWM_ON("7", "128") : FH_PWM_OFF("7", "128"));
    gpio_write(FH_LAMP_WHITE_GPIO, (flags & HAL_LIGHT_WHITE) ? 1 : 0);
    return EXIT_SUCCESS;
}

#endif
