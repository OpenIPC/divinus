#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <sys/syscall.h>
#include <unistd.h>

void __assert(void) {}
void backtrace(void) {}
void backtrace_symbols(void) {}
void __ctype_b(void) {}
void _MI_PRINT_GetDebugLevel(void) {}
void __stdin(void) {}
void _stdlib_mb_cur_max(void) {}

float __expf_finite(float x) { return expf(x); }
int __fgetc_unlocked(FILE *stream) { return fgetc(stream); }
double __log_finite(double x) { return log(x); }

void *mmap(void *start, size_t len, int prot, int flags, int fd, uint32_t off) {
    return (void*)syscall(SYS_mmap2, start, len, prot, flags, fd, off >> 12);
}

void *mmap64(void *start, size_t len, int prot, int flags, int fd, off_t off) {
    return (void*)syscall(SYS_mmap2, start, len, prot, flags, fd, off >> 12);
}
