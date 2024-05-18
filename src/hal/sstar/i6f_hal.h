#include "i6f_common.h"
#include "i6f_isp.h"
#include "i6f_rgn.h"
#include "i6f_scl.h"
#include "i6f_snr.h"
#include "i6f_sys.h"
#include "i6f_venc.h"
#include "i6f_vif.h"

extern char keepRunning;

extern int (*i6f_venc_cb)(char, hal_vidstream*);

void i6f_hal_deinit(void);
int i6f_hal_init(void);

int i6f_channel_bind(char index, char framerate, char jpeg);
int i6f_channel_create(char index, short width, short height, char mirror, char flip, char jpeg);
void i6f_channel_disable(char index);
int i6f_channel_enabled(char index);
int i6f_channel_grayscale(char enable);
int i6f_channel_in_mainloop(char index);
int i6f_channel_next(char mainLoop);
int i6f_channel_unbind(char index, char jpeg);

int i6f_config_load(char *path);

int i6f_encoder_create(char index, hal_vidconfig *config);
int i6f_encoder_destroy(char index, char jpeg);
int i6f_encoder_destroy_all(void);
int i6f_encoder_snapshot_grab(char index, short width, short height,
    char quality, char grayscale, hal_vidstream *stream);
void *i6f_encoder_thread(void);

int i6f_pipeline_create(char sensor, short width, short height, char framerate);
void i6f_pipeline_destroy(void);

void i6f_system_deinit(void);
int i6f_system_init(void);