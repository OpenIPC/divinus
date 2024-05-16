#pragma once

#include "config.h"

struct AppConfig {
    // [system]
    char sensor_config[128];

    bool rtsp_enable;

    // [video_0]
    bool mp4_enable;
    unsigned int mp4_fps;
    unsigned int mp4_width;
    unsigned int mp4_height;
    unsigned int mp4_profile;
    unsigned int mp4_bitrate;

    // [jpeg]
    bool jpeg_enable;
    unsigned int jpeg_width;
    unsigned int jpeg_height;
    unsigned int jpeg_qfactor;

    // [mjpeg]
    bool mjpeg_enable;
    unsigned int mjpeg_fps;
    unsigned int mjpeg_width;
    unsigned int mjpeg_height;
    unsigned int mjpeg_bitrate;

    // [http_post_jpeg]
    bool http_post_enable;
    char http_post_host[128];
    char http_post_url[128];
    char http_post_login[128];
    char http_post_password[128];
    unsigned int http_post_width;
    unsigned int http_post_height;
    unsigned int http_post_qfactor;
    unsigned int http_post_interval;

    bool osd_enable;
    bool motion_detect_enable;

    unsigned int web_port;
    bool web_enable_static;

    unsigned int isp_thread_stack_size;
    unsigned int venc_stream_thread_stack_size;
    unsigned int web_server_thread_stack_size;

    unsigned int align_width;
    unsigned int max_pool_cnt;
    unsigned int blk_cnt;
    bool mirror;
    bool flip;

    // [night_mode]
    bool night_mode_enable;
    unsigned int ir_cut_pin1;
    unsigned int ir_cut_pin2;
    unsigned int ir_sensor_pin;
    unsigned int check_interval_s;
    unsigned int pin_switch_delay_us;
};

extern struct AppConfig app_config;

enum ConfigError parse_app_config(const char *path);
