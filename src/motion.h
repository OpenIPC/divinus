#pragma once

#include <pthread.h>
#include <stdbool.h>
#include <time.h>
#include <unistd.h>

#include "app_config.h"
#include "hal/macros.h"
#include "media.h"
#include "motion_core.h"
#include "onvif_event.h"

int motion_start(void);
void motion_stop(void);
void motion_pause(int ms);
