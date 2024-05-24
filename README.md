![OpenIPC logo][logo]

## Divinus
**_Divinus is a new open source, multi-platform streamer_**

### Inner workings

This project strives to adopt a KISS "keep it simple, stupid!" structure while being as portable as can be.

Compared to most streamer software solutions available to this day, it attaches to the application-specific peripherals of a system-on-a-chip using an easy to understand HAL platform (hardware abstraction layer) proper to each chip series it supports.

Since it is exclusively using dynamic linking, its executable remains particularly compact and can be run on a multitude of environments, including in a tethered context (e.g. running from a temporary filesystem on read-only systems).

In spite of these design choices, Divinus boasts numerous features that cater to a wide range of uses you will be able to make of it.


### Supported hardware and features

| SoC Family              | Audio Stream | JPEG Snapshot | RTSP Stream | Motion Detect | On-Screen Display* |
|-------------------------|:------------:|:-------------:|:-----------:|:-------------:|:------------------:|
| Hi3516AV300/CV500/DV300 | ✗            | ✔️           | ✔️          | ✗            | ✔️                 |
| Hi3516DV200             | ✗            | ✔️           | ✔️          | ✗            | ✔️                 |
| Hi3516EV200/300         | ✗            | ✔️           | ✔️          | ✗            | ✔️                 |
| Hi3518EV300             | ✗            | ✔️           | ✔️          | ✗            | ✔️                 |
| GK7605V100              | ✗            | ✔️           | ✔️          | ✗            | ✔️                 |
| GK7205V200/300          | ✗            | ✔️           | ✔️          | ✗            | ✔️                 |
| GK7202V300              | ✗            | ✔️           | ✔️          | ✗            | ✔️                 |
| Hi3519AV100             | ✗            | ✔️           | ✔️          | ✗            | ✔️                 |
| Hi3559AV100             | ✗            | ✔️           | ✔️          | ✗            | ✔️                 |
| infinity6/6b0/6e        | ↻            | ✔️           | ✔️          | ✗            | ✔️                 |
| infinity6c              | ↻            | ✔️           | ✔️          | ✗            | ✔️                 |
| infinity6f              | ↻            | ✔️           | ✔️          | ✗            | ✔️                 |

_✔️ - supported, ↻ - in development, ✗ - unsupported, ⁿ/ₐ - not supported by hardware_

_* At the moment, only text overlays are handled, eventually bitmaps, matricial formats and covers are to follow_


### Technical support and donations

Please **_[support our project](https://openipc.org/support-open-source)_** with donations or orders for development or maintenance. Thank you!


[logo]: https://openipc.org/assets/openipc-logo-black.svg
