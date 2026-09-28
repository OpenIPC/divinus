#pragma once

#define RECORD_PATH_MAX 512

#include <errno.h>
#include <pthread.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>

#include "app_config.h"
#include "fmt/mp4.h"
#include "hal/macros.h"
#include "hal/types.h"

bool record_start(void);
void record_stop(void);
bool record_active(void);
time_t record_start_time(void);
void record_ingest_stream(hal_vidstream *stream, char isH265);
