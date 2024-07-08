#ifndef _RTSP_RTCP_H
#define _RTSP_RTCP_H

#include <stdlib.h>
#include <stdio.h>
#include "rtp.h"
#include "rfc.h"
#include "rtsp.h"
#include "common.h"
/******************************************************************************
 *              DECLARATIONS
 ******************************************************************************/

static inline int __rtcp_send_sr(struct connection_item_t *con);


/******************************************************************************
 *              INLINE FUNCTIONS
 ******************************************************************************/
static inline int __rtcp_send_sr(struct connection_item_t *con)
{
    struct timeval tv;
    unsigned int ts_h; 
    unsigned int ts_l; 
    int send_bytes;
    struct sockaddr_in to_addr;

    ASSERT(gettimeofday(&tv,NULL) == 0, return FAILURE);

    ts_h = (unsigned int)tv.tv_sec + 2208988800U;
    ts_l = (((double)tv.tv_usec) / 1e6) * 4294967296.0;

    rtcp_t rtcp = { common: {version: 2, length: htons(6), p:0, count: 0, pt:RTCP_SR},
        r: { sr: { ssrc: htonl(con->ssrc),
            ntp_sec: htonl(ts_h),
            ntp_frac: htonl(ts_l),
            rtp_ts: htonl(con->trans[con->track_id].rtp_timestamp),
            psent: htonl(con->trans[con->track_id].rtcp_packet_cnt),
            osent: htonl(con->trans[con->track_id].rtcp_octet)}}};

    to_addr = con->addr;
    to_addr.sin_port = con->trans[con->track_id].client_port_rtcp;

    ASSERT((send_bytes = send(con->trans[con->track_id].server_rtcp_fd,
        &(rtcp),36,0)) == 36, ({
                ERR("send:%d:%s¥n",send_bytes,strerror(errno));
                return FAILURE;}));

    con->trans[con->track_id].rtcp_packet_cnt = 0;
    con->trans[con->track_id].rtcp_octet = 0;
    con->trans[con->track_id].rtcp_tick = con->trans[con->track_id].rtcp_tick_org;

    return SUCCESS;
}

#endif