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
    if (night) {
        printf(tag "Change mode to NIGHT\n");
        ircut_off();
        set_grayscale(true);
    } else {
        printf(tag "Change mode to DAY\n");
        ircut_on();
        set_grayscale(false);
    }
}

void *night_thread(void) {
    usleep(1000);
    set_night_mode(night_mode);

    while (keepRunning) {
        bool state = false;
        if (!gpio_read(app_config.ir_sensor_pin, &state)) {
            sleep(app_config.check_interval_s);
            continue;
        }
        if (night_mode != state) {
            night_mode = state;
            set_night_mode(night_mode);
        }
        sleep(app_config.check_interval_s);
    }
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