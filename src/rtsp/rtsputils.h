#pragma once

#include "rtspdefines.h"

#include <ctype.h>
#include <math.h>
#include <stdio.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <time.h>

#define RTSP_BUFFERSIZE  4096
#define MAX_DESCR_LENGTH 4096

#define RTSP_VER "RTSP/1.0"

#define RTSP_EL "\r\n"

#define PACKAGE "sunshine"
#define VERSION "1.11"

typedef struct {
    int RTP;
    int RTCP;
} rtpPortPair;

typedef struct _rtpTransport {
    rtpType type;
    int rtpFd;
    union {
        struct {
            rtpPortPair cliPorts;
            rtpPortPair serPorts;
            unsigned char isMulticast;
        } udp;
        struct {
            rtpPortPair interleaved;
        } tcp;
        // other trasports here
    } u;
} rtpTransport;

typedef struct _rtpSession {
    struct _tagStRtpHandle *rtpHandle;
    rtpTransport transport;
    unsigned char pause;
    unsigned char started;
    int schedId;
    struct _rtpSession *next;
} rtpSession;

typedef struct _rtspSession {
    int cur_state;
    int session_id;

    rtpSession *rtpSession;

    struct _rtspSession *next;
} rtspSession;

typedef struct _rtspBuffer {
    int fd;
    unsigned int port;

    struct sockaddr stClientAddr;

    char in_buffer[RTSP_BUFFERSIZE];
    unsigned int in_size;
    char out_buffer[RTSP_BUFFERSIZE + MAX_DESCR_LENGTH];
    int out_size;

    unsigned int rtsp_cseq;
    char descr[MAX_DESCR_LENGTH];
    rtspSession *session_list;
    struct _rtspBuffer *next;
} rtspBuffer;

char *sock_ntop_host(
    const struct sockaddr *sa, socklen_t salen, char *str, size_t len);
int tcp_accept(int fd);
int tcp_connect(unsigned short port, char *addr);
int tcp_listen(unsigned short port);
int tcp_read(int fd, void *buffer, int nbytes, struct sockaddr *Addr);
int tcp_send(int fd, void *dataBuf, unsigned int dataSize);
int tcp_write(int fd, char *buffer, int nbytes);

#define MAX_CONNECTION 10

typedef struct _play_args {
    struct tm playback_time;
    short playback_time_valid;
    float start_time;
    short start_time_valid;
    float end_time;
} playArgs;

typedef unsigned int (*rtpPlayAct)(unsigned int rtp, char *data, int size,
    unsigned int tstamp);

typedef struct _rtspSchedList {
    int valid;
    int BeginFrame;
    rtpSession *session;
    rtpPlayAct playAction;
} rtspSchedList;

void rtsp_deinit_schedule();
int rtsp_init_schedule();
int schedule_add(rtpSession *session);
int schedule_start(int id, playArgs *args);
void schedule_stop(int id);
int schedule_remove(int id);
int schedule_resume(int id, playArgs *args);

typedef enum {
    /*sender report,for transmission and reception statics from participants
       that are active senders*/
    SR = 200,
    /*receiver report,for reception statistics from participants that are not
       active senders and in combination with SR for    active senders reporting
       on more than 31 sources
     */
    RR = 201,
    SDES = 202, /*Source description items, including CNAME,NAME,EMAIL,etc*/
    BYE = 203,  /*Indicates end of participation*/
    APP = 204   /*Application-specific functions*/
} rtcp_pkt_type;

#define SERVER_RTSP_PORT_DEFAULT 554

typedef struct rtpPlayAct {
    char hostname[256];
    char serv_root[256];
    char log[256];
    unsigned int port;
    unsigned int max_session;
} rtspServPrefs;

int send_reply(int err, char *addon, rtspBuffer *rtsp);
int bwrite(char *buffer, unsigned short len, rtspBuffer *rtsp);
const char *get_stat(int err);