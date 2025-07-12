# Configuration Reference

This document describes the fields that can be found within a configuration file for Divinus.

## System section

- **sensor_config**: Path to the sensor calibration or configuration file, if applicable (e.g., `/etc/sensors/imx415.bin`).
- **web_port**: Port number for the web server (default: `80`).
- **web_whitelist**: Array of up to 4 IP addresses or domains allowed to access the web server.
- **web_enable_auth**: Boolean to enable authentication on the API, live stream and WebUI endpoints (default: `false`).
- **web_auth_user**: Username for basic authentication (default: `admin`).
- **web_auth_pass**: Password for basic authentication (default: `12345`).
- **web_enable_static**: Boolean to enable serving static web content (default: `false`).
- **isp_thread_stack_size**: Stack size for ISP thread, if applicable (default: `16384`).
- **venc_stream_thread_stack_size**: Stack size for video encoding stream thread (default: `16384`).
- **web_server_thread_stack_size**: Stack size for web server thread (default: `65536`).
- **time_format**: Format for displaying time, refer to strftime() modifiers for exact parameters (e.g., `"%Y-%m-%d %H:%M:%S"`).
- **watchdog**: Watchdog timer in seconds, where 0 means disabled (default: `30`).

## Night mode section

- **enable**: Boolean to activate night mode support.
- **ir_cut_pin1**: GPIO number for IR cut filter control (normal state pin).
- **ir_cut_pin2**: GPIO number for IR cut filter control (inverted state pin).
- **ir_led_pin**: GPIO number for IR LED control.
- **ir_sensor_pin**: GPIO number for PIR motion sensor or similar digital toggle.
- **check_interval_s**: Interval in seconds to check night mode conditions.
- **pin_switch_delay_us**: Delay in microseconds before switching GPIO pins, must be used to protect cut filter coils from burning.
- **adc_device**: Path to the ADC device used for night mode.
- **adc_threshold**: Threshold raw value to trigger night mode, depends on the bitness of the given ADC device.

## ISP section

- **mirror**: Boolean to turn on image mirroring (default: `false`).
- **flip**: Boolean to turn on image flipping (default: `false`).
- **antiflicker**: Antiflicker setting in Hz (default: `60`).

## mDNS section

- **enable**: Boolean to turn on mDNS announcer, lets LAN users access the device by a domain name, its configured hostname followed by .local (default: `false`).

## ONVIF section

- **enable**: Boolean to activate ONVIF services (default: `false`).
- **enable_auth**: Boolean to turn on ONVIF authentication (default: `false`).
- **auth_user**: Username for ONVIF authentication (default: `admin`).
- **auth_pass**: Password for ONVIF authentication (default: `12345`).

## RTSP section

- **enable**: Boolean to activate the integrated RTSP server (default: `true`).
- **enable_auth**: Boolean to turn on RTSP authentication (default: `false`).
- **auth_user**: Username for RTSP authentication (default: `admin`).
- **auth_pass**: Password for RTSP authentication (default: `12345`).
- **port**: Port number for RTSP server (default: `554`).

## Record section

- **enable**: Boolean to allow or block recording operations (default: `false`).
- **continuous**: Boolean to turn on continuous recording at launch (default: `false`).
- **path**: Path to save recordings (e.g., `/mnt/sdcard/recordings`).
- **filename**: String for a fixed destination file, leave empty to use incremental numbering
- **segment_duration**: Target duration for a recording in seconds
- **segment_size**: Target file size for a recording in bytes

## Stream section

- **enable**: Boolean to turn on special streaming methods (default: `false`).
- **udp_srcport**: Source port for UDP streaming (default: `5600`).
- **dest**: List of destination URLs for streaming (e.g., `udp://239.255.255.0:5600`).

## Audio section

- **enable**: Boolean to activate or deactivate audio functionality.
- **bitrate**: Audio bitrate in kbps (e.g., `128`).
- **gain**: Audio gain in decibels (e.g., `0` for no gain).
- **srate**: Audio sampling rate in Hz (e.g., `44100`).

## MP4 section

- **enable**: Boolean to activate or deactivate MP4 encoding.
- **codec**: Codec used for encoding (H.264 or H.265).
- **mode**: Encoding mode.
- **width**: Video width in pixels.
- **height**: Video height in pixels.
- **fps**: Frames per second.
- **gop**: Interval between keyframes.
- **profile**: Encoding profile.
- **bitrate**: Bitrate in kbps.

## OSD section

- **enable**: Boolean to turn on On-Screen Display regions globally, used to reduce resource usage or let another app manage the functionality (default: `true`).
- **regX_img**: Path to the image for OSD region X.
- **regX_text**: Text displayed in OSD region X.
- **regX_font**: Font used for text in OSD region X.
- **regX_opal**: Opacity of OSD region X.
- **regX_posx**: X position of OSD region X.
- **regX_posy**: Y position of OSD region X.
- **regX_size**: Size of the text or image in OSD region X.
- **regX_color**: Color of the text or image in OSD region X.
- **regX_outl**: Outline color of the text in OSD region X.
- **regX_thick**: Thickness of the text outline in OSD region X.

## JPEG section

- **enable**: Boolean to activate or deactivate JPEG encoding.
- **width**: Image width in pixels.
- **height**: Image height in pixels.
- **qfactor**: JPEG compression quality factor.

## MJPEG section

- **enable**: Boolean to activate or deactivate MJPEG encoding.
- **mode**: Encoding mode.
- **width**: Video width in pixels.
- **height**: Video height in pixels.
- **fps**: Frames per second.
- **bitrate**: Bitrate in kbps.

## HTTP POST section

- **enable**: Boolean to activate or deactivate HTTP POST requests.
- **host**: Host for HTTP POST requests.
- **url**: URL for HTTP POST requests.
- **login**: Login for HTTP POST authentication.
- **password**: Password for HTTP POST authentication.
- **width**: Image width sent in pixels.
- **height**: Image height sent in pixels.
- **interval**: Interval between requests in seconds.
- **qfactor**: JPEG compression quality factor for HTTP POST.