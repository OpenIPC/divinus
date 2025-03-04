#include "night.h"

static bool night_mode = false;
pthread_t nightPid = 0;

bool night_mode_is_enabled() { return night_mode; }

void ircut_on() {
    gpio_write(app_config.ir_cut_pin1, false);
    gpio_write(app_config.ir_cut_pin2, true);
    usleep(app_config.pin_switch_delay_us * 100);
    gpio_write(app_config.ir_cut_pin1, false);
    gpio_write(app_config.ir_cut_pin2, false);
}

void ircut_off() {
    gpio_write(app_config.ir_cut_pin1, true);
    gpio_write(app_config.ir_cut_pin2, false);
    usleep(app_config.pin_switch_delay_us * 100);
    gpio_write(app_config.ir_cut_pin1, false);
    gpio_write(app_config.ir_cut_pin2, false);
}

void set_night_mode(bool night) {
    if (night == night_mode) return;
    if (night) {
        HAL_INFO("night", "Changing mode to NIGHT\n");
        ircut_off();
        gpio_write(app_config.ir_led_pin, true);
        set_grayscale(true);
    } else {
        HAL_INFO("night", "Changing mode to DAY\n");
        ircut_on();
        gpio_write(app_config.ir_led_pin, false);
        set_grayscale(false);
    }
    night_mode = night;
}

void *night_thread(void) {
    gpio_init();
    usleep(10000);

    set_night_mode(night_mode);

    if (app_config.adc_device[0]) {
        int adc_fd = -1;
        fd_set adc_fds;
        int cnt = 0, tmp = 0, val;

        if ((adc_fd = open(app_config.adc_device, O_RDONLY | O_NONBLOCK)) <= 0) {
            HAL_DANGER("night", "Could not open the ADC virtual device!\n");
            return NULL;
        }
        while (keepRunning) {
            if (read(adc_fd, &val, sizeof(val)) > 0) {
                usleep(10000);
                tmp += val;
                cnt++;
            }
            if (cnt == 12) {
                tmp /= cnt;
                set_night_mode(tmp >= app_config.adc_threshold);
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
            if (!gpio_read(app_config.ir_sensor_pin, &state)) {
                sleep(app_config.check_interval_s);
                continue;
            }
            set_night_mode(night_mode);
            sleep(app_config.check_interval_s);
        }
    }
    usleep(10000);
    gpio_deinit();
    HAL_INFO("night", "Night mode thread is closing...\n");
}

int start_monitor_light_sensor() {
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
}

void stop_monitor_light_sensor() {
    pthread_join(nightPid, NULL);
}