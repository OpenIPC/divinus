#pragma once

#include <arpa/inet.h>
#include <netdb.h>
#include <netinet/in.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <time.h>
#include <unistd.h>

#include "hal/macros.h"
#include "jpeg.h"

extern char keepRunning;

void start_http_post_send();
void stop_http_post_send();