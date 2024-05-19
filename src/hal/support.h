#include "types.h"
#include "hisi/v3_hal.h"
#include "sstar/i6_hal.h"
#include "sstar/i6c_hal.h"
#include "sstar/i6f_hal.h"

#include <errno.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <unistd.h>

extern void *isp_thread;
extern void *venc_thread;

extern char chnCount;
extern hal_chnstate *chnState;
extern hal_platform plat;

extern void *v3_encoder_thread(void);
extern hal_chnstate v3_state[V3_VENC_CHN_NUM];

extern void *i6_encoder_thread(void);
extern hal_chnstate i6_state[I6_VENC_CHN_NUM];

extern void *i6c_encoder_thread(void);
extern hal_chnstate i6c_state[I6C_VENC_CHN_NUM];

extern void *i6f_encoder_thread(void);
extern hal_chnstate i6f_state[I6F_VENC_CHN_NUM];

bool hal_registry(unsigned int addr, unsigned int *data, hal_register_op op);
void hal_identify(void);