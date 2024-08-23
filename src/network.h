#pragma once

#include <ifaddrs.h>
#include <net/if.h>
#include <stdio.h>
#include <sys/utsname.h>

#ifdef __UCLIBC__
extern int asprintf(char **restrict strp, const char *restrict fmt, ...);
#endif

#include "hal/support.h"
#include "lib/tinysvcmdns.h"

int start_mdns();
void stop_mdns();