#include "types.h"
#include "globals.h"
#include "tools.h"

#if defined(__ARM_PCS_VFP)
#include "star/i3_hal.h"
#include "star/i6_hal.h"
#include "star/i6c_hal.h"
#include "star/m6_hal.h"
#include "plus/rk_hal.h"
#elif defined(__arm__) && !defined(__ARM_PCS_VFP)
#include "plus/ak_hal.h"
#include "plus/gm_hal.h"
#include "hisi/v1_hal.h"
#include "hisi/v2_hal.h"
#include "hisi/v3_hal.h"
#include "hisi/v4_hal.h"
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

void hal_identify(void);
float hal_temperature_read(void);
