#ifndef _RTSP_RTP_H
#define _RTSP_RTP_H

#if defined (__cplusplus)
extern "C" {
#endif

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

/******************************************************************************
 *              DECLARATIONS
 ******************************************************************************/
static inline int __split_nal(unsigned char *buf, unsigned char **nalptr, size_t *p_len, size_t max_len);

/******************************************************************************
 *              INLINE FUNCTIONS
 ******************************************************************************/
static inline int __split_nal(unsigned char *buf, unsigned char **nalptr, size_t *p_len, size_t max_len)
{
    int i;
    int start = -1;

    for(i = (*nalptr) - buf + *p_len;i<max_len-5;i++) {
        if(buf[i] == 0x00 &&
                buf[i+1] == 0x00 &&
                buf[i+2] == 0x00 &&
                buf[i+3] == 0x01) {
            if(start == -1){
                i += 4;
                start = i;
            } else {
                *nalptr = &(buf[start]);
                while(buf[i-1] == 0) i--;
                *p_len = i - start;
                return SUCCESS;
            }
        }
    }

    if(start == -1) {
        /* malformed NAL */
        return FAILURE;
    }

    *nalptr = &(buf[start]);
    *p_len = max_len + 2 - start;

    return SUCCESS;
}

#if defined (__cplusplus)
}
#endif

#endif