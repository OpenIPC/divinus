#pragma once

#include "fh_isp.h"

/*
 * Sensor drivers for the Fullhan ISP live in user space: the ISP core calls
 * back through an fh_isp_snrops table for exposure, gain, VTS and flip control,
 * and the driver programs the sensor over /dev/i2c-0 itself.
 */
typedef struct {
    const char *name;           /* matches the ISP tuning file prefix, e.g. "gc4653_mipi" */
    fh_common_dim dim;          /* native output size */
    unsigned int bayer;         /* value for API_ISP_SetMirrorAndflipEx() */
    int (*fnProbe)(void);       /* returns 0 if the sensor answers on the bus */
    int (*fnFormatForFramerate)(int framerate);   /* fh sensor format code */
    int (*fnFramerateForFormat)(int format);
    fh_isp_snrops *(*fnCreate)(fh_isp_impl *isp);
} fh_snr_driver;

extern fh_snr_driver fh_snr_gc4653;

static fh_snr_driver *fh_snr_drivers[] = {
    &fh_snr_gc4653,
    NULL
};
