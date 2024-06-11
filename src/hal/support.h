#include "types.h"
#if defined(__arm__)
#include "hisi/v3_hal.h"
#include "hisi/v4_hal.h"
#include "sstar/i6_hal.h"
#include "sstar/i6c_hal.h"
#include "sstar/i6f_hal.h"
#elif defined(__mips__)
#include "inge/t31_hal.h"
#endif

#include <errno.h>
#include <fcntl.h>
#include <stdbool.h>
#include <stdio.h>
#include <sys/mman.h>
#include <unistd.h>

extern void *isp_thread;
extern void *venc_thread;

extern char chnCount;
extern hal_chnstate *chnState;
extern hal_platform plat;
extern char series[16];

#if defined(__arm__)
extern void *v3_video_thread(void);
extern hal_chnstate v3_state[V4_VENC_CHN_NUM];

extern void *v4_video_thread(void);
extern hal_chnstate v4_state[V4_VENC_CHN_NUM];

extern void *i6_video_thread(void);
extern hal_chnstate i6_state[I6_VENC_CHN_NUM];

extern void *i6c_video_thread(void);
extern hal_chnstate i6c_state[I6C_VENC_CHN_NUM];

extern void *i6f_video_thread(void);
extern hal_chnstate i6f_state[I6F_VENC_CHN_NUM];
#elif defined(__mips__)
extern void *t31_video_thread(void);
extern hal_chnstate t31_state[T31_VENC_CHN_NUM];
#endif

bool hal_registry(unsigned int addr, unsigned int *data, hal_register_op op);
void hal_identify(void);

void *mmap64(void *start, size_t len, int prot, int flags, int fd, off_t off);