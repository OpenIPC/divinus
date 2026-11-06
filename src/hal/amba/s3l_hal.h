#pragma once

#include "s3l_iav.h"

#include "../globals.h"
#include "../tools.h"
#include "../../fmt/nal.h"

#include <errno.h>
#include <fcntl.h>
#include <pthread.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <unistd.h>

#define S3L_VENC_CHN_NUM 2

extern hal_chnstate s3l_state[S3L_VENC_CHN_NUM];
extern int (*s3l_aud_cb)(hal_audframe*);
extern int (*s3l_vid_cb)(char, hal_vidstream*);

void s3l_hal_deinit(void);
int s3l_hal_init(void);

void s3l_channel_grayscale(char enable);
int s3l_channel_bind(char index);
int s3l_channel_unbind(char index);

void s3l_sub_size(short width, short height);
int s3l_pipeline_create(short width, short height, char mirror, char flip, char framerate);
void s3l_pipeline_destroy(void);

int s3l_video_create(char index, hal_vidconfig *config);
int s3l_video_snapshot_grab(hal_jpegdata *jpeg);
int s3l_video_destroy(char index);
int s3l_video_destroy_all(void);
void s3l_video_request_idr(char index);
void *s3l_video_thread(void);

int s3l_aaa_config(int fd);
int s3l_ae_start(int fd, int fps);
int s3l_isp_set_wb(double r, double b);
int s3l_isp_get_wb(double *r, double *b);
int s3l_isp_dbp_mode(void);
int s3l_isp_dbp_base(void);
int s3l_isp_set_dbp(int thr);
int s3l_isp_flush(void);
void s3l_aaa_options(char ae, int target, char awb, int antiflicker);
void s3l_ae_stop(void);
void s3l_aaa_frame(const unsigned char *cfa);

void s3l_system_deinit(void);
int s3l_system_init(char *snrConfig);
int s3l_system_identify(void);
