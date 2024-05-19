#include <math.h>
#include <stdio.h>

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