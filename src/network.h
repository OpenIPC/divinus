#pragma once

#include <ifaddrs.h>
#include <netdb.h>
#include <net/if.h>
#include <netinet/in.h>
#include <pthread.h>
#include <stdio.h>
#include <sys/utsname.h>

#ifdef __UCLIBC__
extern int asprintf(char **restrict strp, const char *restrict fmt, ...);
#endif

#include "app_config.h"
#include "hal/support.h"
#include "lib/tinysvcmdns.h"

int start_mdns();
void stop_mdns();

void *onvif_thread();

int start_onvif_server();
void stop_onvif_server();