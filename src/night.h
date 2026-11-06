#pragma once

#include <pthread.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/ioctl.h>
#include <unistd.h>

#include "app_config.h"
#include "hal/gpio.h"
#include "hal/macros.h"
#include "hal/support.h"
#include "media.h"

bool night_grayscale_on(void);
bool night_ircut_on(void);
bool night_irled_on(void);
bool night_manual_on(void);
bool night_mode_on(void);

void night_grayscale(bool enable);
void night_ircut(bool enable);
void night_irled(bool enable);
bool night_whiteled_on(void);
void night_whiteled(bool enable);
void night_manual(bool enable);
int night_exposure(unsigned int *sensorGain, unsigned int *ispGain,
    unsigned int *shutterUs, unsigned int *avgY, int *bv);
void night_mode(bool enable);

void night_disable(void);
int night_enable(void);
