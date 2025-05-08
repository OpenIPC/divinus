#pragma once

#include <time.h>

#include "app_config.h"
#include "fmt/mp4.h"
#include "hal/macros.h"
#include "hal/types.h"

void record_start(void);
void record_stop(void);
void send_mp4_to_record(hal_vidstream *stream, char isH265);