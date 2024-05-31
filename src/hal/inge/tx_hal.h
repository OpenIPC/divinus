#pragma once

#include "tx_common.h"
#include "tx_aud.h"
#include "tx_fs.h"
#include "tx_isp.h"
#include "tx_osd.h"
#include "tx_sys.h"
#include "tx_venc.h"

extern char keepRunning;

extern hal_chnstate tx_state[TX_VENC_CHN_NUM];
extern int (*tx_venc_cb)(char, hal_vidstream*);

void tx_hal_deinit(void);
int tx_hal_init(void);

int tx_pipeline_create(short width, short height, char framerate);
void tx_pipeline_destroy(void);

void *tx_video_thread(void);

void tx_system_deinit(void);
int tx_system_init(void);