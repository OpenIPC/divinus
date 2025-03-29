# API Endpoints Reference

This document describes the REST API endpoints available in Divinus' web server.

## General Response Format

All APIs return responses in JSON format with appropriate HTTP status codes.

## Authentication

If authentication is enabled, all requests must include a basic authentication header:

```
Authorization: Basic [ENCODED_CREDENTIALS]
```

where \[ENCODED_CREDENTIALS\] is a string representing a Base64-encoded string formatted this way: `username:password`. For example, if your username was "admin", and your password "secret", the header's value would be encoded from "admin:secret" to "YWRtaW46c2VjcmV0".

## Endpoints

### System Management

#### `/api/cmd`

Calls various integrated commands.

| Method | Parameters | Description                     |
|--------|------------|---------------------------------|
| GET    | `save`     | Saves the current configuration |

**Response**
```json
{
  "code": 0  // 0 = success, !0 = error code
}
```

#### `/api/status`

Gets live system status information.

| Method | Parameters | Description                |
|--------|------------|----------------------------|
| GET    | none       | Returns system information |

**Response**
```json
{
  "chip": "GK7205V300",
  "loadavg": [0.05, 0.07, 0.06],
  "memory": "62/121MB",
  "sensor": "imx335",
  "uptime": "7 days, 14:08:57"
}
```

#### `/api/time`

Configures or reads the real-time clock.

| Method | Parameters | Description                                |
|--------|------------|--------------------------------------------|
| GET    | `fmt`      | Time display format (used by dynamic OSDs) |
| GET    | `ts`       | Unix timestamp to set (assuming local TZ)  |

**Response**
```json
{
  "fmt": "%Y-%m-%d %H:%M:%S",
  "ts": 1716633815
}
```

### Video Configuration

#### `/api/jpeg`

Configures JPEG capture parameters.

| Method | Parameters | Description             |
|--------|------------|-------------------------|
| GET    | `width`    | Image width (px)        |
| GET    | `height`   | Image height (px)       |
| GET    | `qfactor`  | Quality factor (1-100%) |

**Response**
```json
{
  "enable": true,
  "width": 1920,
  "height": 1080,
  "qfactor": 80
}
```

#### `/api/mjpeg`

Configures the MJPEG stream.

| Method | Parameters | Description                     |
|--------|------------|---------------------------------|
| GET    | `enable`   | Enable/disable MJPEG stream     |
| GET    | `width`    | Video width (px)                |
| GET    | `height`   | Video height (px)               |
| GET    | `fps`      | Frames per second               |
| GET    | `mode`     | Compression mode (CBR, VBR, QP) |

**Response**
```json
{
  "enable": true,
  "width": 640,
  "height": 480,
  "fps": 15,
  "mode": "CBR",
  "bitrate": 1000000
}
```

#### `/api/mp4`

Configures the H.26x MP4 stream.

| Method | Parameters | Description                                |
|--------|------------|--------------------------------------------|
| GET    | `enable`   | Enable/disable MP4 stream                  |
| GET    | `width`    | Video width (px)                           |
| GET    | `height`   | Video height (px)                          |
| GET    | `fps`      | Frames per second                          |
| GET    | `bitrate`  | Bits per second                            |
| GET    | `h265`     | Use H.265 instead of H.264                 |
| GET    | `mode`     | Compression mode (CBR, VBR, QP, ABR, AVBR) |
| GET    | `profile`  | Profile (BP/BASELINE, MP/MAIN, HP/HIGH)    |

**Response**
```json
{
  "enable": true,
  "width": 1280,
  "height": 720,
  "fps": 30,
  "h265": false,
  "mode": "VBR",
  "profile": "MP",
  "bitrate": 2000000
}
```

### Audio Configuration

#### `/api/audio`

Configures the audio parameters.

| Method | Parameters | Description                   |
|--------|------------|-------------------------------|
| GET    | `enable`   | Enable/disable audio          |
| GET    | `bitrate`  | Bits per second               |
| GET    | `gain`     | Audio gain (dB amplification) |
| GET    | `srate`    | Sample rate in Hz             |

**Response**
```json
{
  "enable": true,
  "bitrate": 32000,
  "gain": 50,
  "srate": 16000
}
```

### Night Mode

#### `/api/night`

Configures the night mode parameters.

| Method | Parameters     | Description               |
|--------|----------------|---------------------------|
| GET    | `enable`       | Enable/disable night mode |
| GET    | `manual`       | Manual or automatic mode  |
| GET    | `grayscale`    | Switch to grayscale       |
| GET    | `ircut`        | Control IR-Cut filter     |
| GET    | `ircut_pin1`   | GPIO pin 1 for IR-Cut     |
| GET    | `ircut_pin2`   | GPIO pin 2 for IR-Cut     |
| GET    | `irled`        | Control IR LEDs           |
| GET    | `irled_pin`    | GPIO pin for IR LEDs      |
| GET    | `irsense_pin`  | GPIO pin for IR sensor    |
| GET    | `adc_device`   | ADC device                |
| GET    | `adc_threshold`| Activation threshold      |

**Response**
```json
{
  "active": true,
  "manual": false,
  "grayscale": true,
  "ircut": true,
  "ircut_pin1": 17,
  "ircut_pin2": 18,
  "irled": true,
  "irled_pin": 20,
  "irsense_pin": 21,
  "adc_device": "/dev/adc",
  "adc_threshold": 800
}
```

### On-Screen Display (OSD)

#### `/api/osd/{id}`

Configures text or image overlays by their ID (0-9 at the moment).

| Method | Parameters | Description                                    |
|--------|------------|------------------------------------------------|
| GET    | `text`     | Text to display (special specifiers supported) |
| GET    | `font`     | Font name to be used                           |
| GET    | `size`     | Font size (decimal in pt)                      |
| GET    | `color`    | Color (hex format, 5-bit color precision)      |
| GET    | `opal`     | Opacity level (0-255)                          |
| GET    | `pos`      | Position on main stream \[x,y\]                |
| GET    | `posx`     | X coordinate (write-only)                      |
| GET    | `posy`     | Y coordinate (write-only)                      |
| POST   | file       | Bitmap or PNG image to upload (replaces text)  |

**Response**
```json
{
  "id": 0,
  "color": "#ff0000",
  "opal": 255,
  "pos": [16, 64],
  "font": "UbuntuMono-Regular",
  "size": 15.0,
  "text": "Backyard (%T)"
}
```

## Content Streaming

### `/image.jpg`

Captures an instant JPEG image.

| Method | Parameters    | Description          |
|--------|---------------|----------------------|
| GET    | `width`       | Snapshot width (px)  |
| GET    | `height`      | Snapshot height (px) |
| GET    | `qfactor`     | Quality factor       |
| GET    | `color2gray`  | Convert to grayscale |

**Response**: JPEG file

### `/mjpeg`

Continuous MJPEG stream.

**Response**: multipart/x-mixed-replace stream

### `/video.mp4`

Continuous MP4 video stream.

**Response**: Segmented MP4 video stream

### `/video.264` or `/video.265`

Raw H.264/H.265 stream.

**Response**: Binary data stream

### `/audio.mp3`

Continuous MP3 audio stream.

**Response**: MP3 audio stream

### `/audio.pcm`

Raw PCM audio stream.

**Response**: Uncompressed PCM audio stream

## Special Commands

### `/exit`

Gracefully stops the server.

| Method | Parameters | Description      |
|--------|------------|------------------|
| GET    | none       | Stops the server |

**Response**: Message "Closing..."

### `/`

WebUI homepage.

**Response**: HTML SPA with embedded resources