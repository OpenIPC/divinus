#include "jpeg.h"

#include <fcntl.h>
#include <pthread.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include <sys/select.h>

#include "error.h"
#include "night.h"
#include "video.h"

#define tag "[jpeg] "

int jpeg_index;
bool jpeg_module_init = false;

pthread_mutex_t jpeg_mutex;

int jpeg_init() {
    int ret;

    pthread_mutex_lock(&jpeg_mutex);

    jpeg_index = take_next_free_channel(false);

    if (ret = create_vpss_chn(jpeg_index, app_config.jpeg_width, app_config.jpeg_height, 1, 1)) {
        printf(
            tag "Creating channel %d failed with %#x!\n%s\n", 
            jpeg_index, ret, errstr(ret));
        pthread_mutex_unlock(&jpeg_mutex);
        return EXIT_FAILURE;
    }

    {
        hal_vidconfig config;
        config.width = app_config.jpeg_width;
        config.height = app_config.jpeg_height;
        config.codec = HAL_VIDCODEC_JPG;
        config.mode = HAL_VIDMODE_QP;
        config.minQual = app_config.jpeg_qfactor;

        switch (plat) {
            case HAL_PLATFORM_I6: ret = i6_video_create(jpeg_index, &config); break;
            case HAL_PLATFORM_I6C: ret = i6c_video_create(jpeg_index, &config); break;
            case HAL_PLATFORM_I6F: ret = i6f_video_create(jpeg_index, &config); break;
            case HAL_PLATFORM_V3: ret = v3_video_create(jpeg_index, &config); break;
            default: 
                pthread_mutex_unlock(&jpeg_mutex);
                return EXIT_FAILURE;      
        }

        if (ret) {
            printf(
                tag "Creating encoder %d failed with %#x!\n%s\n", 
                jpeg_index, ret, errstr(ret));
            pthread_mutex_unlock(&jpeg_mutex);
            return EXIT_FAILURE;
        }
    }

    jpeg_module_init = true;
    pthread_mutex_unlock(&jpeg_mutex);
    printf(tag "Module initialization completed!\n");

    return EXIT_SUCCESS;
}

void jpeg_deinit() {
    pthread_mutex_lock(&jpeg_mutex);
    disable_venc_chn(jpeg_index, 1);
    jpeg_module_init = false;
    pthread_mutex_unlock(&jpeg_mutex);
}

int jpeg_get(short width, short height, char quality, char grayscale, 
    hal_jpegdata *jpeg) {
    pthread_mutex_lock(&jpeg_mutex);
    if (!jpeg_module_init) {
        pthread_mutex_unlock(&jpeg_mutex);
        printf(tag "Module is not enabled!\n");
        return EXIT_FAILURE;
    }
    int ret;

    switch (plat) {
        case HAL_PLATFORM_I6: ret = i6_video_snapshot_grab(jpeg_index, width, height, 
            quality, grayscale, jpeg); break;
        case HAL_PLATFORM_I6C: ret = i6c_video_snapshot_grab(jpeg_index, width, height, 
            quality, grayscale, jpeg); break;
        case HAL_PLATFORM_I6F: ret = i6f_video_snapshot_grab(jpeg_index, width, height, 
            quality, grayscale, jpeg); break;
    }
    if (ret) {
        if (jpeg->data)
            free(jpeg->data);
        jpeg->data = NULL;
        return EXIT_FAILURE;
    }

    pthread_mutex_unlock(&jpeg_mutex);
    return ret;
}