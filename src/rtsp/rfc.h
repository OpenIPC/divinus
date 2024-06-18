#ifndef __RTSP_RFC_H
#define __RTSP_RFC_H
#include <sys/types.h>

/*
 * Current protocol version.
 */
#define RTP_VERSION    2

#define RTP_SEQ_MOD (1<<16)
#define RTP_MAX_SDES 255      /* maximum text length for SDES */

#define H264_NAL_TYPE_SEI 6
#define H264_NAL_TYPE_SPS 7
#define H264_NAL_TYPE_PPS 8
#define H265_NAL_TYPE_VPS 32
#define H265_NAL_TYPE_SPS 33
#define H265_NAL_TYPE_PPS 34
#define H265_NAL_TYPE_SEIP 39
#define H265_NAL_TYPE_SEIS 40

typedef enum {
    RTCP_SR   = 200,
    RTCP_RR   = 201,
    RTCP_SDES = 202,
    RTCP_BYE  = 203,
    RTCP_APP  = 204
} rtcp_type_t;

typedef enum {
    RTCP_SDES_END   = 0,
    RTCP_SDES_CNAME = 1,
    RTCP_SDES_NAME  = 2,
    RTCP_SDES_EMAIL = 3,
    RTCP_SDES_PHONE = 4,
    RTCP_SDES_LOC   = 5,
    RTCP_SDES_TOOL  = 6,
    RTCP_SDES_NOTE  = 7,
    RTCP_SDES_PRIV  = 8
} rtcp_sdes_type_t;

/*
 * RTCP common header word
 */
typedef struct {
#ifdef __RTSP_BIG_ENDIAN
    unsigned int version:2;   /* protocol version */
    unsigned int p:1;         /* padding flag */
    unsigned int count:5;     /* varies by packet type */
#else
    unsigned int count:5;     /* varies by packet type */
    unsigned int p:1;         /* padding flag */
    unsigned int version:2;   /* protocol version */
#endif
    unsigned int pt:8;        /* RTCP packet type */
    unsigned short length;    /* pkt len in words, w/o this word */
} rtcp_common_t;

/*
 * Big-endian mask for version, padding bit and packet type pair
 */
#define RTCP_VALID_MASK (0xc000 | 0x2000 | 0xfe)
#define RTCP_VALID_VALUE ((RTP_VERSION << 14) | RTCP_SR)

/*
 * Reception report block
 */
typedef struct {
    unsigned int ssrc;        /* data source being reported */
    unsigned int fraction:8;  /* fraction lost since last SR/RR */
    int lost:24;              /* cumul. no. pkts lost (signed!) */
    unsigned int last_seq;    /* extended last seq. no. received */
    unsigned int jitter;      /* interarrival jitter */
    unsigned int lsr;         /* last SR packet from this source */
    unsigned int dlsr;        /* delay since last SR packet */
} rtcp_rr_t;

/*
 * SDES item
 */
typedef struct {
    unsigned char type;       /* type of item (rtcp_sdes_type_t) */
    unsigned char length;     /* length of item (in octets) */
    char data[1];             /* text, not null-terminated */
} rtcp_sdes_item_t;

/*
 * One RTCP packet
 */
typedef struct {
    rtcp_common_t common;     /* common header */
    union {
        /* sender report (SR) */
        struct {
            unsigned int ssrc;     /* sender generating this report */
            unsigned int ntp_sec;  /* NTP timestamp */
            unsigned int ntp_frac;
            unsigned int rtp_ts;   /* RTP timestamp */
            unsigned int psent;    /* packets sent */
            unsigned int osent;    /* octets sent */
            rtcp_rr_t rr[1];       /* variable-length list */
        } sr;

        /* reception report (RR) */
        struct {
            unsigned int ssrc; /* receiver generating this report */
            rtcp_rr_t rr[1];   /* variable-length list */
        } rr;

        /* source description (SDES) */
        struct rtcp_sdes {
            unsigned int src;         /* first SSRC/CSRC */
            rtcp_sdes_item_t item[1]; /* list of SDES items */
        } sdes;

        /* BYE */
        struct {
            unsigned int src[1]; /* list of sources */
            /* can't express trailing text for reason */
        } bye;
    } r;
} rtcp_t;

typedef struct rtcp_sdes rtcp_sdes_t;

#endif