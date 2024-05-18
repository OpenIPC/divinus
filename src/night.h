#pragma once

#include "common.h"
#include "gpio.h"
#include "video.h"

bool night_mode_is_enabled();
void *night_thread();

int start_monitor_light_sensor();
void stop_monitor_light_sensor();