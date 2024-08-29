#include "app_config.h"
#include "hal/macros.h"
#include "http_post.h"
#include "media.h"
#include "network.h"
#include "night.h"
#include "rtsp/rtsp_server.h"
#include "server.h"
#include "watchdog.h"

#include <getopt.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

rtsp_handle rtspHandle;
char graceful = 0, keepRunning = 1;

void handle_error(int signo) {
    write(STDERR_FILENO, "Error occurred! Quitting...\n", 28);
    keepRunning = 0;
    exit(EXIT_FAILURE);
}

void handle_exit(int signo) {
    write(STDERR_FILENO, "Graceful shutdown...\n", 21);
    keepRunning = 0;
    graceful = 1;
}

int main(int argc, char *argv[]) {
    {
        char signal_error[] = {SIGABRT, SIGBUS, SIGFPE, SIGILL, SIGSEGV};
        char signal_exit[] = {SIGINT, SIGQUIT, SIGTERM};
        char signal_null[] = {EPIPE, SIGPIPE};

        for (char *s = signal_error; s < (&signal_error)[1]; s++)
            signal(*s, handle_error);
        for (char *s = signal_exit; s < (&signal_exit)[1]; s++)
            signal(*s, handle_exit);
        for (char *s = signal_null; s < (&signal_null)[1]; s++)
            signal(*s, NULL);
    }

    hal_identify();

    if (!*family)
        HAL_ERROR("hal", "Unsupported chip family! Quitting...\n");

    fprintf(stderr, "\033[7m Divinus for %s \033[0m\n", family);
    fprintf(stderr, "Chip ID: %s\n", chip);

    if (parse_app_config() != CONFIG_OK)
        HAL_ERROR("hal", "Can't load app config 'divinus.yaml'\n");

    if (app_config.watchdog)
        watchdog_start(app_config.watchdog);

    if (app_config.mdns_enable)
        start_mdns();

    start_server();

    if (app_config.rtsp_enable) {
        rtspHandle = rtsp_create(RTSP_MAXIMUM_CONNECTIONS, 2);
        HAL_INFO("rtsp", "Started listening for clients...\n");
    }

    if (start_sdk())
        HAL_ERROR("hal", "Failed to start SDK!\n");

    if (app_config.night_mode_enable)
        start_monitor_light_sensor();

    if (app_config.http_post_enable)
        start_http_post_send();

    if (app_config.osd_enable)
        start_region_handler();

    while (keepRunning) {
        watchdog_reset();
        sleep(1);
    }

    if (app_config.rtsp_enable) {
        rtsp_finish(rtspHandle);
        HAL_INFO("rtsp", "Server has closed!\n");
    }

    if (app_config.osd_enable)
        stop_region_handler();

    if (app_config.night_mode_enable)
        stop_monitor_light_sensor();

    stop_sdk();

    stop_server();

    if (app_config.mdns_enable)
        stop_mdns();

    if (app_config.watchdog)
        watchdog_stop();

    if (!graceful)
        restore_app_config();

    fprintf(stderr, "Main thread is shutting down...\n");
    return EXIT_SUCCESS;
}