#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <sys/syscall.h>
#include <unistd.h>

#ifndef __UCLIBC__
void __stdin(void) {}

#ifndef __GLIBC__
#include <sys/stat.h>

struct _stat_ {
    char padding[16];
    int st_mode;
};

int sTaT(const char *path, struct _stat_ *buf)
{
    struct stat st;
    int ret = stat(path, &st);
    buf->st_mode = st.st_mode;
    return ret;
}
#endif
#endif

void __assert(void) {}
void akuio_clean_invalidate_dcache(void) {}
void backtrace(void) {}
void backtrace_symbols(void) {}
void __ctype_b(void) {}
void __ctype_b_loc(void) {}
void __ctype_tolower(void) {}
void _MI_PRINT_GetDebugLevel(void) {}
void __pthread_register_cancel(void) {}
void __pthread_unregister_cancel(void) {}
void _stdlib_mb_cur_max(void) {}

float __expf_finite(float x) { return expf(x); }
int __fgetc_unlocked(FILE *stream) { return fgetc(stream); }
double __log_finite(double x) { return log(x); }

#if !defined(__riscv) && !defined(__riscv__)
void *mmap(void *start, size_t len, int prot, int flags, int fd, uint32_t off) {
    return (void*)syscall(SYS_mmap2, start, len, prot, flags, fd, off >> 12);
}

void *mmap64(void *start, size_t len, int prot, int flags, int fd, off_t off) {
    return (void*)syscall(SYS_mmap2, start, len, prot, flags, fd, off >> 12);
}
#else
void *mmap64(void *start, size_t len, int prot, int flags, int fd, off_t off) {
    return (void*)syscall(SYS_mmap, start, len, prot, flags, fd, off);
}
#endif