#pragma once

typedef enum {
    GM_CAP_OUT_DISP,
    GM_CAP_OUT_VIDOUT,
    GM_CAP_OUT_SCALER1,
    GM_CAP_OUT_SCALER2
} gm_cap_out;

typedef struct {
    int internal[8];
    int channel;
    // Scaler 2 generally cannot scale down
    gm_cap_out output;
    int motionDataOn;
    // Leave to zero to disable
    int intl2ProgThres;
    unsigned int dmaPath;
    unsigned short prescaleWidth;
    unsigned short prescaleHeight;
    int reserved[2];
} gm_cap_cnf;