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

static inline int __rtcp_send_sr(struct connection_item_t *con, int track_id);


/******************************************************************************
 *              INLINE FUNCTIONS
 ******************************************************************************/
static inline int __rtcp_send_sr(struct connection_item_t *con, int track_id)
{
    struct timeval tv;
    unsigned int ts_h, ts_l;
    int send_bytes;
    struct sockaddr_in to_addr;
    transport_t *t;

    ASSERT(track_id >= 0 &&
        track_id < (int)(sizeof(con->trans) / sizeof(con->trans[0])),
        return FAILURE);
    t = &con->trans[track_id];

    /* Unsetup track has no transport; fd 0 is stdin. */
    if (!t->server_port_rtp && !t->is_tcp) return SUCCESS;

    ASSERT(gettimeofday(&tv,NULL) == 0, return FAILURE);

    ts_h = (unsigned int)tv.tv_sec + 2208988800U;
    ts_l = (((double)tv.tv_usec) / 1e6) * 4294967296.0;

    rtcp_t rtcp = { common: {version: 2, length: htons(8), p:0, count: 0, pt:RTCP_SR},
        r: { sr: { ssrc: htonl(con->ssrc),
            ntp_sec: htonl(ts_h),
            ntp_frac: htonl(ts_l),
            rtp_ts: htonl(t->rtp_timestamp),
            psent: htonl(t->rtcp_packet_cnt),
            osent: htonl(t->rtcp_octet)}}};

    if (t->is_tcp) {
        /* Queued behind the media like any interleaved packet; a client
         * whose queue is full simply misses this report */
        pthread_mutex_lock(&con->write_mutex);
        if (__tcp_pump(con) == SUCCESS &&
            __tcp_queue(con, t->channel_rtcp, &rtcp, 36))
            __tcp_pump(con);
        pthread_mutex_unlock(&con->write_mutex);
    } else {
        to_addr = con->addr;
        to_addr.sin_port = htons(t->client_port_rtcp);

        ASSERT((send_bytes = send(t->server_rtcp_fd,
            &(rtcp), 36, 0)) == 36, ({
                    ERR("send:%d:%s\n", send_bytes, strerror(errno));
                    return FAILURE;}));
    }

    t->rtcp_packet_cnt = 0;
    t->rtcp_octet = 0;
    t->rtcp_tick = t->rtcp_tick_org;

    return SUCCESS;
}

#endif
