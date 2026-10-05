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
    gpio_write(app_config.ir_led_pin, enable);
    irled = enable;
}

void night_manual(bool enable) { manual = enable; }

void night_mode(bool enable) {
    HAL_INFO("night", "Changing mode to %s\n", enable ? "NIGHT" : "DAY");
    night_grayscale(enable);
    night_ircut(!enable);
    night_irled(enable);
}

void *night_thread(void) {
    unsigned int gain;

    gpio_init();
    usleep(10000);

    if (!manual) night_mode(night_mode_on());

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
    } else if (app_config.ir_sensor_pin == 999 && get_isp_gain(&gain)) {
        HAL_WARNING("night", "No ISP gain on this platform, automatic switching disabled!\n");
    } else if (app_config.ir_sensor_pin == 999) {
        bool night = night_mode_on(), gain_lost = false;
        unsigned int held = 0, since_pulse = 0;

        while (keepRunning && nightOn) {
            sleep(1);
            if (manual) {
                held = 0;
                continue;
            }
            // A missed pulse leaves the filter out by day, which lowers the gain
            // so no crossing ever corrects it: pulse the current position again
            if (++since_pulse >= 600) {
                night_ircut(!night);
                since_pulse = 0;
            }
            if (get_isp_gain(&gain)) {
                if (!gain_lost)
                    HAL_WARNING("night", "Could not read the ISP gain, retrying...\n");
                gain_lost = true;
                held = 0;
                continue;
            }
            gain_lost = false;

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
