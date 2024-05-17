#include <stdio.h>

void __assert(void) {}
void backtrace(void) {}
void backtrace_symbols(void) {}
void _MI_PRINT_GetDebugLevel(void) {}
void __stdin(void) {}

int __fgetc_unlocked(FILE *stream) {
  return fgetc(stream);
}