#ifndef _RTSP_PRIV_H
#define _RTSP_PRIV_H

#include <sys/socket.h>
#include <sys/types.h>
#include <arpa/inet.h>

#include "rtsp_server.h"

#include "common.h"
#include "rfc.h"
#include "list.h"
#include "thread.h"
#include "bufpool.h"
#include "mime.h"

/******************************************************************************
 *              DEFINITIONS
 ******************************************************************************/
#define __RTSP_TCP_BUF_SIZE 4096
#define __CONNECTION_QUEUE_SIZE 16

#define __TERM  "\r\n"
#define SCMP(id,s) (strncasecmp(id,s,strlen(id)) == 0)
#define STR_KEY_NUM(buf, key, num) do {  \
		char *__point = NULL; \
		if ((__point=strstr(buf, key))) { \
			num = strtoul(__point+strlen(key), NULL, 10);  \
		} else { \
			num = 0; \
		} \
}while(0)

enum __connection_state_e {
    __CON_S_INIT = 0,
    __CON_S_READY,
    __CON_S_PLAYING,
    __CON_S_RECORDING,
    __CON_S_DISCONNECTED,
    __CON_S_COUNT
};

enum __parser_state_e {
    __PARSER_S_INIT = 0,
    __PARSER_S_HEAD,
    __PARSER_S_CSEQ,
    __PARSER_S_TRANSPORT,
    __PARSER_S_SESSION,
    __PARSER_S_RANGE,
    __PARSER_S_ERROR,
    __PARSER_S_COUNT
};

enum __method_e {
    __METHOD_OPTIONS,
    __METHOD_DESCRIBE,
    __METHOD_SETUP,
    __METHOD_PLAY,
    __METHOD_TEARDOWN,
    __METHOD_PAUSE,
    __METHOD_RECORDING,
    __METHOD_NONE,
    __METHOD_COUNT
};

/******************************************************************************
 *              DATA STRUCTURES
 ******************************************************************************/
struct __time_stat_t {
    struct timeval prev_tv;
    unsigned long long avg;
    unsigned long long total_cnt;
    unsigned long long jitter_mask;
    unsigned int cnt;
    unsigned int ts_offset;
};

struct connection_item_t {
    struct sockaddr_in addr;
    FILE *fp_tcp_read;
    FILE *fp_tcp_write;
    int client_fd;
    int track_id;
    int cseq;

    struct {
        int server_rtcp_fd;
        int server_rtp_fd;
        unsigned int client_port_rtp;
        unsigned int client_port_rtcp;
        unsigned int server_port_rtp;
        unsigned int server_port_rtcp;
        unsigned int range_start;
        unsigned int range_end;
        unsigned int rtcp_octet;
        unsigned int rtcp_packet_cnt;
        int rtcp_tick;
        int rtcp_tick_org;
        unsigned short rtp_seq;
        unsigned int rtp_timestamp;
    } trans[2];

    enum __connection_state_e con_state;
    enum __parser_state_e parser_state;
    enum __method_e method;
    bufpool_handle pool;
    unsigned long long session_id;
    unsigned long long given_session_id;
    unsigned int ssrc;
    struct list_t list_entry;
};

struct transfer_item_t {
    struct list_t list_entry;
    struct connection_item_t *con;
    bufpool_handle pool;
};

struct __rtsp_obj_t {
    pthread_mutex_t mutex;
    struct list_head_t con_list;
    threadpool_handle pool;
    bufpool_handle con_pool;
    bufpool_handle transfer_pool;
    unsigned short  port;
    struct __time_stat_t stat;
    char isH265;
    char audioPt;
    mime_encoded_handle sprop_vps_b64;
    mime_encoded_handle sprop_sps_b64;
    mime_encoded_handle sprop_pps_b64;
    mime_encoded_handle sprop_sps_b16;
    unsigned        ctx; /* for rand_r */
    int             con_num;
    unsigned char   max_con;
    int             priority; 
};

struct sock_select_t {
    struct timeval timeout;
    fd_set rfds;
    fd_set wfds;
    fd_set efds;
    int nfds;
    rtsp_handle h_rtsp;
};

/******************************************************************************
 *              FUNCTION DECLARATIONS
 ******************************************************************************/
static inline void rtsp_lock(rtsp_handle h);
static inline void rtsp_unlock(rtsp_handle h);
static inline int __read_line(struct connection_item_t *p, char *buf);
static inline int __transfer_item_cleaner(struct list_t *e);

/******************************************************************************
 *              INLINE FUNCTIONS
 ******************************************************************************/
static inline void rtsp_lock(rtsp_handle h)
{
    pthread_mutex_lock(&h->mutex);
}

static inline void rtsp_unlock(rtsp_handle h)
{
    pthread_mutex_unlock(&h->mutex);
}

static inline int __read_line(struct connection_item_t *p, char *buf)
{
    /* we set the socket to non-blocking */
    if(fgets(buf,__RTSP_TCP_BUF_SIZE,p->fp_tcp_read) == NULL) 
	{
        /* unexpected end. we do not expect it */
        if(p->parser_state == __PARSER_S_INIT) {
            /* when this selected sd is EOF at first glance, it's dead */
            DBG("disconnected\n");
            
        } else {
            /* corrupted message. nothing to be done */
            ERR("message end before delimiter\n");
        }

        p->con_state = __CON_S_DISCONNECTED;
        ASSERT(bufpool_detach(p->pool,p) == SUCCESS, ERR("connection detach failed\n"));
        return FALSE;
    }

    DBG(">%s",buf);

    /* check end of request */
    return !(SCMP(__TERM,buf));
}

static inline unsigned long long __get_random_byte(unsigned *ctx)
{
    return (unsigned long long)(((int)(random())*(*ctx)) % 256);
}

static inline unsigned long long __get_random_llu(unsigned *ctx)
{
    return 0
        | __get_random_byte(ctx)
        | (__get_random_byte(ctx)) << 8 
        | (__get_random_byte(ctx)) << 16 
        | (__get_random_byte(ctx)) << 24
        | (__get_random_byte(ctx)) << 32
        | (__get_random_byte(ctx)) << 40
        | (__get_random_byte(ctx)) << 48
        | (__get_random_byte(ctx)) << 56;
}

static inline int __transfer_item_cleaner(struct list_t *e)
{
    struct transfer_item_t *p;
    list_upcast(p,e);

    if(p->con) {
        ASSERT(bufpool_detach(p->con->pool,p->con) == SUCCESS,
            return FAILURE);
        p->con = NULL;
    }

    ASSERT(bufpool_detach(p->pool,p) == SUCCESS,
        return FAILURE);

    return SUCCESS;
}

static inline int __get_timestamp_offset(struct __time_stat_t *p_stat, struct timeval *p_tv)
{
    unsigned long long  kts;

    if(p_stat->prev_tv.tv_sec == 0) {
        p_stat->prev_tv = *p_tv;
        p_stat->total_cnt = 0;
        p_stat->cnt = 0;
        p_stat->avg = 0;
        p_stat->jitter_mask = 0;
        return SUCCESS;
    }

    /* we fix time stamp offset in 5 years of running on 30fps.. */
    if(p_stat->total_cnt < 0xFFFFFFFFLLU) {


        kts = ((p_tv->tv_sec - p_stat->prev_tv.tv_sec) * 1000000 + (p_tv->tv_usec - p_stat->prev_tv.tv_usec)) * 90;

        p_stat->avg = ((p_stat->avg * p_stat->total_cnt) + kts * 1000) / (p_stat->total_cnt + 1);
        p_stat->prev_tv = *p_tv;
        p_stat->total_cnt += 1;
    }

    p_stat->jitter_mask += p_stat->avg % 1000000;
    p_stat->ts_offset = (p_stat->avg / 1000000);

    if(p_stat->jitter_mask > 1000000) {
        p_stat->ts_offset += 1;
        p_stat->jitter_mask -= 1000000;
    }

    return SUCCESS;
}

#endif