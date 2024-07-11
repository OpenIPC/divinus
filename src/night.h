#pragma once

#include "app_config.h"
#include "gpio.h"
#include "hal/macros.h"
#include "media.h"

#include <pthread.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/ioctl.h>
#include <unistd.h>

void set_night_mode(bool night);
bool night_mode_is_enabled();
void *night_thread();

int start_monitor_light_sensor();
void stop_monitor_light_sensor();