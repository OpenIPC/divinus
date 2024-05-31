#include "video.h"

#include <pthread.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "error.h"
#include "http_post.h"
#include "jpeg.h"
#include "rtsp/ringfifo.h"
#include "rtsp/rtputils.h"
#include "rtsp/rtspservice.h"
#include "server.h"

pthread_mutex_t mutex;
pthread_t ispPid = 0;
pthread_t vencPid = 0;

int save_stream(char index, hal_vidstream *stream) {
    int ret;

    switch (chnState[index].payload) {
        case HAL_VIDCODEC_H264:
        case HAL_VIDCODEC_H265:
            if (app_config.mp4_enable) {
                send_mp4_to_client(index, stream, 
                    chnState[index].payload == HAL_VIDCODEC_H265 ? 1 : 0);
                send_h26x_to_client(index, stream);
            }
            if (app_config.rtsp_enable)
                put_h264_data_to_buffer(stream);
            break;
        case HAL_VIDCODEC_MJPG:
            if (app_config.mjpeg_enable) {
                static char *mjpeg_buf;
                static ssize_t mjpeg_buf_size = 0;
                ssize_t buf_size = 0;
                for (unsigned int i = 0; i < stream->count; i++) {
                    hal_vidpack *data = &stream->pack[i];
                    ssize_t need_size = buf_size + data->length - data->offset + 2;
                    if (need_size > mjpeg_buf_size) {
                        mjpeg_buf = realloc(mjpeg_buf, need_size);
                        mjpeg_buf_size = need_size;
                    }
                    memcpy(mjpeg_buf + buf_size, data->data + data->offset,
                        data->length - data->offset);
                    buf_size += data->length - data->offset;
                }
                send_mjpeg(index, mjpeg_buf, buf_size);
            }
            break;
        case HAL_VIDCODEC_JPG:
        {
            static char *jpeg_buf;
            static ssize_t jpeg_buf_size = 0;
            ssize_t buf_size = 0;
            for (unsigned int i = 0; i < stream->count; i++) {
                hal_vidpack *data = &stream->pack[i];
                ssize_t need_size = buf_size + data->length - data->offset + 2;
                if (need_size > jpeg_buf_size) {
                    jpeg_buf = realloc(jpeg_buf, need_size);
                    jpeg_buf_size = need_size;
                }
                memcpy(jpeg_buf + buf_size, data->data + data->offset,
                    data->length - data->offset);
                buf_size += data->length - data->offset;
            }
            if (app_config.jpeg_enable)
                send_jpeg(index, jpeg_buf, buf_size);
            break;
        }
        default:
            return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}

int take_next_free_channel(bool mainLoop) {
    pthread_mutex_lock(&mutex);
    for (int i = 0; i < chnCount; i++) {
        if (!chnState[i].enable) {
            chnState[i].enable = true;
            chnState[i].mainLoop = mainLoop;
            pthread_mutex_unlock(&mutex);
            return i;
        }
    }
    pthread_mutex_unlock(&mutex);
    return -1;
}

void set_grayscale(bool active) {
    pthread_mutex_lock(&mutex);
    switch (plat) {
        case HAL_PLATFORM_I6: 
        case HAL_PLATFORM_I6B0:
        case HAL_PLATFORM_I6E:  i6_channel_grayscale(active); break;
        case HAL_PLATFORM_I6C:  i6c_channel_grayscale(active); break;
        case HAL_PLATFORM_I6F:  i6f_channel_grayscale(active); break;
    }
    pthread_mutex_unlock(&mutex);
}

int create_vpss_chn(char index, short width, short height, char framerate, char jpeg) {
    switch (plat) {
        case HAL_PLATFORM_I6: 
        case HAL_PLATFORM_I6B0:
        case HAL_PLATFORM_I6E:  return i6_channel_create(index, width, height,
            app_config.mirror, app_config.flip, jpeg);
        case HAL_PLATFORM_I6C:  return i6c_channel_create(index, width, height,
            app_config.mirror, app_config.flip, jpeg);
        case HAL_PLATFORM_I6F:  return i6f_channel_create(index, width, height,
            app_config.mirror, app_config.flip, jpeg);
        case HAL_PLATFORM_V4:   return v4_channel_create(index, app_config.mirror,
            app_config.flip, framerate);
    }
}

int bind_vpss_venc(char index, char framerate, char jpeg) {
    switch (plat) {
        case HAL_PLATFORM_I6: 
        case HAL_PLATFORM_I6B0:
        case HAL_PLATFORM_I6E:  return i6_channel_bind(index, framerate, jpeg);
        case HAL_PLATFORM_I6C:  return i6c_channel_bind(index, framerate, jpeg);
        case HAL_PLATFORM_I6F:  return i6f_channel_bind(index, framerate, jpeg);
        case HAL_PLATFORM_V4:   return v4_channel_bind(index);
    }
}

int unbind_vpss_venc(char index, char jpeg) {
    switch (plat) {
        case HAL_PLATFORM_I6: 
        case HAL_PLATFORM_I6B0:
        case HAL_PLATFORM_I6E:  return i6_channel_unbind(index);
        case HAL_PLATFORM_I6C:  return i6c_channel_unbind(index, jpeg);
        case HAL_PLATFORM_I6F:  return i6f_channel_unbind(index, jpeg);
        case HAL_PLATFORM_V4:   return v4_channel_unbind(index);
    }
}

int disable_venc_chn(char index, char jpeg) {
    switch (plat) {
        case HAL_PLATFORM_I6: 
        case HAL_PLATFORM_I6B0:
        case HAL_PLATFORM_I6E:  return i6_video_destroy(index);
        case HAL_PLATFORM_I6C:  return i6c_video_destroy(index, jpeg);
        case HAL_PLATFORM_I6F:  return i6f_video_destroy(index, jpeg);
        case HAL_PLATFORM_V4:   return v4_video_destroy(index);
    }    
    return 0;
};

int start_sdk() {
    int ret;

    switch (plat) {
        case HAL_PLATFORM_I6: 
        case HAL_PLATFORM_I6B0:
        case HAL_PLATFORM_I6E:  ret = i6_hal_init(); break;
        case HAL_PLATFORM_I6C:  ret = i6c_hal_init(); break;
        case HAL_PLATFORM_I6F:  ret = i6f_hal_init(); break;
        case HAL_PLATFORM_T21:
        case HAL_PLATFORM_T31:  ret = tx_hal_init(); break;
        case HAL_PLATFORM_V4:   ret = v4_hal_init(); break;
    }
    if (ret) {
        fprintf(stderr, "HAL initialization failed with %#x!\n%s\n",
            ret, errstr(ret));
        return EXIT_FAILURE;
    }

    switch (plat) {
        case HAL_PLATFORM_I6: 
        case HAL_PLATFORM_I6B0:
        case HAL_PLATFORM_I6E:  i6_venc_cb = save_stream; break;
        case HAL_PLATFORM_I6C:  i6c_venc_cb = save_stream; break;
        case HAL_PLATFORM_I6F:  i6f_venc_cb = save_stream; break;
        case HAL_PLATFORM_V4:   v4_venc_cb = save_stream; break;
    }

    switch (plat) {
        case HAL_PLATFORM_I6: 
        case HAL_PLATFORM_I6B0:
        case HAL_PLATFORM_I6E:  ret = i6_system_init(); break;
        case HAL_PLATFORM_I6C:  ret = i6c_system_init(); break;
        case HAL_PLATFORM_I6F:  ret = i6f_system_init(); break;
        case HAL_PLATFORM_T21:
        case HAL_PLATFORM_T31:  ret = tx_system_init(); break;
        case HAL_PLATFORM_V4:   ret = v4_system_init(app_config.sensor_config); break;
    }
    if (ret) {
        fprintf(stderr, "System initialization failed with %#x!\n%s\n",
            ret, errstr(ret));
        return EXIT_FAILURE;
    }

    short width = MAX(app_config.mp4_width, app_config.mjpeg_width);
    short height = MAX(app_config.mp4_height, app_config.mjpeg_height);
    short framerate = MAX(app_config.mp4_fps, app_config.mjpeg_fps);

    switch (plat) {
        case HAL_PLATFORM_I6: 
        case HAL_PLATFORM_I6B0:
        case HAL_PLATFORM_I6E:  ret = i6_pipeline_create(0, width,
            height, framerate); break;
        case HAL_PLATFORM_I6C:  ret = i6c_pipeline_create(0, width,
            height, framerate); break;
        case HAL_PLATFORM_I6F:  ret = i6f_pipeline_create(0, width,
            height, framerate); break;
        case HAL_PLATFORM_V4:   ret = v4_pipeline_create(); break;
    }
    if (ret) {
        fprintf(stderr, "Pipeline creation failed with %#x!\n%s\n",
            ret, errstr(ret));
        return EXIT_FAILURE;
    }

    if (isp_thread) {
        pthread_attr_t thread_attr;
        pthread_attr_init(&thread_attr);
        size_t stacksize;
        pthread_attr_getstacksize(&thread_attr, &stacksize);
        size_t new_stacksize = app_config.isp_thread_stack_size;
        if (pthread_attr_setstacksize(&thread_attr, new_stacksize)) {
            fprintf(stderr, "Can't set stack size %zu!\n", new_stacksize);
        }
        if (pthread_create(
                     &ispPid, &thread_attr, (void *(*)(void *))isp_thread, NULL)) {
            fprintf(stderr, "Starting the imaging thread failed!\n");
            return EXIT_FAILURE;
        }
        if (pthread_attr_setstacksize(&thread_attr, stacksize)) {
            fprintf(stderr, "Can't set stack size %zu!\n", stacksize);
        }
        pthread_attr_destroy(&thread_attr);
    }

    if (app_config.mp4_enable) {
        int index = take_next_free_channel(true);

        if (ret = create_vpss_chn(index, app_config.mp4_width, 
            app_config.mp4_height, app_config.mp4_fps, 0)) {
            fprintf(stderr, 
                "Creating channel %d failed with %#x!\n%s\n", 
                index, ret, errstr(ret));
            return EXIT_FAILURE;
        }

        {
            hal_vidconfig config;
            config.width = app_config.mp4_width;
            config.height = app_config.mp4_height;
            config.codec = app_config.mp4_codecH265 ? 
                HAL_VIDCODEC_H265 : HAL_VIDCODEC_H264;
            config.mode = HAL_VIDMODE_CBR;
            config.profile = app_config.mp4_profile;
            config.gop = app_config.mp4_fps * 2;
            config.framerate = app_config.mp4_fps;
            config.bitrate = app_config.mp4_bitrate;

            switch (plat) {
                case HAL_PLATFORM_I6: 
                case HAL_PLATFORM_I6B0:
                case HAL_PLATFORM_I6E:  ret = i6_video_create(index, &config); break;
                case HAL_PLATFORM_I6C:  ret = i6c_video_create(index, &config); break;
                case HAL_PLATFORM_I6F:  ret = i6f_video_create(index, &config); break;
                case HAL_PLATFORM_V4:   ret = v4_video_create(index, &config); break;
            }

            if (ret) {
                fprintf(stderr, 
                    "Creating encoder %d failed with %#x!\n%s\n", 
                    index, ret, errstr(ret));
                return EXIT_FAILURE;
            }
        }

        if (ret = bind_vpss_venc(index, app_config.mp4_fps, 0)) {
            fprintf(stderr, 
                "Binding channel %d failed with %#x!\n%s\n",
                index, ret, errstr(ret));
            return EXIT_FAILURE;
        }
    }

    if (app_config.mjpeg_enable) {
        int index = take_next_free_channel(true);
    
        if (ret = create_vpss_chn(index, app_config.mjpeg_width, app_config.mjpeg_height,
            app_config.mjpeg_fps, 1)) {
            fprintf(stderr, 
                "Creating channel %d failed with %#x!\n%s\n", 
                index, ret, errstr(ret));
            return EXIT_FAILURE;
        }

        {
            hal_vidconfig config;
            config.width = app_config.mjpeg_width;
            config.height = app_config.mjpeg_height;
            config.codec = HAL_VIDCODEC_MJPG;
            config.mode = HAL_VIDMODE_CBR;
            config.framerate = app_config.mjpeg_fps;
            config.bitrate = app_config.mjpeg_bitrate;

            switch (plat) {
                case HAL_PLATFORM_I6: 
                case HAL_PLATFORM_I6B0:
                case HAL_PLATFORM_I6E:  ret = i6_video_create(index, &config); break;
                case HAL_PLATFORM_I6C:  ret = i6c_video_create(index, &config); break;
                case HAL_PLATFORM_I6F:  ret = i6f_video_create(index, &config); break;
                case HAL_PLATFORM_V4:   ret = v4_video_create(index, &config); break;
            }

            if (ret) {
                fprintf(stderr, 
                    "Creating encoder %d failed with %#x!\n%s\n", 
                    index, ret, errstr(ret));
                return EXIT_FAILURE;
            }
        }

        if (ret = bind_vpss_venc(index, app_config.mjpeg_fps, 1)) {
            fprintf(stderr, 
                "Binding channel %d failed with %#x!\n%s\n",
                index, ret, errstr(ret));
            return EXIT_FAILURE;
        }
    }

    if (app_config.jpeg_enable) {
        ret = jpeg_init();
        if (ret) {
            fprintf(stderr, "JPEG initialization failed with %#x!\n", ret);
            return EXIT_FAILURE;
        }
    }

    {
        pthread_attr_t thread_attr;
        pthread_attr_init(&thread_attr);
        size_t stacksize;
        pthread_attr_getstacksize(&thread_attr, &stacksize);
        size_t new_stacksize = app_config.venc_stream_thread_stack_size;
        if (pthread_attr_setstacksize(&thread_attr, new_stacksize)) {
            fprintf(stderr, "Can't set stack size %zu\n", new_stacksize);
        }
        if (pthread_create(
                     &vencPid, &thread_attr, (void *(*)(void *))venc_thread, NULL)) {
            fprintf(stderr, "Starting the video encoding thread failed!\n");
            return EXIT_FAILURE;
        }
        if (pthread_attr_setstacksize(&thread_attr, stacksize)) {
            fprintf(stderr, "Can't set stack size %zu\n", stacksize);
        }
        pthread_attr_destroy(&thread_attr);
    }

    if (!access(app_config.sensor_config, 0) ^ sleep(1))
        switch (plat) {
            case HAL_PLATFORM_I6: 
            case HAL_PLATFORM_I6B0:
            case HAL_PLATFORM_I6E:  i6_config_load(app_config.sensor_config); break;
            case HAL_PLATFORM_I6C:  i6c_config_load(app_config.sensor_config); break;
            case HAL_PLATFORM_I6F:  i6f_config_load(app_config.sensor_config); break;    
        }

    fprintf(stderr, "SDK has started successfully!\n");

    return EXIT_SUCCESS;
}

int stop_sdk() {
    pthread_join(vencPid, NULL);

    if (app_config.jpeg_enable)
        jpeg_deinit();

    switch (plat) {
        case HAL_PLATFORM_I6: 
        case HAL_PLATFORM_I6B0:
        case HAL_PLATFORM_I6E:  i6_video_destroy_all(); break;
        case HAL_PLATFORM_I6C:  i6c_video_destroy_all(); break;
        case HAL_PLATFORM_I6F:  i6f_video_destroy_all(); break;
        case HAL_PLATFORM_V4:   v4_video_destroy_all(); break;
    }

    switch (plat) {
        case HAL_PLATFORM_I6: 
        case HAL_PLATFORM_I6B0:
        case HAL_PLATFORM_I6E:  i6_pipeline_destroy(); break;
        case HAL_PLATFORM_I6C:  i6c_pipeline_destroy(); break;
        case HAL_PLATFORM_I6F:  i6f_pipeline_destroy(); break;
        case HAL_PLATFORM_V4:   v4_pipeline_destroy(); break;   
    }

    if (isp_thread)
        pthread_join(ispPid, NULL);

    switch (plat) {
        case HAL_PLATFORM_I6: 
        case HAL_PLATFORM_I6B0:
        case HAL_PLATFORM_I6E:  i6_system_deinit(); break;
        case HAL_PLATFORM_I6C:  i6c_system_deinit(); break;
        case HAL_PLATFORM_I6F:  i6f_system_deinit(); break;
        case HAL_PLATFORM_V4:   v4_system_deinit(); break;  
    }

    switch (plat) {
        case HAL_PLATFORM_V4: v4_sensor_deinit(); break;
    }

    switch (plat) {
        case HAL_PLATFORM_I6: 
        case HAL_PLATFORM_I6B0:
        case HAL_PLATFORM_I6E:  i6_hal_deinit(); break;
        case HAL_PLATFORM_I6C:  i6c_hal_deinit(); break;
        case HAL_PLATFORM_I6F:  i6f_hal_deinit(); break;
        case HAL_PLATFORM_V4:   v4_hal_deinit(); break;
    }

    fprintf(stderr, "SDK had stopped successfully!\n");
    return EXIT_SUCCESS;
}