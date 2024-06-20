#include "night.h"

#include <pthread.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/ioctl.h>
#include <unistd.h>

#define tag "[night] "

static bool night_mode = false;
pthread_t nightPid = 0;

bool night_mode_is_enabled() { return night_mode; }

void ircut_on() {
    gpio_write(app_config.ir_cut_pin1, false);
    gpio_write(app_config.ir_cut_pin2, true);
    usleep(app_config.pin_switch_delay_us);
    gpio_write(app_config.ir_cut_pin1, false);
    gpio_write(app_config.ir_cut_pin2, false);
    set_grayscale(true);
}

void ircut_off() {
    gpio_write(app_config.ir_cut_pin1, true);
    gpio_write(app_config.ir_cut_pin2, false);
    usleep(app_config.pin_switch_delay_us);
    gpio_write(app_config.ir_cut_pin1, false);
    gpio_write(app_config.ir_cut_pin2, false);
    set_grayscale(false);
}

void set_night_mode(bool night) {
    if (night == night_mode) return;
    if (night) {
        printf(tag "Change mode to NIGHT\n");
        ircut_off();
        set_grayscale(true);
    } else {
        printf(tag "Change mode to DAY\n");
        ircut_on();
        set_grayscale(false);
    }
    night_mode = night;
}

void *night_thread(void) {
    usleep(1000);
    set_night_mode(night_mode);

    if (app_config.adc_device[0]) {
        int adc_fd = -1;
        fd_set adc_fds;
        int cnt = 0, tmp = 0, val;

        if ((adc_fd = open(app_config.adc_device, O_RDONLY | O_NONBLOCK)) <= 0) {
            printf(tag "Could not open the ADC virtual device!\n");
            return NULL;
        }
        while (keepRunning) {
            struct timeval tv = { 
                .tv_sec = app_config.check_interval_s, .tv_usec = 0 };
            FD_ZERO(&adc_fds);
            FD_SET(adc_fd, &adc_fds);
            select(adc_fd + 1, &adc_fds, NULL, NULL, &tv);
            if (read(adc_fd, &val, sizeof(val)) > 0) {
                usleep(10000);
                tmp += val;
            }
            cnt++;
            if (cnt == 12) {
                tmp /= cnt;
                set_night_mode(tmp >= app_config.adc_threshold);
                cnt = tmp = 0;
            }
            usleep(250000);
        }
        if (adc_fd) close(adc_fd);
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
    printf(tag "Night mode thread is closing...\n");
}

int start_monitor_light_sensor() {
    pthread_attr_t thread_attr;
    pthread_attr_init(&thread_attr);
    size_t stacksize;
    pthread_attr_getstacksize(&thread_attr, &stacksize);
    size_t new_stacksize = 16 * 1024;
    if (pthread_attr_setstacksize(&thread_attr, new_stacksize)) {
        printf(tag "Error:  Can't set stack size %zu\n", new_stacksize);
    }
    pthread_create(&nightPid, &thread_attr, (void *(*)(void *))night_thread, NULL);
    if (pthread_attr_setstacksize(&thread_attr, stacksize)) {
        printf(tag "Error:  Can't set stack size %zu\n", stacksize);
    }
    pthread_attr_destroy(&thread_attr);
}

void stop_monitor_light_sensor() {
    pthread_join(nightPid, NULL);
}