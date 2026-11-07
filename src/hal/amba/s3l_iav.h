#pragma once

#include <stdint.h>

#define S3L_IOC_GET_IAV_STATE       0x80045610u
#define S3L_IOC_ENTER_IDLE          0x00005611u
#define S3L_IOC_ENABLE_PREVIEW      0x40045612u
#define S3L_IOC_START_ENCODE        0x40045613u
#define S3L_IOC_STOP_ENCODE         0x40045614u
#define S3L_IOC_SET_SYSRES          0x40045615u
#define S3L_IOC_GET_SYSRES          0xc0045615u
#define S3L_IOC_QUERY_BUF           0xc0045618u
#define S3L_IOC_QUERY_DESC          0xc0045619u
#define S3L_IOC_SET_SRCBUF_SETUP    0x40045630u
#define S3L_IOC_GET_SRCBUF_SETUP    0xc0045630u
#define S3L_IOC_SET_STREAM_FORMAT   0x40045634u
#define S3L_IOC_GET_STREAM_FORMAT   0xc0045634u
#define S3L_IOC_SET_H264_CFG        0x40045638u
#define S3L_IOC_GET_H264_CFG        0xc0045638u
#define S3L_IOC_SET_STREAM_CFG      0x40045636u
#define S3L_IOC_GET_STREAM_CFG      0xc0045636u
#define S3L_IOC_SET_MJPEG_CFG       0x40045639u
#define S3L_IOC_SET_H265_CFG        0x4004563cu
#define S3L_IOC_GET_H265_CFG        0xc004563cu
#define S3L_IOC_GET_MJPEG_CFG       0xc0045639u
#define S3L_IOC_GET_STATISTICS      0x80047080u
#define S3L_IOC_IMG_CONFIG_STATISTICS 0x40047001u

#define S3L_IOC_VIN_SET_MODE        0x40107303u
#define S3L_IOC_VIN_SET_FPS         0x40087304u
#define S3L_IOC_VIN_GET_DEVINFO     0x803c7301u
#define S3L_IOC_VIN_GET_FPS         0x80087304u
#define S3L_IOC_VIN_GET_VIDEOINFO   0x80247305u
#define S3L_IOC_VIN_SET_MIRROR      0x400c730au

#define S3L_SZ_SYSRES       464
#define S3L_SZ_SRCBUF_SETUP 232
#define S3L_SZ_STREAM_FMT   36
#define S3L_SZ_H264_CFG     52
#define S3L_SZ_QUERYDESC    960

#define S3L_IAV_STATE_INIT      0xff
#define S3L_IAV_STATE_IDLE      0
#define S3L_IAV_STATE_PREVIEW   1
#define S3L_IAV_STATE_ENCODING  2

#define S3L_BUFFER_BSB          1
#define S3L_DESC_FRAME          0
#define S3L_STREAM_TYPE_H264    1
#define S3L_STREAM_TYPE_MJPEG   2
#define S3L_STREAM_TYPE_H265    3

#define S3L_STMCFG_FPS              0x0000
#define S3L_STMCFG_H264_BITRATE     0x1001
#define S3L_STMCFG_H264_FORCE_IDR   0x1002
#define S3L_STMCFG_H265_BITRATE     0x3001
#define S3L_STMCFG_H265_FORCE_IDR   0x3002
#define S3L_BRC_CBR                 1
#define S3L_BRC_VBR                 3

#define S3L_H264_W_GOP              1
#define S3L_H264_W_IDR_PROFILE      2
#define S3L_SRCBUF_TYPE_ENCODE  1
#define S3L_SRCBUF_TYPE_PREVIEW 2
#define S3L_SRCBUF1_MAX         720

#define S3L_VIN_FPS(fps)        ((uint32_t)((512000000ull + (fps) / 2) / (fps)))

struct s3l_vin_mode { uint32_t vsrc_id, mode, hdr_mode, bits; };
struct s3l_vin_fps  { uint32_t vsrc_id, fps; };
struct s3l_vin_info { uint32_t vsrc_id, video_mode, width, height, fps, rsvd[4]; };

#define S3L_SYSRES_ENC_MODE         0x00
#define S3L_SYSRES_MAX_NUM_ENC      0x01
#define S3L_SYSRES_BUF_MAX(i)       (0x04 + 8 * (i))
#define S3L_SYSRES_STREAM_MAX(i)    (0x3c + 8 * (i))
#define S3L_SYSRES_EXTRA_A4         0xa4
#define S3L_SYSRES_C0               0xc0
#define S3L_SYSRES_EXTRA_D8         0xd8
#define S3L_SYSRES_DC               0xdc

#define S3L_SRCBUF_SIZE(i)          (0x00 + 8 * (i))
#define S3L_SRCBUF_MAIN_WIN_A       0x30
#define S3L_SRCBUF_MAIN_WIN_B       0x38
#define S3L_SRCBUF_SUB_INPUT(k)     (0x48 + 16 * (k))
#define S3L_SRCBUF_TYPE(i)          (0xa8 + 4 * (i))

#define S3L_STRFMT_ID               0x00
#define S3L_STRFMT_TYPE             0x04
#define S3L_STRFMT_BUF_ID           0x08
#define S3L_STRFMT_ENC_WIN          0x0c

#define S3L_DESC_W_ID               2
#define S3L_DESC_W_TIMEOUT_MS       3
#define S3L_DESC_W_DATA_OFF         4
#define S3L_DESC_W_STREAM_TYPE      5
#define S3L_DESC_W_SLICE            16
#define S3L_DESC_W_PIC_TYPE         6
#define S3L_DESC_W_FRAME_NUM        7
#define S3L_DESC_W_PTS              8
#define S3L_DESC_W_SIZE             13

#define S3L_BUFFER_IMG          8
#define S3L_IMG_RINGS_OFF       0x500000
#define S3L_IMG_RINGS_LEN       0x3000
#define S3L_IOC_IMG_SET(nr)     (0x40017000u | (nr))
#define S3L_IOC_IMG_UPDATE(nr)  (0x40047000u | (nr))

#define S3L_ISP_MAGIC           "S3LISP01"
#define S3L_ISP_DEFAULT         "/etc/sensors/s3l_isp.bin"
#define S3L_ISP_MAGIC2          "S3LISP02"
struct s3l_isp_hdr { char magic[8]; uint32_t ops, img_len; };
struct s3l_isp_op  { uint8_t kind, nr, rsvd[2]; uint32_t offset, flags; };

#define S3L_IOC_SET_OVERLAY     0x40045660u
#define S3L_IOC_GET_OVERLAY     0xc0045660u
#define S3L_BUFFER_OVERLAY      4
#define S3L_OVL_AREAS           10
#define S3L_OVL_CLUT_LEN        1024
#define S3L_OVL_DATA_OFF        0x8000
struct s3l_ovl_area { uint16_t enable, width, pitch, height; uint32_t size; uint16_t x, y; uint32_t clut, data; };
struct s3l_ovl { uint32_t stream, flags; struct s3l_ovl_area area[S3L_OVL_AREAS]; };
_Static_assert(sizeof(struct s3l_ovl) == 248, "IAV overlay insert is 248 bytes on S3L");

#define S3L_NL_PORT_IMAGE   20
#define S3L_NL_MSG_LEN      128
struct s3l_nl_msg { uint32_t pid, port, type, dir, cmd, status; uint8_t pad[S3L_NL_MSG_LEN - 24]; };
