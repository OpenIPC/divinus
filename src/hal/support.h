#include "types.h"
#if defined(__arm__)
#include "plus/gm_hal.h"
#include "hisi/v3_hal.h"
#include "hisi/v4_hal.h"
#include "star/i6_hal.h"
#include "star/i6c_hal.h"
#include "star/i6f_hal.h"
#elif defined(__mips__)
#include "inge/t31_hal.h"
#endif

#include <errno.h>
#include <fcntl.h>
#include <stdbool.h>
#include <stdio.h>
#include <sys/mman.h>
#include <unistd.h>

#ifndef CEILING
#define CEILING_POS(X) ((X-(int)(X)) > 0 ? (int)(X+1) : (int)(X))
#define CEILING_NEG(X) (int)(X)
#define CEILING(X) ( ((X) > 0) ? CEILING_POS(X) : CEILING_NEG(X) )
#endif

#define starts_with(a, b) !strncmp(a, b, strlen(b))
#define equals(a, b) !strcmp(a, b)
#define equals_case(a, b) !strcasecmp(a, b)
#define ends_with(a, b)      \
    size_t alen = strlen(a); \
    size_t blen = strlen(b); \
    return (alen > blen) && strcmp(a + alen - blen, b);
#define empty(x) (x[0] == '\0')

extern void *aud_thread;
extern void *isp_thread;
extern void *vid_thread;

extern char chnCount;
extern hal_chnstate *chnState;

extern char chipId[16];
extern hal_platform plat;
extern int series;

#if defined(__arm__)
extern void *gm_video_thread(void);
extern hal_chnstate gm_state[GM_VENC_CHN_NUM];

extern void *v3_audio_thread(void);
extern void *v3_video_thread(void);
extern hal_chnstate v3_state[V4_VENC_CHN_NUM];

extern void *v4_audio_thread(void);
extern void *v4_video_thread(void);
extern hal_chnstate v4_state[V4_VENC_CHN_NUM];

extern void *i6_audio_thread(void);
extern void *i6_video_thread(void);
extern hal_chnstate i6_state[I6_VENC_CHN_NUM];

extern void *i6c_audio_thread(void);
extern void *i6c_video_thread(void);
extern hal_chnstate i6c_state[I6C_VENC_CHN_NUM];

extern void *i6f_audio_thread(void);
extern void *i6f_video_thread(void);
extern hal_chnstate i6f_state[I6F_VENC_CHN_NUM];
#elif defined(__mips__)
extern void *t31_video_thread(void);
extern hal_chnstate t31_state[T31_VENC_CHN_NUM];
#endif

bool hal_registry(unsigned int addr, unsigned int *data, hal_register_op op);
void hal_identify(void);

void *mmap64(void *start, size_t len, int prot, int flags, int fd, off_t off);