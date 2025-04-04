#include "m6_common.h"
#include "m6_aud.h"
#include "m6_ipu.h"
#include "m6_isp.h"
#include "m6_rgn.h"
#include "m6_scl.h"
#include "m6_snr.h"
#include "m6_sys.h"
#include "m6_venc.h"
#include "m6_vif.h"

#include <sys/select.h>
#include <unistd.h>

extern char audioOn, keepRunning;

extern hal_chnstate m6_state[M6_VENC_CHN_NUM];
extern int (*m6_aud_cb)(hal_audframe*);
extern int (*m6_vid_cb)(char, hal_vidstream*);

void m6_hal_deinit(void);
int m6_hal_init(void);

void m6_audio_deinit(void);
int m6_audio_init(int samplerate, int gain);
void *m6_audio_thread(void);

int m6_channel_bind(char index, char framerate);
int m6_channel_create(char index, short width, short height, char mirror, char flip, char jpeg);
int m6_channel_grayscale(char enable);
int m6_channel_unbind(char index);

int m6_config_load(char *path);

int m6_pipeline_create(char sensor, short width, short height, char framerate);
void m6_pipeline_destroy(void);

int m6_region_create(char handle, hal_rect rect, short opacity);
void m6_region_deinit(void);
void m6_region_destroy(char handle);
void m6_region_init(void);
int m6_region_setbitmap(int handle, hal_bitmap *bitmap);

int m6_video_create(char index, hal_vidconfig *config);
int m6_video_destroy(char index);
int m6_video_destroy_all(void);
void m6_video_request_idr(char index);
int m6_video_snapshot_grab(char index, char quality, hal_jpegdata *jpeg);
void *m6_video_thread(void);

void m6_system_deinit(void);
int m6_system_init(void);