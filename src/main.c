#include "common.h"

#include <getopt.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "http_post.h"
#include "night.h"
#include "rtsp/rtsp_server.h"
#include "server.h"
#include "video.h"
#include "watchdog.h"

rtsp_handle rtspHandle;

int main(int argc, char *argv[]) {
    hal_identify();
    switch (plat) {
#if defined(__arm__)
        case HAL_PLATFORM_GM:
            fprintf(stderr, "Divinus for grainmedia\n"); break;
        case HAL_PLATFORM_I6:
            fprintf(stderr, "Divinus for infinity6(b0/e)\n"); break;
        case HAL_PLATFORM_I6C:
            fprintf(stderr, "Divinus for infinity6c\n"); break;
        case HAL_PLATFORM_I6F:
            fprintf(stderr, "Divinus for infinity6f\n"); break;
        case HAL_PLATFORM_V3:
            fprintf(stderr, "Divinus for hisi-gen3\n"); break;
        case HAL_PLATFORM_V4:
            fprintf(stderr, "Divinus for hisi-gen4\n"); break;
#elif defined(__mips__)
        case HAL_PLATFORM_T31:
            fprintf(stderr, "Divinus for ingenic t31\n"); break;
#endif
        default:
            fprintf(stderr, "Unsupported chip family! Quitting...\n");
            return EXIT_FAILURE;
    }
    fprintf(stderr, "Chip series: %s\n", series);

    if (parse_app_config() != CONFIG_OK) {
        fprintf(stderr, "Can't load app config 'divinus.yaml'\n");
        return EXIT_FAILURE;
    }

    start_server();

    if (app_config.rtsp_enable) {
        rtspHandle = rtsp_create(RTSP_MAXIMUM_CONNECTIONS, 2);
        fprintf(stderr, "RTSP server started, listening for clients...\n");
    }

    if (start_sdk())
        return EXIT_FAILURE;

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
        printf("RTSP server has closed!\n");
    }

    if (app_config.osd_enable)
        stop_region_handler();

    if (app_config.night_mode_enable)
        stop_monitor_light_sensor();

    stop_sdk();

    stop_server();

    printf("Main thread is shutting down...\n");
    return EXIT_SUCCESS;
}
