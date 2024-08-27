#include "types.h"
#if defined(__arm__)
#include "plus/gm_hal.h"
#include "hisi/v1_hal.h"
#include "hisi/v2_hal.h"
#include "hisi/v3_hal.h"
#include "hisi/v4_hal.h"
#include "star/i6_hal.h"
#include "star/i6c_hal.h"
#include "star/i6f_hal.h"
#elif defined(__mips__)
#include "inge/t31_hal.h"
#elif defined(__riscv) || defined(__riscv__)
#include "plus/cvi_hal.h"
#endif

#include <errno.h>
#include <fcntl.h>
#include <linux/version.h>
#include <stdbool.h>
#include <stdio.h>
#include <sys/mman.h>
#include <unistd.h>

// Newer versions of musl have UAPI headers 
// that redefine struct sysinfo
#if defined(__GLIBC__) || defined(__UCLIBC__) \
    || LINUX_VERSION_CODE < KERNEL_VERSION(3, 4, 0)
#include <sys/sysinfo.h>
#else
#include <linux/sysinfo.h>
#endif

#ifdef __UCLIBC__
extern int asprintf(char **restrict strp, const char *restrict fmt, ...);
#endif

extern int sysinfo (struct sysinfo *__info);

void *mmap64(void *start, size_t len, int prot, int flags, int fd, off_t off);

extern void *aud_thread;
extern void *isp_thread;
extern void *vid_thread;

extern char chnCount;
extern hal_chnstate *chnState;

extern char chip[16];
extern char family[32];
extern hal_platform plat;
extern int series;

#if defined(__arm__)
extern hal_chnstate gm_state[GM_VENC_CHN_NUM];
extern hal_chnstate v1_state[V1_VENC_CHN_NUM];
extern hal_chnstate v2_state[V2_VENC_CHN_NUM];
extern hal_chnstate v3_state[V3_VENC_CHN_NUM];
extern hal_chnstate v4_state[V4_VENC_CHN_NUM];
extern hal_chnstate i6_state[I6_VENC_CHN_NUM];
extern hal_chnstate i6c_state[I6C_VENC_CHN_NUM];
extern hal_chnstate i6f_state[I6F_VENC_CHN_NUM];
#elif defined(__mips__)
extern hal_chnstate t31_state[T31_VENC_CHN_NUM];
#endif

bool hal_registry(unsigned int addr, unsigned int *data, hal_register_op op);
void hal_identify(void);