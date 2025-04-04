#pragma once

#include <ifaddrs.h>
#include <net/if.h>
#include <stdio.h>
#include <sys/utsname.h>

#include "app_config.h"
#include "hal/support.h"
#include "lib/tinysvcmdns.h"

void init_network(void);
int start_network(void);
void stop_network(void);

int start_mdns(void);
void stop_mdns(void);

int start_onvif(void);
void stop_onvif(void);

void *onvif_thread();