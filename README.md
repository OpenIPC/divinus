![OpenIPC logo][logo]

## Divinus
**_Divinus is a new open source, multi-platform streamer_**

### Inner workings

This project strives to adopt a KISS "keep it simple, stupid!" structure while being as portable as can be.

Compared to most streamer software solutions available to this day, it attaches to the application-specific peripherals of a system-on-a-chip using an easy to understand HAL platform (hardware abstraction layer) proper to each chip series it supports.

Since it is using runtime dynamic linking, its executable remains particularly compact and can be run on a multitude of environments, including in a tethered context (e.g. running from a temporary filesystem on read-only systems).

In spite of these design choices, Divinus boasts numerous features that cater to a wide range of uses you will be able to make of it.


### Supported hardware and features

| SoC Family              | Audio Stream | JPEG Snapshot | fMP4 Stream | RTSP Stream | On-Screen Display* |
|-------------------------|:------------:|:-------------:|:-----------:|:-----------:|:------------------:|
| AK3918                  | ↻            | ↻            | ↻           | ↻           | ↻                 |
| CV181x[^1]              | ↻            | ↻            | ↻           | ↻           | ↻                 |
| GM813x[^2]              | ✔️           | ✔️           | ✔️          | ✔️          | ✔️                |
| Hi3516AV100[^3]         | ✔️           | ✔️           | ✔️          | ✔️          | ✔️                |
| Hi3516CV100[^4]         | ↻            | ↻            | ↻           | ↻           | ↻                 |
| Hi3516CV200[^5]         | ✔️           | ✔️           | ✔️          | ✔️          | ✔️                |
| Hi3516CV300[^6]         | ✔️           | ✔️           | ✔️          | ✔️          | ✔️                |
| Hi3516CV500[^7]         | ✔️           | ✔️           | ✔️          | ✔️          | ✔️                |
| Hi3516EV200[^8]         | ✔️           | ✔️           | ✔️          | ✔️          | ✔️                |
| GK7205V200[^9]          | ✔️           | ✔️           | ✔️          | ✔️          | ✔️                |
| Hi3519V100[^10]         | ✔️           | ✔️           | ✔️          | ✔️          | ✔️                |
| Hi3519AV100             | ✔️           | ✔️           | ✔️          | ✔️          | ✔️                |
| Hi3559AV100             | ✔️           | ✔️           | ✔️          | ✔️          | ✔️                |
| RV11xx[^11]             | ↻            | ↻            | ↻           | ↻           | ↻                 |
| T31 series              | ✔️           | ✔️           | ✔️          | ✔️          | ✔️                |
| infinity3[^12]          | ↻            | ↻            | ↻           | ↻           | ↻                 |
| infinity6[^13]          | ✔️           | ✔️           | ✔️          | ✔️          | ✔️                |
| infinity6b0[^14]        | ✔️           | ✔️           | ✔️          | ✔️          | ✔️                |
| infinity6e[^15]         | ✔️           | ✔️           | ✔️          | ✔️          | ✔️                |
| infinity6c[^16]         | ✔️           | ✔️           | ✔️          | ✔️          | ✔️                |
| infinity6f[^17]         | ✔️           | ✔️           | ✔️          | ✔️          | ✔️                |

_✔️ - supported, ↻ - in development, ✗ - unsupported, ⁿ/ₐ - not supported by hardware_

_* At the moment, only text, 24-bit and 32-bit RGB overlays are handled, matricial formats and covers are to follow_

[^1]: CV181x\[C/H\], SG200\[0/2\]
[^2]: GM813\[5/6/8\]\(S\)
[^3]: Hi3516AV100 and Hi3516DV100
[^4]: Hi3516CV100, Hi3518AV100, Hi3518CV100 and Hi3518EV100
[^5]: Hi3516CV200 and Hi3518EV20\[0/1\]
[^6]: Hi3516CV300 and Hi3516EV100
[^7]: Hi3516AV300, Hi3516CV500 and Hi3516DV300
[^8]: Hi3516DV200, Hi3516EV200/300 and Hi3518EV300
[^9]: GK7202V300, GK7205V200/300 and GK7605V100
[^10]: Hi3516AV200 and Hi3519V101
[^11]: RV110\[3/7/8/9\] and RV1106\(G2/G3\)
[^12]: MSC313E, MSC316\[DC/Q\] and MSC318
[^13]: SSC323, SSC325\(D/DE\) and SSC327\(D/DE/Q\)
[^14]: SSC33\[3/5/7\]\(DE\)
[^15]: SSC30K\[D/Q\], SSC336\[D/Q\], SSC338\[D/G/Q\] and SSC339G
[^16]: SSC377\(D/DE/QE\) or SSC378\[DE/QE\]
[^17]: SSC379G


### Documentation

- [Overlays](doc/overlays.md) - _doc/overlays.md_


### Roadmap

- [ ] Audio source, input gain and output volume
- [ ] Motors and PTZ control
- [ ] ONVIF services
- [ ] Additional WebUI functionalities
- [ ] Lens correction profiles
- [ ] Local recordings with motion detection
- [ ] Alternative audio codecs


### Technical support and donations

Please **_[support our project](https://openipc.org/support-open-source)_** with donations or orders for development or maintenance. Thank you!


[logo]: https://openipc.org/assets/openipc-logo-black.svg