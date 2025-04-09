#pragma once

#include <ifaddrs.h>
#include <net/if.h>
#include <stdio.h>
#include <sys/utsname.h>

#include "app_config.h"
#include "hal/support.h"
#include "lib/tinysvcmdns.h"
#include "onvif.h"

typedef struct {
    char intf[3][16];
    char ipaddr[3][INET_ADDRSTRLEN];
    char count;
    char host[65];
} NetInfo;

void init_network(void);
int start_network(void);
void stop_network(void);

int start_mdns(void);
void stop_mdns(void);