#include "night.h"

char nightOn = 0;
static bool grayscale = false, ircut = true, irled = false, manual = false, whiteled = false;
pthread_t nightPid = 0;

bool night_grayscale_on(void) { return grayscale; }

bool night_ircut_on(void) { return ircut; }

bool night_irled_on(void) { return irled; }

bool night_manual_on(void) { return manual; }

bool night_mode_on(void) { return grayscale && !ircut && irled; }

void night_grayscale(bool enable) {
    set_grayscale(enable);
    grayscale = enable;
}

// 999 is the "not configured" default; it must never reach gpio_write(), whose
// pin argument is a char.
#define PIN_SET(pin) ((pin) >= 0 && (pin) <= 127)

void night_ircut(bool enable) {
    bool has1 = PIN_SET(app_config.ir_cut_pin1);
    bool has2 = PIN_SET(app_config.ir_cut_pin2);

    if (has1) gpio_write(app_config.ir_cut_pin1, !enable);
    if (has2) gpio_write(app_config.ir_cut_pin2, enable);
    if (has1 || has2) {
        usleep(app_config.pin_switch_delay_us * 100);
        if (has1) gpio_write(app_config.ir_cut_pin1, false);
        if (has2) gpio_write(app_config.ir_cut_pin2, false);
    }
    ircut = enable;
}

void night_irled(bool enable) {
    if (PIN_SET(app_config.ir_led_pin))
        gpio_write(app_config.ir_led_pin, enable);
    irled = enable;
}

/* Visible-light lamp (spotlight); independent of the day/night state */
bool night_whiteled_on(void) { return whiteled; }

void night_whiteled(bool enable) {
    if (PIN_SET(app_config.white_led_pin))
        gpio_write(app_config.white_led_pin, enable);
    whiteled = enable;
}

void night_manual(bool enable) { manual = enable; }

/* Auto-exposure state of the image pipeline, where the platform exposes it */
int night_exposure(unsigned int *sensorGain, unsigned int *ispGain,
    unsigned int *shutterUs, unsigned int *avgY, int *bv) {
    switch (plat) {
#if defined(__ARM_PCS_VFP)
        case HAL_PLATFORM_I3:
            return i3_exposure_info(sensorGain, ispGain, shutterUs, avgY, bv);
#endif
        default: return EXIT_FAILURE;
    }
}

void night_mode(bool enable) {
    HAL_INFO("night", "Changing mode to %s\n", enable ? "NIGHT" : "DAY");
#if defined(__arm__) && !defined(__ARM_PCS_VFP) && __ARM_ARCH == 6
    if (plat == HAL_PLATFORM_F1) {
        hal_light lamp = app_config.night_lamp;
        if (!enable) {
            night_grayscale(false);
            night_ircut(true);
            night_irled(false);
            fh_lamp_enable(HAL_LIGHT_NONE);
        } else if (lamp & HAL_LIGHT_WHITE) {
            night_grayscale(false);
            night_ircut(true);
            night_irled((lamp & HAL_LIGHT_IR) != 0);
            fh_lamp_enable(lamp);
        } else {
            night_grayscale(true);
            night_ircut(false);
            night_irled((lamp & HAL_LIGHT_IR) != 0);
            fh_lamp_enable(lamp);
        }
        return;
    }
#endif
    night_grayscale(enable);
    night_ircut(!enable);
    night_irled(enable);
}

void *night_thread(void) {
    gpio_init();
    usleep(10000);

    night_mode(night_mode_on());

#if defined(__arm__) && !defined(__ARM_PCS_VFP) && __ARM_ARCH == 6
    if (plat == HAL_PLATFORM_F1 && f1_night_available() &&
        !app_config.adc_device[0] && app_config.ir_sensor_pin == 999) {
        HAL_INFO("night", "Using image gain for day/night switching\n");
        int applied = -1;
        while (keepRunning && nightOn) {
            int state = f1_night_status();
            if (manual) applied = -1;
            else if (state != applied) { night_mode(state); applied = state; }
            usleep(40000);
        }
    } else
#endif
    if (app_config.adc_device[0]) {
        int adc_fd = -1;
        fd_set adc_fds;
        int cnt = 0, tmp = 0, val;

        if ((adc_fd = open(app_config.adc_device, O_RDONLY | O_NONBLOCK)) <= 0) {
            HAL_DANGER("night", "Could not open the ADC virtual device!\n");
            return NULL;
        }
        while (keepRunning && nightOn) {
            if (read(adc_fd, &val, sizeof(val)) > 0) {
                usleep(10000);
                tmp += val;
                cnt++;
            }
            if (cnt == 12) {
                tmp /= cnt;
                if (!manual) night_mode(tmp >= app_config.adc_threshold);
                cnt = tmp = 0;
            }
            usleep(app_config.check_interval_s * 1000000 / 12);
        }
        if (adc_fd) close(adc_fd);
    } else if (app_config.ir_sensor_pin == 999 && app_config.exposure_night &&
        !night_exposure(NULL, NULL, NULL, NULL, NULL)) {
        // No light sensor: decide from how hard the auto-exposure has to work.
        // Exposure index = sensor gain x ISP gain x shutter (relative to 40 ms).
        // raw = the same in 1/1024 steps, for comparing readings in night mode.
        unsigned int sg, ig, us, luma, over = 0, drop = 0, dwell = 0, wait = 120, base = 0;
        while (keepRunning && nightOn) {
            for (int i = 0; i < app_config.check_interval_s * 5 && keepRunning && nightOn; i++)
                usleep(200000);
            dwell++;
            if (manual || night_exposure(&sg, &ig, &us, &luma, NULL)) continue;
            unsigned int raw = (unsigned int)((unsigned long long)sg * ig / 1024 * us / 40000);
            unsigned int idx = raw / 1024;
            if (!night_mode_on()) {
                // Night: the exposure is (nearly) maxed out and the picture is
                // still well under the auto-exposure target (about 40).
                over = (idx >= app_config.exposure_night && luma < 20) ? over + 1 : 0;
                // Two readings in a row and at least six intervals in day mode
                if (over >= 2 && dwell >= 6) {
                    night_mode(true);
                    over = drop = dwell = base = 0; wait = 120;
                }
                continue;
            }
            // Night, IR lamps on: they light the scene, so the exposure says
            // nothing about daylight by itself. Remember the darkest it settles
            // at and look for it dropping (more light than the lamps give), or
            // the picture getting brighter than the auto-exposure aims for.
            // Each probe blanks the picture for a moment, so the timed ones
            // start after ten minutes and back off to about once an hour.
            if (idx > app_config.exposure_day) { drop = 0; base = 0; continue; }
            if (dwell >= 2 && raw > base) base = raw;
            drop = (raw * 10 < base * 8 || luma >= 80) ? drop + 1 : 0;
            if (dwell < 3 || (drop < 2 && dwell < wait)) continue;
            // Switch the lamps off for a moment and see whether there is light
            // without them.
            night_irled(false);
            for (int i = 0; i < 12 && keepRunning && nightOn; i++) usleep(200000);
            bool lit = !night_exposure(&sg, &ig, &us, &luma, NULL) && (luma >= 30 ||
                (unsigned int)((unsigned long long)sg * ig / 1024 * us / 40000 / 1024)
                    < app_config.exposure_night * 2 / 3);
            if (manual) continue;
            if (lit) night_mode(false);
            else {
                // Still dark: lamps back on, wait longer before the next timed probe
                night_irled(true);
                if (drop < 2 && wait < 720) wait = wait * 2 > 720 ? 720 : wait * 2;
            }
            over = drop = dwell = base = 0;
        }
    } else if (app_config.ir_sensor_pin == 999) {
        // night_disable() joins this thread, so it has to notice nightOn clearing.
        while (keepRunning && nightOn) usleep(200000);
    } else {
        while (keepRunning && nightOn) {
            bool state = false;
            if (!gpio_read(app_config.ir_sensor_pin, &state))
                if (!manual) night_mode(state);

            for (int i = 0; i < app_config.check_interval_s * 5 && keepRunning && nightOn; i++)
                usleep(200000);
        }
    }

    usleep(10000);
    gpio_deinit();
    HAL_INFO("night", "Night mode thread is closing...\n");
    nightOn = 0;
}

int night_enable(void) {
    int ret = EXIT_SUCCESS;

    if (nightOn) return ret;

    pthread_attr_t thread_attr;
    pthread_attr_init(&thread_attr);
    size_t stacksize;
    pthread_attr_getstacksize(&thread_attr, &stacksize);
    size_t new_stacksize = 16 * 1024;
    if (pthread_attr_setstacksize(&thread_attr, new_stacksize))
        HAL_DANGER("night", "Error:  Can't set stack size %zu\n", new_stacksize);
    nightOn = 1;
    pthread_create(&nightPid, &thread_attr, (void *(*)(void *))night_thread, NULL);
    if (pthread_attr_setstacksize(&thread_attr, stacksize))
        HAL_DANGER("night", "Error:  Can't set stack size %zu\n", stacksize);
    pthread_attr_destroy(&thread_attr);

    nightOn = 1;

    return ret;
}

void night_disable(void) {
    if (!nightOn) return;

    nightOn = 0;
    pthread_join(nightPid, NULL);
}
