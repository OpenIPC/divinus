#include "jpeg.h"

int jpeg_index;
bool jpeg_module_init = false;

pthread_mutex_t jpeg_mutex;

int jpeg_init() {  
    int ret;

    pthread_mutex_lock(&jpeg_mutex);

    switch (plat) {
#if defined(__arm__)
        case HAL_PLATFORM_GM: goto active;
#elif defined(__mips__)
        case HAL_PLATFORM_T31:
            if (app_config.mjpeg_enable) goto active;
            break;
#endif
    }

    jpeg_index = take_next_free_channel(false);

    if (ret = create_channel(jpeg_index, app_config.jpeg_width, app_config.jpeg_height, 1, 1)) {
        pthread_mutex_unlock(&jpeg_mutex);
        HAL_ERROR("jpeg", "Creating channel %d failed with %#x!\n%s\n", 
            jpeg_index, ret, errstr(ret));
    }

    {
        hal_vidconfig config;
        config.width = app_config.jpeg_width;
        config.height = app_config.jpeg_height;
        config.codec = HAL_VIDCODEC_JPG;
        config.mode = HAL_VIDMODE_QP;
        config.minQual = config.maxQual = app_config.jpeg_qfactor;

        switch (plat) {
#if defined(__arm__)
            case HAL_PLATFORM_I6:  ret = i6_video_create(jpeg_index, &config); break;
            case HAL_PLATFORM_I6C: ret = i6c_video_create(jpeg_index, &config); break;
            case HAL_PLATFORM_I6F: ret = i6f_video_create(jpeg_index, &config); break;
            case HAL_PLATFORM_V1:  ret = v1_video_create(jpeg_index, &config); break;
            case HAL_PLATFORM_V2:  ret = v2_video_create(jpeg_index, &config); break;
            case HAL_PLATFORM_V3:  ret = v3_video_create(jpeg_index, &config); break;
            case HAL_PLATFORM_V4:  ret = v4_video_create(jpeg_index, &config); break;
#elif defined(__mips__)
            case HAL_PLATFORM_T31: ret = t31_video_create(jpeg_index, &config); break;
#endif
            default: 
                pthread_mutex_unlock(&jpeg_mutex);
                return EXIT_FAILURE;      
        }

        if (ret) {
            pthread_mutex_unlock(&jpeg_mutex);
            HAL_ERROR("jpeg", "Creating encoder %d failed with %#x!\n%s\n", 
                jpeg_index, ret, errstr(ret));
        }
    }

active:
    jpeg_module_init = true;
    pthread_mutex_unlock(&jpeg_mutex);
    HAL_INFO("jpeg", "Module enabled!\n");

    return EXIT_SUCCESS;
}

void jpeg_deinit() {
    pthread_mutex_lock(&jpeg_mutex);

    switch (plat) {
#if defined(__arm__)
        case HAL_PLATFORM_GM:  goto active;
        case HAL_PLATFORM_I6:  i6_video_destroy(jpeg_index); break;
        case HAL_PLATFORM_I6C: i6c_video_destroy(jpeg_index); break;
        case HAL_PLATFORM_I6F: i6f_video_destroy(jpeg_index); break;
        case HAL_PLATFORM_V1:  v1_video_destroy(jpeg_index); break;
        case HAL_PLATFORM_V2:  v2_video_destroy(jpeg_index); break;
        case HAL_PLATFORM_V3:  v3_video_destroy(jpeg_index); break;
        case HAL_PLATFORM_V4:  v4_video_destroy(jpeg_index); break;
#elif defined(__mips__)
        case HAL_PLATFORM_T31:
            if (app_config.mjpeg_enable) goto active;
            t31_video_destroy(jpeg_index);
            break;
#endif
        default: 
            pthread_mutex_unlock(&jpeg_mutex);
            return;    
    }
    disable_video(jpeg_index, 1);

active:
    jpeg_module_init = false;
    pthread_mutex_unlock(&jpeg_mutex);
    HAL_INFO("jpeg", "Module disabled!\n");
}

int jpeg_get(short width, short height, char quality, char grayscale, 
    hal_jpegdata *jpeg) {
    pthread_mutex_lock(&jpeg_mutex);
    if (!jpeg_module_init) {
        pthread_mutex_unlock(&jpeg_mutex);
        HAL_ERROR("jpeg", "Module is not enabled!\n");
    }
    int ret;

    switch (plat) {
#if defined(__arm__)
        case HAL_PLATFORM_GM:  ret = gm_video_snapshot_grab(width, height, quality, jpeg); break;
        case HAL_PLATFORM_I6:  ret = i6_video_snapshot_grab(jpeg_index, quality, jpeg); break;
        case HAL_PLATFORM_I6C: ret = i6c_video_snapshot_grab(jpeg_index, quality, jpeg); break;
        case HAL_PLATFORM_I6F: ret = i6f_video_snapshot_grab(jpeg_index, quality, jpeg); break;
        case HAL_PLATFORM_V1:  ret = v1_video_snapshot_grab(jpeg_index, jpeg); break;
        case HAL_PLATFORM_V2:  ret = v2_video_snapshot_grab(jpeg_index, jpeg); break;
        case HAL_PLATFORM_V3:  ret = v3_video_snapshot_grab(jpeg_index, jpeg); break;
        case HAL_PLATFORM_V4:  ret = v4_video_snapshot_grab(jpeg_index, jpeg); break;
#elif defined(__mips__)
        case HAL_PLATFORM_T31: ret = t31_video_snapshot_grab(app_config.mjpeg_enable ? 
            -1 : jpeg_index, jpeg); break;
#endif
    }
    if (ret) {
        if (jpeg->data)
            free(jpeg->data);
        jpeg->data = NULL;
    }

    pthread_mutex_unlock(&jpeg_mutex);
    return ret;
}