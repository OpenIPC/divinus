#pragma once

#include "common.h"
#include "gpio.h"
#include "media.h"

void set_night_mode(bool night);
bool night_mode_is_enabled();
void *night_thread();

int start_monitor_light_sensor();
void stop_monitor_light_sensor();