#include "i6f_common.h"
#include "i6f_aud.h"
#include "i6f_isp.h"
#include "i6f_rgn.h"
#include "i6f_scl.h"
#include "i6f_snr.h"
#include "i6f_sys.h"
#include "i6f_venc.h"
#include "i6f_vif.h"

extern char keepRunning;

extern int (*i6f_aud_cb)(hal_audframe*);
extern int (*i6f_venc_cb)(char, hal_vidstream*);

void i6f_hal_deinit(void);
int i6f_hal_init(void);

void i6f_audio_deinit(void);
int i6f_audio_init(void);

int i6f_channel_bind(char index, char framerate, char jpeg);
int i6f_channel_create(char index, short width, short height, char mirror, char flip, char jpeg);
int i6f_channel_grayscale(char enable);
int i6f_channel_unbind(char index, char jpeg);

int i6f_config_load(char *path);

int i6f_pipeline_create(char sensor, short width, short height, char framerate);
void i6f_pipeline_destroy(void);

int i6f_region_create(char handle, hal_rect rect, short opacity);
void i6f_region_deinit(void);
void i6f_region_destroy(char handle);
void i6f_region_init(void);
int i6f_region_setbitmap(int handle, hal_bitmap *bitmap);

int i6f_video_create(char index, hal_vidconfig *config);
int i6f_video_destroy(char index, char jpeg);
int i6f_video_destroy_all(void);
int i6f_video_snapshot_grab(char index, char quality, hal_jpegdata *jpeg);
void *i6f_video_thread(void);

void i6f_system_deinit(void);
int i6f_system_init(void);