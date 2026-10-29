#include <sys/socket.h>
#include <sys/types.h>
#include <sys/time.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <errno.h>
#include <fcntl.h>

#include "rtsp_server.h"
#include "common.h"
#include "rtsp.h"
#include "list.h"
#include "hash.h"
#include "thread.h"
#include "rfc.h"
#include "rtp.h"
#include "rtcp.h"
#include "bufpool.h"
#include "mime.h"

/******************************************************************************
 *              PRIVATE DEFINITIONS
 ******************************************************************************/
//static void *rtpThrFxn(void *v);
static inline int __rtp_send(struct nal_rtp_t *rtp, struct list_head_t *trans_list);
static inline int __rtp_send_eachconnection(struct list_t *e, void *v);
static inline int __rtp_setup_transfer(struct list_t *e, void *v);
static inline int __transfer_nal_h26x(struct list_head_t *trans_list, unsigned char *nalptr, size_t nalsize, char isH265);
static inline int __transfer_nal_mpga(struct list_head_t *trans_list, unsigned char *ptr, size_t size);
static inline int __retrieve_sprop(rtsp_handle h, unsigned char *buf, size_t len);

struct __transfer_set_t {
    struct list_head_t list_head;
    rtsp_handle h;
    int track_id;
};

static unsigned int __frame_ts_video, __frame_ts_audio;

/* Room kept for audio packets queued while a video frame is being written */
#define RTSP_TX_SPARE (8 * 1024)

struct __frame_t {
    unsigned int bytes;
};

static int __tcp_flush_each(struct list_t *e, void *v)
{
    struct transfer_item_t *trans;
    struct connection_item_t *con;
    list_upcast(trans, e);
    MUST(con = trans->con, return FAILURE);
    if (!con->tx_buf) return SUCCESS;
    pthread_mutex_lock(&con->write_mutex);
    if (__tcp_pump(con) != SUCCESS)
        ERR("send (interleaved):%s\n", strerror(errno));
    pthread_mutex_unlock(&con->write_mutex);
    return SUCCESS;
}

/* Decides whether an interleaved client takes the coming video frame: only
 * with nothing left from earlier ones, so it never gets half a frame */
static int __tcp_admit_frame(struct list_t *e, void *v)
{
    struct transfer_item_t *trans;
    struct connection_item_t *con;
    struct __frame_t *frame = v;
    list_upcast(trans, e);
    MUST(con = trans->con, return FAILURE);
    if (!con->trans[0].is_tcp) return SUCCESS;

    pthread_mutex_lock(&con->write_mutex);
    if (__tcp_pump(con) != SUCCESS)
        ERR("send (interleaved):%s\n", strerror(errno));
    con->tx_frame = con->tx_off == con->tx_len && frame->bytes <= RTSP_TX_BATCH - RTSP_TX_SPARE;
    pthread_mutex_unlock(&con->write_mutex);
    return SUCCESS;
}

/******************************************************************************
 *              PRIVATE FUNCTIONS
 ******************************************************************************/

static inline int __transfer_nal_h26x(struct list_head_t *trans_list, unsigned char *nalptr, size_t nalsize, char isH265)
{
    /* Checked before the header is read: an empty pack comes with no data */
    if (nalsize < 4) return SUCCESS;

    struct nal_rtp_t rtp;
    unsigned int nri = isH265 ? (nalptr[0] & 0x81) : (nalptr[0] & 0x60);
    unsigned int pt  = isH265 ? (nalptr[0] >> 1 & 0x3F) : (nalptr[0] & 0x1F);
    unsigned int ids = isH265 ? nalptr[1] : 0;
    char head = isH265 ? 3 : 2;

    rtp_hdr_t *p_header = &(rtp.packet.header);
    unsigned char *payload = rtp.packet.payload;

    p_header->version = 2;
    p_header->p = 0;
    p_header->x = 0;
    p_header->cc = 0;
    p_header->pt = 96 & 0x7F;

    if (nalsize <= __RTP_MAXPAYLOADSIZE) {
        /* single packet */
        /* SPS, PPS, SEI is not marked */
        if ((isH265 && pt < H265_NAL_TYPE_VPS) ||
            (!isH265 &&
                pt != H264_NAL_TYPE_SPS && 
                pt != H264_NAL_TYPE_PPS &&
                pt != H264_NAL_TYPE_SEI)) { 
            p_header->m = 1;
        } else {
            p_header->m = 0;
        }

        memcpy(payload, nalptr, nalsize);

        rtp.rtpsize = nalsize + sizeof(rtp_hdr_t);

        ASSERT(__rtp_send(&rtp, trans_list) == SUCCESS, return FAILURE);
    } else {
        nalptr += isH265 ? 2 : 1;
        nalsize -= isH265 ? 2 : 1;

        if (isH265) {
            payload[0] = 49 << 1;
            payload[0] |= nri;
            payload[1] = ids;
            payload[2] = pt;
        } else {
            payload[0] = 28;
            payload[0] |= nri;
            payload[1] = pt;
        }
        payload[head - 1] |= 1 << 7;

        /* send fragmented nal */
        while (nalsize > __RTP_MAXPAYLOADSIZE - head) {
            p_header->m = 0;

            memcpy(&(payload[head]), nalptr, __RTP_MAXPAYLOADSIZE - head);

            rtp.rtpsize = sizeof(rtp_hdr_t) + __RTP_MAXPAYLOADSIZE;

            nalptr += __RTP_MAXPAYLOADSIZE - head;
            nalsize -= __RTP_MAXPAYLOADSIZE - head;

            ASSERT(__rtp_send(&rtp, trans_list) == SUCCESS, return FAILURE);

            /* intended xor. blame vim :( */
            payload[head - 1] &= 0xFF ^ (1<<7); 
        }

        /* send trailing nal */
        p_header->m = 1;

        payload[head - 1] |= 1 << 6;

        /* intended xor. blame vim :( */
        payload[head - 1] &= 0xFF ^ (1<<7);

        rtp.rtpsize = nalsize + sizeof(rtp_hdr_t) + head;

        memcpy(&(payload[head]), nalptr, nalsize);

        ASSERT(__rtp_send(&rtp, trans_list) == SUCCESS, return FAILURE);
    }

    return SUCCESS;
}

static inline int __transfer_nal_mpga(struct list_head_t *trans_list, unsigned char *ptr, size_t size)
{
    struct nal_rtp_t rtp;

    rtp_hdr_t *p_header = &(rtp.packet.header);
    unsigned char *payload = rtp.packet.payload;

    p_header->version = 2;
    p_header->p = 0;
    p_header->x = 0;
    p_header->cc = 0;
    p_header->pt = 14;
    p_header->m = 1;

    payload[0] = payload[1] = payload[2] = payload[3] = 0;
    memcpy(payload + 4, ptr, size);
    size += 4;

    rtp.rtpsize = size + sizeof(rtp_hdr_t);

    ASSERT(__rtp_send(&rtp, trans_list) == SUCCESS, return FAILURE);

    return SUCCESS;
}

/* One RTP packet per 20 ms of 8 kHz G.711 (PT 8 for A-law, 0 for mu-law) */
static inline int __transfer_g711(struct list_head_t *trans_list, unsigned char *ptr, size_t size, unsigned char pt)
{
    struct nal_rtp_t rtp;
    rtp_hdr_t *p_header = &(rtp.packet.header);

    p_header->version = 2;
    p_header->p = 0;
    p_header->x = 0;
    p_header->cc = 0;
    p_header->pt = pt;
    p_header->m = 1;

    memcpy(rtp.packet.payload, ptr, size);
    rtp.rtpsize = size + sizeof(rtp_hdr_t);

    ASSERT(__rtp_send(&rtp, trans_list) == SUCCESS, return FAILURE);
    return SUCCESS;
}

static inline int __rtp_send_eachconnection(struct list_t *e, void *v)
{
    int send_bytes;
    struct connection_item_t *con;
    struct transfer_item_t *trans;
    struct nal_rtp_t *rtp = v;
    int track_id = rtp->packet.header.pt == 96 ? 0 : 1;

    list_upcast(trans,e); 

    MUST(con = trans->con, return FAILURE);
    if (!con->trans[track_id].server_port_rtp && !con->trans[track_id].is_tcp) return SUCCESS;

    rtp->packet.header.seq = htons(con->trans[track_id].rtp_seq);
    con->trans[track_id].rtp_timestamp = track_id ? __frame_ts_audio : __frame_ts_video;
    rtp->packet.header.ts = htonl(con->trans[track_id].rtp_timestamp);
    rtp->packet.header.ssrc = htonl(con->ssrc);

    if (con->trans[track_id].is_tcp) {
        /* A video frame goes in whole or not at all; audio only behind
         * nothing pending. A client that fails is shut down by the pump and
         * dropped by the RTSP thread, the others carry on */
        pthread_mutex_lock(&con->write_mutex);
        bool queued;
        if (track_id == 0) {
            queued = con->tx_frame && __tcp_queue(con, con->trans[0].channel_rtp,
                &(rtp->packet), rtp->rtpsize);
            if (con->tx_frame && !queued) con->tx_frame = false;
        } else {
            queued = __tcp_pump(con) == SUCCESS && con->tx_off == con->tx_len &&
                __tcp_queue(con, con->trans[1].channel_rtp, &(rtp->packet), rtp->rtpsize);
            if (queued) __tcp_pump(con);
        }
        pthread_mutex_unlock(&con->write_mutex);

        if (queued) {
            con->trans[track_id].rtp_seq += 1;
            con->trans[track_id].rtcp_packet_cnt += 1;
            con->trans[track_id].rtcp_octet += rtp->rtpsize;
        }
        return SUCCESS;
    } else {
        char attempts = 0;
        con->trans[track_id].rtp_seq += 1;
        do  {
            send_bytes = send(con->trans[track_id].server_rtp_fd,
                &(rtp->packet),rtp->rtpsize,0);

            if (send_bytes == rtp->rtpsize) {
                con->trans[track_id].rtcp_packet_cnt += 1;
                con->trans[track_id].rtcp_octet += rtp->rtpsize;
                return SUCCESS;
            } else if(con->con_state != __CON_S_PLAYING) {
                DBG("connection state changed before send\n");
                return SUCCESS;
            } else
                usleep(5000);
        } while (++attempts < 10 && 
            send_bytes == -1 && (errno == EAGAIN || errno == EWOULDBLOCK));
    }
    
    ERR("send:%d:%s\n", send_bytes, strerror(errno));
    return FAILURE;
}

static inline int __rtp_send(struct nal_rtp_t *rtp, struct list_head_t *trans_list)
{
    return list_map_inline(trans_list, (__rtp_send_eachconnection), rtp);
}


static inline int __rtp_setup_transfer(struct list_t *e, void *v)
{
    struct connection_item_t *con;
    struct __transfer_set_t *trans_set = v;
    struct transfer_item_t *trans;
    unsigned int timestamp_offset;
    int ret = FAILURE;

    list_upcast(con,e);

    MUST(bufpool_attach(con->pool, con) == SUCCESS,
        return FAILURE);

    if (con->con_state == __CON_S_PLAYING) {

        ASSERT(bufpool_get_free(trans_set->h->transfer_pool, &trans) == SUCCESS, ({
            ERR("transfer object resource starvation detected. possibly connection limits are wrongfully setup\n");
            goto error;}));

        MUST(bufpool_attach(con->pool, con) == SUCCESS,
            return FAILURE);

        trans->con = con;

        MUST(list_push(&trans_set->list_head, &trans->list_entry) == SUCCESS,
            goto error);

        timestamp_offset = trans_set->h->stat.ts_offset;

        con->trans[trans_set->track_id].rtp_timestamp = 
            ((unsigned int)con->trans[trans_set->track_id].rtp_timestamp + timestamp_offset);
    }

    ret = SUCCESS;

error:
    ASSERT(bufpool_detach(con->pool, con) == SUCCESS, ret = FAILURE);

    return ret;
}

static inline int __retrieve_sprop(rtsp_handle h, unsigned char *buf, size_t len)
{
    unsigned char *nalptr;
    size_t single_len;
    mime_encoded_handle base64 = NULL;
    mime_encoded_handle base16 = NULL;

    /* check VPS is set */
    if (h->isH265 && !(h->sprop_vps_b64)) {
        nalptr = buf;
        single_len = 0;
        while (nal_split(buf, &nalptr, &single_len, len) == SUCCESS) {
            if ((nalptr[0] >> 1 & 0x3F) == H265_NAL_TYPE_VPS) {
                ASSERT(single_len >= 4, return FAILURE);
                ASSERT(base64 = mime_base64_create((char *)&(nalptr[0]), single_len), return FAILURE);

                DASSERT(base64->base == 64, return FAILURE);

                /* optimistic lock */
                rtsp_lock(h);
                if (h->sprop_vps_b64) {
                    DBG("vps is set by another thread?\n");
                    mime_encoded_delete(base64);
                } else {
                    h->sprop_vps_b64 = base64;
                }
                rtsp_unlock(h);
            }
        }
        rtsp_lock(h);
        rtsp_unlock(h);
        base64 = NULL;
    }

    /* check SPS is set */
    if (!(h->sprop_sps_b64)) {
        nalptr = buf;
        single_len = 0;

        while (nal_split(buf, &nalptr, &single_len, len) == SUCCESS) {
            if ((!(h->isH265) && (nalptr[0] & 0x1F) == H264_NAL_TYPE_SPS) ||
                (h->isH265 && (nalptr[0] >> 1 & 0x3F) == H265_NAL_TYPE_SPS)) {
                ASSERT(base64 = mime_base64_create((char *)&(nalptr[0]), single_len), return FAILURE);
                ASSERT(base16 = mime_base16_create((char *)&(nalptr[1]), 3), return FAILURE);

                DASSERT(base16->base == 16, return FAILURE);
                DASSERT(base64->base == 64, return FAILURE);

                /* optimistic lock */
                rtsp_lock(h);
                if (h->sprop_sps_b64) {
                    DBG("sps is set by another thread?\n");
                    mime_encoded_delete(base64);
                } else {
                    h->sprop_sps_b64 = base64;
                }
                
                if (h->sprop_sps_b16) {
                    DBG("sps is set by another thread?\n");
                    mime_encoded_delete(base16);
                } else {
                    h->sprop_sps_b16 = base16;
                }
                rtsp_unlock(h);
            }
        }

        base64 = NULL;
        base16 = NULL;
    }

    /* check PPS is set */
    if (!(h->sprop_pps_b64)) {
        nalptr = buf;
        single_len = 0;
        while (nal_split(buf, &nalptr, &single_len, len) == SUCCESS) {
            if ((!(h->isH265) && (nalptr[0] & 0x1F) == H264_NAL_TYPE_PPS) ||
                (h->isH265 && (nalptr[0] >> 1 & 0x3F) == H265_NAL_TYPE_PPS)) {
                ASSERT(single_len >= 4, return FAILURE);
                ASSERT(base64 = mime_base64_create((char *)&(nalptr[0]), single_len), return FAILURE);

                DASSERT(base64->base == 64, return FAILURE);

                /* optimistic lock */
                rtsp_lock(h);
                if (h->sprop_pps_b64) {
                    DBG("pps is set by another thread?\n");
                    mime_encoded_delete(base64);
                } else {
                    h->sprop_pps_b64 = base64;
                }
                rtsp_unlock(h);
            }
        }
        rtsp_lock(h);
        rtsp_unlock(h);
        base64 = NULL;
    }

    return SUCCESS;
}

static inline int __rtcp_poll(struct list_t *e, void *v)
{
    struct connection_item_t *con;
    struct transfer_item_t *trans;
    int *track_id = v;

    list_upcast(trans, e);
    MUST(con = trans->con, return FAILURE);

    if ((con->trans[*track_id].rtcp_tick)-- == 0) {
        ASSERT(__rtcp_send_sr(con, *track_id) == SUCCESS, return FAILURE);

        /* postcondition check */
        DASSERT(con->trans[*track_id].rtcp_tick == 
            con->trans[*track_id].rtcp_tick_org, return FAILURE);
        DASSERT(con->trans[*track_id].rtcp_packet_cnt == 0, return FAILURE);
        DASSERT(con->trans[*track_id].rtcp_octet == 0, return FAILURE);
    }

    return SUCCESS;
}
/******************************************************************************
 *              PUBLIC FUNCTIONS
 ******************************************************************************/
void rtp_disable_audio(rtsp_handle h)
{
    h->audioPt = 255;
}

int rtp_send_h26x(rtsp_handle h, hal_vidstream *stream, char isH265)
{
    int ret = FAILURE;
    int track_id = 0;
    struct __transfer_set_t trans = {};

    /* checkout RTP packet */
    DASSERT(h, return FAILURE);

    if (gbl_get_quit(h->pool->sharedp->gbl)) {
#ifdef DEBUG_RTSP
        ERR("server threads have gone already. call rtsp_finish()\n");
#endif
        return FAILURE;
    }

    h->isH265 = isH265;
    /* Stamp with the encoder's capture time (pack timestamps are microseconds)
     * so receivers pace frames correctly even when a large keyframe makes the
     * sender deliver the following frames in a burst; fall back to the send
     * time for HALs that leave the timestamp at zero */
    __frame_ts_video = (stream->count && stream->pack[0].timestamp) ?
        (unsigned int)(stream->pack[0].timestamp * 9 / 100) : ((millis() * 90) & UINT32_MAX);

    for (int i = 0; i < stream->count; i++) {
        ASSERT(__retrieve_sprop(h, stream->pack[i].data + stream->pack[i].offset, 
            stream->pack[i].length - stream->pack[i].offset) == SUCCESS, goto error);
    }

    trans.h = h;
    trans.track_id = track_id;

    /* setup transmission object */
    rtsp_lock(h);
    ASSERT(list_map_inline(&h->con_list, (__rtp_setup_transfer), &trans) == SUCCESS, ({rtsp_unlock(h); goto error;}));
    rtsp_unlock(h);

    if (trans.list_head.list) {
        struct __frame_t frame = {};
        for (int i = 0; i < stream->count; i++) {
            unsigned char *data = stream->pack[i].data + stream->pack[i].offset;
            size_t length = stream->pack[i].length - stream->pack[i].offset;
            unsigned char *nalptr = data;
            size_t single_len = 0;
            while (nal_split(data, &nalptr, &single_len, length) == SUCCESS) {
                if (single_len < 4) continue;
                /* every RTP packet adds at most its interleave, RTP and FU heads */
                frame.bytes += single_len + (single_len / (__RTP_MAXPAYLOADSIZE - 3) + 1) *
                    (4 + sizeof(rtp_hdr_t) + 3);
            }
        }
        ASSERT(list_map_inline(&(trans.list_head), (__tcp_admit_frame), &frame) == SUCCESS, goto error);

        for (int i = 0; i < stream->count; i++) {
            unsigned char *data = stream->pack[i].data + stream->pack[i].offset;
            size_t length = stream->pack[i].length - stream->pack[i].offset;
            unsigned char *nalptr = data;
            size_t single_len = 0;
            while (nal_split(data, &nalptr, &single_len, length) == SUCCESS) {
                ASSERT(__transfer_nal_h26x(&(trans.list_head), nalptr, single_len, h->isH265) == SUCCESS, goto error);
            }
        }
        ASSERT(list_map_inline(&(trans.list_head), (__tcp_flush_each), NULL) == SUCCESS, goto error);
        ASSERT(list_map_inline(&(trans.list_head), (__rtcp_poll), &track_id) == SUCCESS, goto error);
    } 

    ret = SUCCESS;

error:
    list_destroy(&(trans.list_head));

    return ret;
}

static int __rtp_send_g711(rtsp_handle h, unsigned char *buf, size_t len, unsigned char pt)
{
    /* G.711 is one byte per sample, so the 8 kHz clock advances by exactly the
     * number of samples in the packet. Deriving this from millis() instead let
     * the stamps drift away from the audio actually sent. */
    static unsigned int g711_ts;
    __frame_ts_audio = g711_ts;
    g711_ts += (unsigned int)len;
    int ret = FAILURE;
    int track_id = 1;
    struct __transfer_set_t trans = {};

    DASSERT(h, return FAILURE);
    if (gbl_get_quit(h->pool->sharedp->gbl))
        return FAILURE;

    h->audioPt = pt;

    trans.h = h;
    trans.track_id = track_id;

    rtsp_lock(h);
    ASSERT(list_map_inline(&h->con_list, (__rtp_setup_transfer), &trans) == SUCCESS, ({rtsp_unlock(h); goto error;}));
    rtsp_unlock(h);

    if (trans.list_head.list) {
        ASSERT(__transfer_g711(&(trans.list_head), buf, len, pt) == SUCCESS, goto error);
        ASSERT(list_map_inline(&(trans.list_head), (__tcp_flush_each), NULL) == SUCCESS, goto error);
        ASSERT(list_map_inline(&(trans.list_head), (__rtcp_poll), &track_id) == SUCCESS, goto error);
    }
    ret = SUCCESS;

error:
    list_destroy(&(trans.list_head));
    return ret;
}

int rtp_send_pcma(rtsp_handle h, unsigned char *buf, size_t len)
{
    return __rtp_send_g711(h, buf, len, 8);
}

int rtp_send_pcmu(rtsp_handle h, unsigned char *buf, size_t len)
{
    return __rtp_send_g711(h, buf, len, 0);
}

int rtp_send_mp3(rtsp_handle h, unsigned char *buf, size_t len)
{
    __frame_ts_audio = (millis() * 90) & UINT32_MAX;   /* 90 kHz MPEG audio */
    int ret = FAILURE;
    int track_id = 1;
    struct __transfer_set_t trans = {};

    /* checkout RTP packet */
    DASSERT(h, return FAILURE);

    if (gbl_get_quit(h->pool->sharedp->gbl)) {
#ifdef DEBUG_RTSP
        ERR("server threads have gone already. call rtsp_finish()\n");
#endif
        return FAILURE;
    }

    h->audioPt = 14;

    trans.h = h;
    trans.track_id = track_id;

    /* setup transmission object */
    rtsp_lock(h);
    ASSERT(list_map_inline(&h->con_list, (__rtp_setup_transfer), &trans) == SUCCESS, ({rtsp_unlock(h); goto error;}));
    rtsp_unlock(h);
    
    if (trans.list_head.list) {
        ASSERT(__transfer_nal_mpga(&(trans.list_head), buf, len) == SUCCESS, goto error);
        ASSERT(list_map_inline(&(trans.list_head), (__tcp_flush_each), NULL) == SUCCESS, goto error);
        ASSERT(list_map_inline(&(trans.list_head), (__rtcp_poll), &track_id) == SUCCESS, goto error);
    } 

    ret = SUCCESS;

error:
    list_destroy(&(trans.list_head));

    return ret;
}
