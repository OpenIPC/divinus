
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

#include "timestamp.h"

#include <time.h>



#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define SEI_PAYLOAD_TYPE_USER_DATA_UNREGISTERED 5

typedef struct {
    uint8_t payloadType;
    uint8_t payloadSize;
    uint8_t payload[256];
} sei_message_t;

static void create_sei_message(sei_message_t *sei, uint32_t frame_number) {
    sei->payloadType = SEI_PAYLOAD_TYPE_USER_DATA_UNREGISTERED;
    sei->payloadSize = sizeof(frame_number);
    frame_number = htonl(frame_number);
    memcpy(sei->payload, &frame_number, sizeof(frame_number));
}

static void format_sei_nalu_h265(uint8_t *sei_nalu, sei_message_t *sei, size_t *sei_nalu_size) {
    uint8_t sei_header[] = {0x4E}; // NALU header for SEI
    size_t sei_header_size = sizeof(sei_header);
    size_t sei_message_size = 2 + sei->payloadSize; // payloadType + payloadSize + payload

    // Copier le header SEI
    memcpy(sei_nalu, sei_header, sei_header_size);
    sei_nalu[sei_header_size] = sei->payloadType;
    sei_nalu[sei_header_size + 1] = sei->payloadSize;
    memcpy(sei_nalu + sei_header_size + 2, sei->payload, sei->payloadSize);

    *sei_nalu_size = sei_header_size + sei_message_size;
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
    static unsigned long rtp_seq = 0;


    rtp->packet.header.seq = htons(rtp_seq);
    rtp->packet.header.ts = htonl((millis() * 90) & UINT32_MAX);
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
                HAL_DEBUG("RTP",  "IFrame or PFrame\n");
                nal_units[nal_index].data = &packet[nal_start];
                nal_units[nal_index].size = packet_size - nal_start;
                nal_index++;
                break;
            }
            else
            {
                HAL_DEBUG("RTP",  "NAL type: %i\n", GET_H265_NAL_UNIT_TYPE(packet[i]));
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
       
        size_t single_len = 0;


        unsigned char *nalptr = buf->buffer;


        NALUnit nal_units[NAL_UNIT_MAX];
        size_t nal_count = 0;

        // Extraire les unités NAL
        extract_nal_units(buf->buffer, buf->size, nal_units, &nal_count);
        
        // Create sei message and send to ground
        static unsigned long frameNb = 0;
        sei_message_t sei;
        create_sei_message(&sei, frameNb);

        uint8_t sei_nalu[256];
        size_t sei_nalu_size;
        format_sei_nalu_h265(sei_nalu, &sei, &sei_nalu_size);
        // TODO : do it also    for h264
        ASSERT(__transfer_nal_h26x_rtp(sei_nalu, sei_nalu_size, buf->isH265) == SUCCESS, goto error);

        // Envoie des unités NAL extraites
        for (size_t i = 0; i < nal_count; i++) {
            
            ASSERT(__transfer_nal_h26x_rtp(nal_units[i].data, nal_units[i].size, buf->isH265) == SUCCESS, goto error);
        }

        // send timestamps to ground
        timestamp_send_finished(frameNb);
        frameNb++;

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