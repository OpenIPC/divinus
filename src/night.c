#include "night.h"

char nightOn = 0;
static bool grayscale = false, ircut = true, irled = false, manual = false;
pthread_t nightPid = 0;

bool night_grayscale_on(void) { return grayscale; }

bool night_ircut_on(void) { return ircut; }

bool night_irled_on(void) { return irled; }

bool night_manual_on(void) { return manual; }

bool night_mode_on(void) { return grayscale && !ircut && irled; }

void night_grayscale(bool enable) {
    set_grayscale(enable);
    grayscale = enable;
}

void night_ircut(bool enable) {
    bool has1 = app_config.ir_cut_pin1 != 999;
    bool has2 = app_config.ir_cut_pin2 != 999;

    if (has1) gpio_write(app_config.ir_cut_pin1, !enable);
    if (has2) gpio_write(app_config.ir_cut_pin2, enable);
    if (has1 || has2) {
        usleep(app_config.pin_switch_delay_us * 100);
        if (has1) gpio_write(app_config.ir_cut_pin1, false);
        if (has2) gpio_write(app_config.ir_cut_pin2, false);
    }
    ircut = enable;
}

void night_irled(bool enable) {
    if (app_config.ir_led_pin != 999)
        gpio_write(app_config.ir_led_pin, enable);
    irled = enable;
}

void night_manual(bool enable) { manual = enable; }

void night_mode(bool enable) {
    HAL_INFO("night", "Changing mode to %s\n", enable ? "NIGHT" : "DAY");
#if defined(__arm__) && !defined(__ARM_PCS_VFP) && __ARM_ARCH == 6
    if (plat == HAL_PLATFORM_F1) {
        hal_light lamp = app_config.night_lamp;
        if (!enable) {
            night_grayscale(false);
            night_ircut(true);
            night_irled(false);
            fh_lamp_enable(HAL_LIGHT_NONE);
        } else if (lamp & HAL_LIGHT_WHITE) {
            night_grayscale(false);
            night_ircut(true);
            night_irled((lamp & HAL_LIGHT_IR) != 0);
            fh_lamp_enable(lamp);
        } else {
            night_grayscale(true);
            night_ircut(false);
            night_irled((lamp & HAL_LIGHT_IR) != 0);
            fh_lamp_enable(lamp);
        }
        return;
    }
#endif
    night_grayscale(enable);
    night_ircut(!enable);
    night_irled(enable);
}

void *night_thread(void) {
    gpio_init();
    usleep(10000);

    night_mode(night_mode_on());

#if defined(__arm__) && !defined(__ARM_PCS_VFP) && __ARM_ARCH == 6
    if (plat == HAL_PLATFORM_F1 && f1_night_available() &&
        !app_config.adc_device[0] && app_config.ir_sensor_pin == 999) {
        HAL_INFO("night", "Using image gain for day/night switching\n");
        int applied = -1;
        while (keepRunning && nightOn) {
            int state = f1_night_status();
            if (manual) applied = -1;
            else if (state != applied) { night_mode(state); applied = state; }
            usleep(40000);
        }
    } else
#endif
    if (app_config.adc_device[0]) {
        int adc_fd = -1;
        fd_set adc_fds;
        int cnt = 0, tmp = 0, val;

        if ((adc_fd = open(app_config.adc_device, O_RDONLY | O_NONBLOCK)) <= 0) {
            HAL_DANGER("night", "Could not open the ADC virtual device!\n");
            return NULL;
        }
        while (keepRunning && nightOn) {
            if (read(adc_fd, &val, sizeof(val)) > 0) {
                usleep(10000);
                tmp += val;
                cnt++;
            }
            if (cnt == 12) {
                tmp /= cnt;
                if (!manual) night_mode(tmp >= app_config.adc_threshold);
                cnt = tmp = 0;
            }
            usleep(app_config.check_interval_s * 1000000 / 12);
        }
        if (adc_fd) close(adc_fd);
    } else if (app_config.ir_sensor_pin == 999) {
        bool night = night_mode_on();
        unsigned int held = 0, gain;

        while (keepRunning && nightOn) {
            sleep(1);
            if (manual) {
                held = 0;
                continue;
            }
            if (get_isp_gain(&gain)) {
                HAL_WARNING("night", "No ISP gain on this platform, automatic switching disabled!\n");
                break;
            }

            bool crossing = night ?
                gain < app_config.day_gain * 1024 : gain >= app_config.night_gain * 1024;
            if (!crossing) {
                held = 0;
                continue;
            }
            if (++held < (night ? app_config.day_hold_s : app_config.night_hold_s))
                continue;

            night = !night;
            held = 0;
            motion_pause(3000);
            night_mode(night);
        }
    } else {
        while (keepRunning && nightOn) {
            bool state = false;
            if (!gpio_read(app_config.ir_sensor_pin, &state))
                if (!manual) night_mode(state);

            for (int i = 0; i < app_config.check_interval_s && keepRunning && nightOn; i++)
                sleep(1);
        }
    }

    usleep(10000);
    gpio_deinit();
    HAL_INFO("night", "Night mode thread is closing...\n");
}

int night_enable(void) {
    int ret = EXIT_SUCCESS;

    if (nightOn) return ret;

    pthread_attr_t thread_attr;
    pthread_attr_init(&thread_attr);
    size_t stacksize;
    pthread_attr_getstacksize(&thread_attr, &stacksize);
    size_t new_stacksize = 16 * 1024;
    if (pthread_attr_setstacksize(&thread_attr, new_stacksize))
        HAL_DANGER("night", "Error:  Can't set stack size %zu\n", new_stacksize);
    nightOn = 1;
    if (pthread_create(&nightPid, &thread_attr, (void *(*)(void *))night_thread, NULL)) {
        HAL_DANGER("night", "Can't create thread\n");
        nightOn = 0;
        ret = EXIT_FAILURE;
    }
    if (pthread_attr_setstacksize(&thread_attr, stacksize))
        HAL_DANGER("night", "Error:  Can't set stack size %zu\n", stacksize);
    pthread_attr_destroy(&thread_attr);

    return ret;
}

void night_disable(void) {
    if (!nightOn) return;

    nightOn = 0;
    pthread_join(nightPid, NULL);
}
