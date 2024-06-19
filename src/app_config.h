#pragma once

#include "hal/config.h"
#include "hal/support.h"

struct AppConfig {
    // [system]
    char sensor_config[128];
    unsigned short web_port;
    bool web_enable_auth;
    char web_auth_user[32];
    char web_auth_pass[32];
    bool web_enable_static;
    unsigned int isp_thread_stack_size;
    unsigned int venc_stream_thread_stack_size;
    unsigned int web_server_thread_stack_size;
    unsigned int watchdog;

    bool motion_detect_enable;
    bool osd_enable;
    bool rtsp_enable;

    // [video_0]
    bool mp4_enable;
    bool mp4_codecH265;
    unsigned int mp4_mode;
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
    unsigned int mjpeg_mode;
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

    // [isp]
    bool mirror;
    bool flip;
    int antiflicker;

    // [night_mode]
    bool night_mode_enable;
    unsigned int ir_cut_pin1;
    unsigned int ir_cut_pin2;
    unsigned int ir_sensor_pin;
    unsigned int check_interval_s;
    unsigned int pin_switch_delay_us;
    char adc_device[128];
    int adc_threshold;
};

extern struct AppConfig app_config;
enum ConfigError parse_app_config(void);
