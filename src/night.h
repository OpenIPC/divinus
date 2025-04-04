#pragma once

#include <pthread.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/ioctl.h>
#include <unistd.h>

#include "app_config.h"
#include "gpio.h"
#include "hal/macros.h"
#include "media.h"

bool night_grayscale_on(void);
bool night_ircut_on(void);
bool night_irled_on(void);
bool night_manual_on(void);
bool night_mode_on(void);

void night_grayscale(bool enable);
void night_ircut(bool enable);
void night_irled(bool enable);
void night_manual(bool enable);
void night_mode(bool enable);

void disable_night(void);
int enable_night(void);