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
static inline int __rtp_send_h26x(struct nal_rtp_t *rtp, struct list_head_t *trans_list);
static inline int __rtp_send_eachconnection_h26x(struct list_t *e, void *v);
static inline int __rtp_setup_transfer(struct list_t *e, void *v);
static inline int __transfer_nal_h26x(struct list_head_t *trans_list, unsigned char *nalptr, size_t nalsize, char isH265);
static inline int __retrieve_sprop(rtsp_handle h, unsigned char *buf, size_t len);

struct __transfer_set_t {
    struct list_head_t list_head;
    rtsp_handle h;
};

/******************************************************************************
 *              PRIVATE FUNCTIONS
 ******************************************************************************/

static inline int __transfer_nal_h26x(struct list_head_t *trans_list, unsigned char *nalptr, size_t nalsize, char isH265)
{
    struct nal_rtp_t rtp;
    unsigned int nri = isH265 ? (nalptr[0] & 0x81) : (nalptr[0] & 0x60);
    unsigned int pt  = isH265 ? (nalptr[0] & 0x7E >> 1) : (nalptr[0] & 0x1F);
    unsigned int ids = isH265 ? nalptr[1] : 0;
    char head = isH265 ? 3 : 2;

    rtp_hdr_t *p_header = &(rtp.packet.header);
    unsigned char *payload = rtp.packet.payload;

    p_header->version = 2;
    p_header->p = 0;
    p_header->x = 0;
    p_header->cc = 0;
    p_header->pt = 96 & 0x7F;

    if (nalsize < 4) return SUCCESS;

    if (nalsize <= __RTP_MAXPAYLOADSIZE){
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

        ASSERT(__rtp_send_h26x(&rtp,trans_list) == SUCCESS, return FAILURE);
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
        while(nalsize > __RTP_MAXPAYLOADSIZE - head){
            p_header->m = 0;

            memcpy(&(payload[head]), nalptr, __RTP_MAXPAYLOADSIZE - head);

            rtp.rtpsize = sizeof(rtp_hdr_t) + __RTP_MAXPAYLOADSIZE;

            nalptr += __RTP_MAXPAYLOADSIZE - head;
            nalsize -= __RTP_MAXPAYLOADSIZE - head;

            ASSERT(__rtp_send_h26x(&rtp,trans_list) == SUCCESS, return FAILURE);

            /* intended xor. blame vim :( */
            payload[head - 1] &= 0xFF ^ (1<<7); 
        }

        /* send trailing nal */
        p_header->m = 1;

        payload[head - 1] |= 1 << 6;

        /* intended xor. blame vim :( */
        payload[head - 1] &= 0xFF ^ (1<<7);

        rtp.rtpsize = nalsize + sizeof(rtp_hdr_t);

        memcpy(&(payload[head]), nalptr, nalsize);

        ASSERT(__rtp_send_h26x(&rtp, trans_list) == SUCCESS, return FAILURE);
    }

    return SUCCESS;
}

static inline int __rtp_send_eachconnection_h26x(struct list_t *e, void *v)
{
    int send_bytes;
    struct connection_item_t *con;
    struct transfer_item_t *trans;
    struct nal_rtp_t *rtp = v;

    list_upcast(trans,e); 

    MUST(con = trans->con, return FAILURE);

    rtp->packet.header.seq = htons(con->rtp_seq);
    rtp->packet.header.ts = htonl(con->rtp_timestamp);
    rtp->packet.header.ssrc = htonl(con->ssrc);
    con->rtp_seq += 1;

    send_bytes = send(con->server_rtp_fd,&(rtp->packet),rtp->rtpsize,0);
    
    if(send_bytes == rtp->rtpsize) {
        con->rtcp_packet_cnt += 1;
        con->rtcp_octet += rtp->rtpsize;
        return SUCCESS;
    } 

    if(con->con_state != __CON_S_PLAYING) {
        DBG("connection state changed before send\n");
        return SUCCESS;
    }

    if(send_bytes == -1 && (errno == EAGAIN || errno == EWOULDBLOCK)){
        ERR("EAGAIN\n");
        return FAILURE;
    } 
    
    ERR("send:%d:%s\n",send_bytes,strerror(errno));
    return FAILURE;
}

static inline int __rtp_send_h26x(struct nal_rtp_t *rtp, struct list_head_t *trans_list)
{
    return list_map_inline(trans_list,(__rtp_send_eachconnection_h26x), rtp);
}


static inline int __rtp_setup_transfer(struct list_t *e, void *v)
{
    struct connection_item_t *con;
    struct __transfer_set_t *trans_set = v;
    struct transfer_item_t *trans;
    unsigned int  timestamp_offset;
    int ret = FAILURE;

    list_upcast(con,e);

    MUST(bufpool_attach(con->pool, con) == SUCCESS,
        return FAILURE);

    if(con->con_state == __CON_S_PLAYING) {

        ASSERT(bufpool_get_free(trans_set->h->transfer_pool, &trans) == SUCCESS, ({
            ERR("transfer object resouce starvation detected. possibly connection limits are wrongfully setup\n");
            goto error;}));

        MUST(bufpool_attach(con->pool, con) == SUCCESS,
            return FAILURE);

        trans->con = con;

        MUST(list_push(&trans_set->list_head,&trans->list_entry) == SUCCESS,
            goto error);

        timestamp_offset = trans_set->h->stat.ts_offset;

        con->rtp_timestamp = ((unsigned int)con->rtp_timestamp + timestamp_offset);
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
    if(h->isH265 && !(h->sprop_vps_b64)){
        nalptr = buf;
        single_len = 0;
        while (__split_nal(buf,&nalptr,&single_len,len) == SUCCESS) {
            if (nalptr[0] & 0x7E >> 1 == H265_NAL_TYPE_VPS) {
                ASSERT(base64 = mime_base64_create((char *)&(nalptr[0]),single_len), return FAILURE);

                DASSERT(base64->base == 64, return FAILURE);

                /* optimistic lock */
                rtsp_lock(h);
                if(h->sprop_vps_b64) {
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
    if(!(h->sprop_sps_b64)){ 
        nalptr = buf;
        single_len = 0;

        while (__split_nal(buf,&nalptr,&single_len,len) == SUCCESS) {
            if ((!(h->isH265) && nalptr[0] & 0x1F == H264_NAL_TYPE_SPS) ||
                (h->isH265 && nalptr[0] & 0x7E >> 1 == H265_NAL_TYPE_SPS)) {
                ASSERT(base64 = mime_base64_create((char *)&(nalptr[0]),single_len), return FAILURE);
                ASSERT(base16 = mime_base16_create((char *)&(nalptr[1]),3), return FAILURE);

                DASSERT(base16->base == 16, return FAILURE);
                DASSERT(base64->base == 64, return FAILURE);

                /* optimistic lock */
                rtsp_lock(h);
                if(h->sprop_sps_b64) {
                    DBG("sps is set by another thread?\n");
                    mime_encoded_delete(base64);
                } else {
                    h->sprop_sps_b64 = base64;
                }
                
                if(h->sprop_sps_b16) {
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
    if(!(h->sprop_pps_b64)){
        nalptr = buf;
        single_len = 0;
        while (__split_nal(buf,&nalptr,&single_len,len) == SUCCESS) {
            if ((!(h->isH265) && nalptr[0] & 0x1F == H264_NAL_TYPE_PPS) ||
                (h->isH265 && nalptr[0] & 0x7E >> 1 == H265_NAL_TYPE_PPS)) {
                ASSERT(single_len >= 4, return FAILURE);
                ASSERT(base64 = mime_base64_create((char *)&(nalptr[0]),single_len), return FAILURE);

                DASSERT(base64->base == 64, return FAILURE);

                /* optimistic lock */
                rtsp_lock(h);
                if(h->sprop_pps_b64) {
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

    list_upcast(trans, e);
    MUST(con = trans->con, return FAILURE);
    
    if((con->rtcp_tick)-- == 0) {
        ASSERT(__rtcp_send_sr(con) == SUCCESS, return FAILURE);

        /* postcondition check */
        DASSERT(con->rtcp_tick == con->rtcp_tick_org, return FAILURE);
        DASSERT(con->rtcp_packet_cnt == 0, return FAILURE);
        DASSERT(con->rtcp_octet == 0, return FAILURE);
    }
    return SUCCESS;
}
/******************************************************************************
 *              PUBLIC FUNCTIONS
 ******************************************************************************/
int rtp_send_h26x(rtsp_handle h, unsigned char *buf, size_t len, struct timeval *p_tv, char isH265)
{
    unsigned char *nalptr = buf;
    size_t single_len = 0;
    int ret = FAILURE;
    struct __transfer_set_t trans = {};

    /* checkout RTP packet */
    DASSERT(h, return FAILURE);
    DASSERT(p_tv, return FAILURE);

    if(gbl_get_quit(h->pool->sharedp->gbl)) {
        ERR("server threads have gone already. call rtsp_finish()\n");
        return FAILURE;
    }
    
    __get_timestamp_offset(&h->stat, p_tv);

    h->isH265 = isH265;

    ASSERT(__retrieve_sprop(h,buf,len) == SUCCESS, goto error);

    trans.h = h;

    /* setup transmission objecl t*/
    ASSERT(list_map_inline(&h->con_list,(__rtp_setup_transfer),&trans) == SUCCESS, goto error);
    
    if(trans.list_head.list) {

        while (__split_nal(buf,&nalptr,&single_len,len) == SUCCESS) {
            
            ASSERT(__transfer_nal_h26x(&(trans.list_head),nalptr,single_len,h->isH265) == SUCCESS, goto error);

        }

        ASSERT(list_map_inline(&(trans.list_head),(__rtcp_poll), NULL) == SUCCESS, goto error);
    } 

    ret = SUCCESS;

error:
    list_destroy(&(trans.list_head));

    return ret;
}