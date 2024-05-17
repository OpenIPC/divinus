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
            case HAL_PLATFORM_I6: ret = i6_encoder_create(jpeg_index, &config);
            case HAL_PLATFORM_I6C: ret = i6c_encoder_create(jpeg_index, &config);
            case HAL_PLATFORM_I6F: ret = i6f_encoder_create(jpeg_index, &config);
            case HAL_PLATFORM_V3: ret = v3_encoder_create(jpeg_index, &config);
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

int jpeg_deinit() {
    pthread_mutex_lock(&jpeg_mutex);
    disable_venc_chn(jpeg_index, 1);
    jpeg_module_init = false;
    pthread_mutex_unlock(&jpeg_mutex);
}

int jpeg_get(short width, short height, char quality, char grayscale, 
    struct jpegdata *jpeg) {
    pthread_mutex_lock(&jpeg_mutex);
    if (!jpeg_module_init) {
        pthread_mutex_unlock(&jpeg_mutex);
        printf(tag "Module is not enabled!\n");
        return EXIT_FAILURE;
    }
    int ret;

    hal_vidstream stream;
    switch (plat) {
        case HAL_PLATFORM_I6: i6_encoder_snapshot_grab(jpeg_index, width, height, 
            quality, grayscale, &stream); break;
        case HAL_PLATFORM_I6C: i6c_encoder_snapshot_grab(jpeg_index, width, height, 
            quality, grayscale, &stream); break;
        case HAL_PLATFORM_I6F: i6f_encoder_snapshot_grab(jpeg_index, width, height, 
            quality, grayscale, &stream); break;
    }
    if (ret) {
        printf(tag "Requesting a picture failed!\n");
        if (!stream.pack) return EXIT_FAILURE;

        free(stream.pack);
        stream.pack = NULL;
        return EXIT_FAILURE;
    }

    {
        jpeg->jpeg_size = 0;
        for (unsigned int i = 0; i < stream.pack; i++) {
            hal_vidpack *pack = &stream.pack[i];
            unsigned int pack_len = pack->length - pack->offset;
            unsigned char *pack_data = pack->addr + pack->offset;

            ssize_t need_size = jpeg->jpeg_size + pack_len;
            if (need_size > jpeg->buf_size) {
                jpeg->buf = realloc(jpeg->buf, need_size);
                jpeg->buf_size = need_size;
            }
            memcpy(
                jpeg->buf + jpeg->jpeg_size, pack_data, pack_len);
            jpeg->jpeg_size += pack_len;
        }
    }

    pthread_mutex_unlock(&jpeg_mutex);
    return ret;
}