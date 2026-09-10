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
    gpio_write(app_config.ir_cut_pin1, !enable);
    gpio_write(app_config.ir_cut_pin2, enable);
    usleep(app_config.pin_switch_delay_us * 100);
    gpio_write(app_config.ir_cut_pin1, false);
    gpio_write(app_config.ir_cut_pin2, false);
    ircut = enable;
}

void night_irled(bool enable) {
#if defined(__arm__) && !defined(__ARM_PCS_VFP) && __ARM_ARCH == 6
    if (plat == HAL_PLATFORM_FH) fh_irled(enable); /* PWM3 brightness; GPIO7 enable follows */
#endif
    gpio_write(app_config.ir_led_pin, enable);
    irled = enable;
}

void night_manual(bool enable) { manual = enable; }

void night_mode(bool enable) {
    HAL_INFO("night", "Changing mode to %s\n", enable ? "NIGHT" : "DAY");
#if defined(__arm__) && !defined(__ARM_PCS_VFP) && __ARM_ARCH == 6
    if (plat == HAL_PLATFORM_FH && EQUALS(app_config.night_lamp, "white")) {
        /* Colour night vision: light the scene with the white lamp and keep the
         * IR-cut filter in and the image in colour; the IR lamp stays off */
        night_grayscale(false);
        night_ircut(true);
        night_irled(false);
        fh_whitelamp(enable);
        return;
    }
    if (plat == HAL_PLATFORM_FH && EQUALS(app_config.night_lamp, "none")) {
        night_grayscale(enable);
        night_ircut(!enable);
        night_irled(false);
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
    if (plat == HAL_PLATFORM_FH && fh_night_available()) {
        HAL_INFO("night", "Using SmartIR (image gain) for day/night switching\n");
        if (app_config.smartir_gain_night || app_config.smartir_gain_day)
            fh_night_thresholds(app_config.smartir_gain_night, app_config.smartir_gain_day);
        while (keepRunning && nightOn) {
            /* SmartIR is polled; only touch the IR-cut/LED GPIOs when the verdict changes,
             * otherwise night_mode() pulses the IR-cut solenoid every interval */
            static int applied = -1;
            int state = fh_night_status();
            if (manual) applied = -1;
            else if (state != applied) { night_mode(state); applied = state; }
            /* SmartIR's debounce counters are per call and sized for the vendor's
             * 40 ms polling loop, so don't pace it with check_interval_s */
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
        while (keepRunning) sleep(1);
    } else {
        while (keepRunning) {
            bool state = false;
            if (!gpio_read(app_config.ir_sensor_pin, &state))
                if (!manual) night_mode(state);

            sleep(app_config.check_interval_s);
        }
    }

    usleep(10000);
    gpio_deinit();
    HAL_INFO("night", "Night mode thread is closing...\n");
    nightOn = 0;
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
    pthread_create(&nightPid, &thread_attr, (void *(*)(void *))night_thread, NULL);
    if (pthread_attr_setstacksize(&thread_attr, stacksize))
        HAL_DANGER("night", "Error:  Can't set stack size %zu\n", stacksize);
    pthread_attr_destroy(&thread_attr);

    nightOn = 1;

    return ret;
}

void night_disable(void) {
    if (!nightOn) return;

    nightOn = 0;
    pthread_join(nightPid, NULL);
}
