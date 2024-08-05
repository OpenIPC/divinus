#pragma once

#include "hal/support.h"
#include "lib/tinysvcmdns.h"

#include <ifaddrs.h>
#include <net/if.h>
#include <stdio.h>
#include <sys/utsname.h>

int start_mdns();
void stop_mdns();