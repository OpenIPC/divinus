#include "common.h"

#include <getopt.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "http_post.h"
#include "night.h"
#include "server.h"
#include "video.h"

#include "rtsp/ringfifo.h"
#include "rtsp/rtputils.h"
#include "rtsp/rtspservice.h"

int main(int argc, char *argv[]) {
    hal_identify();
    switch (plat) {
        case HAL_PLATFORM_I6:
            fprintf(stderr, "Divinus for infinity6[b0/e]\n"); break;
        case HAL_PLATFORM_I6C:
            fprintf(stderr, "Divinus for infinity6c\n"); break;
        case HAL_PLATFORM_I6F:
            fprintf(stderr, "Divinus for infinity6f\n"); break;
        case HAL_PLATFORM_V3:
            fprintf(stderr, "Divinus for hisi-gen3\n"); break;
        default:
            fprintf(stderr, "Unsupported chip family! Quitting...\n");
            return EXIT_FAILURE;
    }

    if (parse_app_config("./divinus.ini") != CONFIG_OK) {
        fprintf(stderr, "Can't load app config './divinus.ini'\n");
        return EXIT_FAILURE;
    }

    start_server();

    int mainFd;
    if (app_config.rtsp_enable) {
        ring_malloc(app_config.mp4_width * app_config.mp4_height);
        signal(SIGINT, rtsp_interrupt);
        fprintf(stderr, "RTSP server started, listening for clients...\n");
        
        mainFd = tcp_listen(SERVER_RTSP_PORT_DEFAULT);
        if (rtsp_init_schedule() == RTSP_ERR_FATAL) {
            fprintf(stderr,
                "Can't start scheduler,\n"
                "Server is aborting.\n");
            return EXIT_SUCCESS;
        }
        rtsp_portpool_init(RTP_DEFAULT_PORT);
    }

    if (start_sdk())
        return EXIT_FAILURE;

    if (app_config.night_mode_enable)
        start_monitor_light_sensor();

    if (app_config.http_post_enable)
        start_http_post_send();

    if (app_config.osd_enable) {
        switch (plat) {
            case HAL_PLATFORM_I6: i6_region_init(); break;
            case HAL_PLATFORM_I6C: i6c_region_init(); break;
            case HAL_PLATFORM_I6F: i6f_region_init(); break;
        }
        start_region_handler();
    }
        
    if (app_config.rtsp_enable) {
        struct timespec ts = {2, 0};
        while (keepRunning) {
            nanosleep(&ts, NULL);
            rtsp_eventloop(mainFd);
        }
        ring_free();
        rtsp_deinit_schedule();
        printf("RTSP server has closed!\n");
    } else 
        while (keepRunning) sleep(1);

    if (app_config.osd_enable) {
        stop_region_handler();
        switch (plat) {
            case HAL_PLATFORM_I6: i6_region_deinit(); break;
            case HAL_PLATFORM_I6C: i6c_region_init(); break;
            case HAL_PLATFORM_I6F: i6f_region_init(); break;
        }
    }

    if (app_config.night_mode_enable)
        stop_monitor_light_sensor();

    stop_sdk();

    stop_server();

    printf("Main thread is shutting down...\n");
    return EXIT_SUCCESS;
}
