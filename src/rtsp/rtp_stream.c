
#include "rtp_stream.h"
#include "thread.h" 
#include <arpa/inet.h>
#include <stdbool.h>
#include <pthread.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "list.h"
#include "rtp.h"
#include "rfc.h"


#include <time.h>

unsigned long long current_time_microseconds(void) {
    struct timespec ts;
    clock_gettime(CLOCK_REALTIME, &ts);
    return (unsigned long long)(ts.tv_sec) * 1000000 + (unsigned long long)(ts.tv_nsec) / 1000;
}

#define RTP_PACKET_SIZE 1500

typedef struct {
    char *rtp_ip;
    int rtp_port;
} rtp_thread_params;

unsigned int quit = 0;

#define MAX_BUFFER_SIZE 10*1024*1024

typedef struct
{
    unsigned char buffer[MAX_BUFFER_SIZE];
    size_t size;
    char isH265;
} buffer_t;

typedef struct
{
    buffer_t doublebuffer[2];
    int current;
    bool data_ready;
} doublebuffer_t;

doublebuffer_t g_db;

void doublebuffer_init(doublebuffer_t *db)
{
    db->doublebuffer[0].size = 0;
    db->doublebuffer[1].size = 0;
    db->current = 0;
    db->data_ready = false;
}


void doublebuffer_swap(doublebuffer_t *db)
{
    db->current = !db->current;
    db->doublebuffer[db->current].size = 0;
    db->data_ready = true;
}

buffer_t* doublebuffer_read(doublebuffer_t *db)
{
    return &db->doublebuffer[!db->current];
}

buffer_t* doublebuffer_write(doublebuffer_t *db)
{
    return &db->doublebuffer[db->current];
}

int rtp_stream_send_h26x(unsigned char *buf, size_t len, char isH265)
{
    int ret = FAILURE;

    //fprintf(stdout, "rtp_stream_send_h26x %llu\n", current_time_microseconds());

    //fprintf(stdout, "write buffer size %i\n", len);
    buffer_t* buffer = doublebuffer_write(&g_db);
    ASSERT(len < MAX_BUFFER_SIZE, return FAILURE);
    // TODO: see if we can avoid memcpy at this stage
    memcpy(buffer->buffer, buf, len);
    buffer->size = len;
    buffer->isH265 = isH265;
    doublebuffer_swap(&g_db);
    return SUCCESS;
}

void send_pkt(void* buffer, size_t size);

static inline int __rtp_send__(struct nal_rtp_t *rtp)
{
    //int send_bytes;
    //struct connection_item_t *con;
    //struct transfer_item_t *trans;
    //int track_id = rtp->packet.header.pt == 96 ? 0 : 1;
    //char attempts = 0;
    static unsigned long rtp_seq = 0;


    rtp->packet.header.seq = htons(rtp_seq);
    //if (rtp->packet.header.m)
    //    con->trans[track_id].rtp_timestamp = (millis() * 90) & UINT32_MAX;
    //rtp->packet.header.ts = htonl(con->trans[track_id].rtp_timestamp);
    rtp->packet.header.ts = htonl((millis() * 90) & UINT32_MAX);
    //rtp->packet.header.ssrc = htonl(con->ssrc);
    rtp->packet.header.ssrc = htonl(0x12345678);
    rtp_seq++;

    send_pkt(&(rtp->packet),rtp->rtpsize);
    return SUCCESS;
}

static inline int __transfer_nal_h26x_rtp(unsigned char *nalptr, size_t nalsize, char isH265)
{
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

    if (nalsize < 4) return SUCCESS;

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

        ASSERT(__rtp_send__(&rtp) == SUCCESS, return FAILURE);
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

            ASSERT(__rtp_send__(&rtp) == SUCCESS, return FAILURE);

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

        ASSERT(__rtp_send__(&rtp) == SUCCESS, return FAILURE);
    }

    return SUCCESS;
}

int g_sockfd;
struct sockaddr_in g_servaddr;

void send_pkt(void* buffer, size_t size)
{
    if (sendto(g_sockfd, buffer, size, 0, (const struct sockaddr *)&g_servaddr, sizeof(g_servaddr)) < 0) {
        perror("sendto failed");
    }
}


#define NAL_UNIT_MAX 10

// Structure pour représenter une unité NAL
typedef struct {
    uint8_t *data;
    size_t size;
} NALUnit;

#define GET_H264_NAL_UNIT_TYPE(byte) (byte & 0x1F)
#define GET_H265_NAL_UNIT_TYPE(byte) ((byte & 0x7E) >> 1)

// Fonction pour extraire les unités NAL d'un paquet H.265
void extract_nal_units(uint8_t *packet, size_t packet_size, NALUnit *nal_units, size_t *nal_count) {
    size_t i = 0;
    size_t nal_index = 0;
    int nal_type = 0;

    while (i < packet_size) {
        // Rechercher le début d'une unité NAL (0x000001 ou 0x00000001)
        if (i + 2 < packet_size && packet[i] == 0x00 && packet[i + 1] == 0x00 && packet[i + 2] == 0x01) {
            size_t nal_start = i + 3;
            i += 3;
            
            nal_type = (packet[i] & 0x7E) >> 1;
            /* if it is an IFrame or PFrame, do not search for other NAL units*/
            if ((GET_H264_NAL_UNIT_TYPE(packet[i]) == 1)||(GET_H264_NAL_UNIT_TYPE(packet[i]) == 5)||(GET_H265_NAL_UNIT_TYPE(packet[i]) == 1)||(GET_H265_NAL_UNIT_TYPE(packet[i]) == 19))
            {
                //fprintf(stdout, "IFrame or PFrame\n");
                nal_units[nal_index].data = &packet[nal_start];
                nal_units[nal_index].size = packet_size - nal_start;
                nal_index++;
                break;
            }
            else
            {
                //fprintf(stdout, "NAL type: %i\n", GET_H265_NAL_UNIT_TYPE(packet[i]));
            }

            // Rechercher la fin de l'unité NAL (le début de la prochaine unité NAL ou la fin du paquet)
            while (i + 2 < packet_size && !(packet[i] == 0x00 && packet[i + 1] == 0x00 && (packet[i + 2] == 0x01 || (i + 3 < packet_size && packet[i + 2] == 0x00 && packet[i + 3] == 0x01)))) {
                i++;
            }

            size_t nal_end = i;

            // Stocker l'unité NAL
            nal_units[nal_index].data = &packet[nal_start];
            nal_units[nal_index].size = nal_end - nal_start;
            nal_index++;

            if (nal_index == NAL_UNIT_MAX)
            {
                break;
            }
        } else {
            i++;
        }
    }

    *nal_count = nal_index;
}



void *rtp_thread_func(void *arg) {
    rtp_thread_params *params = (rtp_thread_params *)arg;
    unsigned long long curms, lastms = 0;
    unsigned long long timediff;
    unsigned long pkt = 0;

    //char buffer[RTP_PACKET_SIZE]; 
    // Create UDP socket
    if ((g_sockfd = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
        perror("socket creation failed");
        pthread_exit(NULL);
    }

    memset(&g_servaddr, 0, sizeof(g_servaddr));

    // Fill server information
    g_servaddr.sin_family = AF_INET;
    g_servaddr.sin_port = htons(params->rtp_port);
    g_servaddr.sin_addr.s_addr = inet_addr(params->rtp_ip);

    // Loop indefinitely to send RTP packets
    while (!quit) {

        if (!g_db.data_ready) {
            usleep(1);
            continue;
        }
        buffer_t* buf = doublebuffer_read(&g_db);

        curms = current_time_microseconds() / 1000;
        if (lastms)
        {
            timediff = curms - lastms;
            //if (timediff < 10 || timediff > 20)
            //    fprintf(stdout, "Time diff %llu ms\n", curms - lastms);
            //if ( buf->size > 25000)
            //{
            //    fprintf(stdout, "pkt %i, read buffer %llu ms, size %i, h265 %i\n", pkt, timediff, buf->size, buf->isH265);
           // }
            fprintf(stdout, "pkt %i, read buffer %llu ms, size %i, h265 %i\n", pkt, timediff, buf->size, buf->isH265);
        }
        lastms = curms;
        pkt++;

        
        size_t single_len = 0;

        //ASSERT(__retrieve_sprop(h, buf, len) == SUCCESS, goto error);

        //trans.h = h;
    
        /* setup transmission objecl t*/
        //ASSERT(list_map_inline(&h->con_list, (__rtp_setup_transfer), &trans) == SUCCESS, goto error);
    
        //if (trans.list_head.list) {
        //    while (__split_nal(buf, &nalptr, &single_len, len) == SUCCESS) {
        //        ASSERT(__transfer_nal_h26x(&(trans.list_head), nalptr, single_len, h->isH265) == SUCCESS, goto error);
        //    }
            //ASSERT(list_map_inline(&(trans.list_head), (__rtcp_poll), &track_id) == SUCCESS, goto error);
        //} 
        unsigned char *nalptr = buf->buffer;
    
        #if 0
        while (__split_nal(buf->buffer, &nalptr, &single_len, buf->size) == SUCCESS) {
            //fprintf(stderr, "split_nal %i\n", single_len);
            fprintf(stderr, "Buf 0x%x, size: %i, nal 0x%x, nal_size: %i\n", buf->buffer, buf->size, nalptr, single_len);
            //ASSERT(__transfer_nal_h26x_rtp(nalptr, single_len, buf->isH265) == SUCCESS, goto error);
        }
        //fprintf(stderr, "nalptr %d %d %d %d\n", buf->buffer[0], buf->buffer[1], buf->buffer[2], buf->buffer[3]);
        #endif
        

        //nalptr = &(buf->buffer[4]);
        //single_len = buf->size - 4;
        //fprintf(stderr, "Buf 0x%x, size: %i, nal 0x%x, nal_size: %i\n", buf->buffer, buf->size, nalptr, single_len);
        //ASSERT(__transfer_nal_h26x_rtp(nalptr, single_len, buf->isH265) == SUCCESS, goto error);
    

        NALUnit nal_units[NAL_UNIT_MAX];
        size_t nal_count = 0;
    
        // Extraire les unités NAL
        extract_nal_units(buf->buffer, buf->size, nal_units, &nal_count);
    
        // Envoie des unités NAL extraites
        for (size_t i = 0; i < nal_count; i++) {
            //printf("NAL Unit %zu: ", i + 1);

            //fprintf(stderr, "Buf 0x%x, size: %i, nal 0x%x, nal_size: %i\n", buf->buffer, buf->size, nal_units[i].data, nal_units[i].size);
            ASSERT(__transfer_nal_h26x_rtp(nal_units[i].data, nal_units[i].size, buf->isH265) == SUCCESS, goto error);
        }


/*
        // Prepare RTP packet (for demonstration, we use a dummy packet)
        memset(buffer, 0, RTP_PACKET_SIZE);
        buffer[0] = 0x80; // RTP version 2
        buffer[1] = 0x60; // Payload type (dynamic)
        // ... fill the rest of the RTP header and payload ...

        // Send RTP packet
        if (sendto(sockfd, buffer, RTP_PACKET_SIZE, 0, (const struct sockaddr *)&servaddr, sizeof(servaddr)) < 0) {
            perror("sendto failed");
            break;
        }

        fprintf(stdout, "Sent RTP packet\n");

        // Sleep for a while to simulate packet interval (e.g., 20ms for 50fps)
        usleep(20000);
*/
        // TODO: check race conditions
        g_db.data_ready = false;
    }

error:
    close(g_sockfd);
    pthread_exit(NULL);
}


void rtp_create(const char *rtp_ip, unsigned int rtp_port)
{
    doublebuffer_init(&g_db);

    pthread_t rtp_thread;
    rtp_thread_params *params = malloc(sizeof(rtp_thread_params));
    if (!params) {
        perror("malloc failed");
        return;
    }

    params->rtp_ip = strdup(rtp_ip);
    params->rtp_port = rtp_port;

    if (pthread_create(&rtp_thread, NULL, rtp_thread_func, params) != 0) {
        perror("pthread_create failed");
        free(params->rtp_ip);
        free(params);
        return;
    }

    // Optionally, detach the thread if you don't need to join it later
    pthread_detach(rtp_thread);
}

void rtp_finish(void)
{
    quit = 1;
}