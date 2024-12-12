#include "tools.h"
#include "types.h"

#if defined(__arm__)
#include "plus/ak_hal.h"
#include "plus/gm_hal.h"
#include "hisi/v1_hal.h"
#include "hisi/v2_hal.h"
#include "hisi/v3_hal.h"
#include "hisi/v4_hal.h"
#include "star/i3_hal.h"
#include "star/i6_hal.h"
#include "star/i6c_hal.h"
#include "star/m6_hal.h"
#elif defined(__mips__)
#include "inge/t31_hal.h"
#elif defined(__riscv) || defined(__riscv__)
#include "plus/cvi_hal.h"
#endif

#include <linux/version.h>
#include <stdbool.h>
#include <stdio.h>

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

extern void *aud_thread;
extern void *isp_thread;
extern void *vid_thread;

extern char chnCount;
extern hal_chnstate *chnState;

extern char chip[16];
extern char family[32];
extern hal_platform plat;
extern char sensor[16];
extern int series;

void hal_identify(void);