#include <getopt.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "config/app_config.h"
#include "http_post.h"
#include "night.h"
#include "video.h"
#include "server.h"

#include "rtsp/ringfifo.h"
#include "rtsp/rtputils.h"
#include "rtsp/rtspservice.h"

hal_platform plat;

int main(int argc, char *argv[]) {
    hal_identfy();
    if (plat == HAL_PLATFORM_UNK) {
        fprintf(stderr, "Unsupported chip family! Quiting...\n");
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
        rtsp_init();
        signal(SIGINT, rtsp_interrupt);

        fprintf(stderr, "RTSP server started, listening for clients...\n");
        
        mainFd = tcp_listen(SERVER_RTSP_PORT_DEFAULT);
        if (rtsp_init_schedule() == ERR_FATAL) {
            fprintf(stderr,
                "Can't start scheduler %s, "
                "%i \nServer is aborting.\n");
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
    if (app_config.rtsp_enable) {
        struct timespec ts = {2, 0};
        while (keepRunning) {
            nanosleep(&ts, NULL);
            rtsp_eventloop(mainFd);
        }
        ring_free();
        printf("RTSP server closed!\n");
    } else 
        while (keepRunning) sleep(1);

    stop_sdk();
    stop_server();

    printf("Main thread is shutting down...\n");
    return EXIT_SUCCESS;
}
