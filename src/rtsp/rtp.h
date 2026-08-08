#ifndef _RTSP_RTP_H
#define _RTSP_RTP_H

#if defined (__cplusplus)
extern "C" {
#endif

#include "../fmt/nal.h"
#include "../hal/tools.h"

/******************************************************************************
 *              DEFINITIONS
 ******************************************************************************/
#define __RTP_MAXPAYLOADSIZE 1460

/******************************************************************************
 *              DATA STRUCTURES
 ******************************************************************************/
/*
 * RTP data header
 */
typedef struct {
#ifdef __RTSP_BIG_ENDIAN
    unsigned int version:2;   /* protocol version */
    unsigned int p:1;         /* padding flag */
    unsigned int x:1;         /* header extension flag */
    unsigned int cc:4;        /* CSRC count */
    unsigned int m:1;         /* marker bit */
    unsigned int pt:7;        /* payload type */
#else
    unsigned int cc:4;        /* CSRC count */
    unsigned int x:1;         /* header extension flag */
    unsigned int p:1;         /* padding flag */
    unsigned int version:2;   /* protocol version */
    unsigned int pt:7;        /* payload type */
    unsigned int m:1;         /* marker bit */
#endif
    unsigned int seq:16;      /* sequence number */
    unsigned int ts;          /* timestamp */
    unsigned int ssrc;        /* synchronization source */
    //unsigned int csrc[1];     /* optional CSRC list */
} rtp_hdr_t;

struct nal_rtp_t {
    struct {
        rtp_hdr_t header;
        unsigned char payload[__RTP_MAXPAYLOADSIZE];
    } packet;
    int    rtpsize;
    struct list_t list_entry;
};

#if defined (__cplusplus)
}
#endif

#endif
