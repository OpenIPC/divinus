#pragma once

#include "common.h"
#include "hal/tools.h"
#include "jpeg.h"
#include "mp4/mp4.h"
#include "mp4/nal.h"

#include <arpa/inet.h>
#include <netdb.h>
#include <netinet/in.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <time.h>

#include <errno.h>
#include <pthread.h>
#include <regex.h>
#include <unistd.h>

extern char keepRunning;

void start_http_post_send();
