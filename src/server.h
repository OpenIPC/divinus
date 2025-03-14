#pragma once

#include <arpa/inet.h>
#include <errno.h>
#include <netinet/in.h>
#include <pthread.h>
#include <regex.h>
#include <signal.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <time.h>
#include <unistd.h>

#include "app_config.h"
#include "hal/types.h"
#include "lib/spng.h"
#include "jpeg.h"
#include "media.h"
#include "mp4/mp4.h"
#include "mp4/nal.h"
#include "night.h"
#include "region.h"
#include "watchdog.h"

#define IMPORT_BIN(sect, file, sym) asm (\
    ".section " #sect "\n"                  /* Change section */\
    ".global " #sym "\n"                    /* Export the object address */\
    ".balign 4\n"                           /* Word alignment */\
    #sym ":\n"                              /* Define the object label */\
    ".incbin \"" file "\"\n"                /* Import the file */\
    ".global " #sym "_size\n"               /* Export the object size */\
    ".balign 8\n"                           /* Word alignment */\
    #sym "_size:\n"                         /* Define the object size label */\
    ".long " #sym "_size - " #sym "\n"      /* Define the object size */\
    ".section \".text\"\n")                 /* Restore section */

#define IMPORT_STR(sect, file, sym) asm (\
    ".section " #sect "\n"                  /* Change section */\
    ".balign 4\n"                           /* Word alignment */\
    ".global " #sym "\n"                    /* Export the object address */\
    #sym ":\n"                              /* Define the object label */\
    ".incbin \"" file "\"\n"                /* Import the file */\
    ".byte 0\n"                             /* Null-terminate the string */\
    ".balign 4\n"                           /* Word alignment */\
    ".section \".text\"\n")                 /* Restore section */

extern char graceful, keepRunning;

int start_server();
int stop_server();

void send_jpeg_to_client(char index, char *buf, ssize_t size);
void send_mjpeg_to_client(char index, char *buf, ssize_t size);
void send_h26x_to_client(char index, hal_vidstream *stream);
void send_mp3_to_client(char *buf, ssize_t size);
void send_mp4_to_client(char index, hal_vidstream *stream, char isH265);
void send_pcm_to_client(hal_audframe *frame);