
#include "rtp_stream.h"
#include "thread.h" 
#include <arpa/inet.h>
#include <stdbool.h>
#include <pthread.h>
#include <unistd.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <sys/socket.h>
#include <sys/un.h>

#include "list.h"
#include "rtp.h"
#include "rfc.h"

#include "timestamp.h"



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


unsigned int g_naluSize;


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

    if (nalsize <= g_naluSize) {
        
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
        payload[head - 1] |= 1 << 7; // start bit

        /* send fragmented nal */
        while (nalsize > g_naluSize - head) {
            p_header->m = 0;

            memcpy(&(payload[head]), nalptr, g_naluSize - head);

            rtp.rtpsize = sizeof(rtp_hdr_t) + g_naluSize;
            nalptr += g_naluSize - head;
            nalsize -= g_naluSize - head;
            ASSERT(__rtp_send__(&rtp) == SUCCESS, return FAILURE);

            /* intended xor. blame vim :( */
            payload[head - 1] &= 0xFF ^ (1<<7); // Clear Start bit
        }

        /* send trailing nal */
        p_header->m = 1;

        payload[head - 1] |= 1 << 6; // end bit

        /* intended xor. blame vim :( */
        payload[head - 1] &= 0xFF ^ (1<<7); // Clear Start bit

        rtp.rtpsize = nalsize + sizeof(rtp_hdr_t) + head;
        memcpy(&(payload[head]), nalptr, nalsize);

        ASSERT(__rtp_send__(&rtp) == SUCCESS, return FAILURE);
    }

    return SUCCESS;
}

static int g_sockfd;
struct sockaddr_un g_servaddr;
size_t g_addr_len;

void send_pkt(void* buffer, size_t size)
{
    if (sendto(g_sockfd, buffer, size, 0, (struct sockaddr *)&g_servaddr, g_addr_len) < 0) {
        perror("sendto failed");
    }
}


void rtp_init(const char *socket_path, unsigned int naluSize)
{
    g_naluSize = naluSize;

    // Créer le socket Unix
    if ((g_sockfd = socket(AF_UNIX, SOCK_DGRAM, 0)) < 0) {
        perror("socket creation failed");
        exit(EXIT_FAILURE);
    }

    memset(&g_servaddr, 0, sizeof(g_servaddr));

    // Configurer l'adresse du socket Unix
    g_servaddr.sun_family = AF_UNIX;
    g_servaddr.sun_path[0] = '\0'; // Socket abstrait
    strncpy(g_servaddr.sun_path + 1, socket_path, sizeof(g_servaddr.sun_path) - 2);

    // Créer le socket Unix
    if ((g_sockfd = socket(AF_UNIX, SOCK_DGRAM, 0)) < 0) {
        perror("socket creation failed");
        pthread_exit(NULL);
    }

    g_addr_len = sizeof(sa_family_t) + strlen(socket_path) + 1;

    HAL_INFO("RTP", "Connected to Unix socket: %s\n", socket_path);

    int buf_size = 1024 * 1024; // 1 MB
    if (setsockopt(g_sockfd, SOL_SOCKET, SO_SNDBUF, &buf_size, sizeof(buf_size)) < 0) {
        perror("setsockopt SO_SNDBUF failed");
    }
}

void rtp_deinit(void)
{
    HAL_INFO("RTP", "Deinitializing RTP socket\n");
    shutdown(g_sockfd, SHUT_RDWR);
    if (close(g_sockfd) < 0) {
        perror("close failed");
    }

    // Vérifiez si le socket est abstrait avant d'appeler unlink
    if (g_servaddr.sun_path[0] != '@') {
        HAL_INFO("RTP", "Unlinking socket file: %s\n", g_servaddr.sun_path);
        if (unlink(g_servaddr.sun_path) < 0) {
            perror("unlink failed");
        }
    } else {
        HAL_INFO("RTP", "Socket is abstract, no unlink needed\n");
    }
}


void rtp_send_frame_h26x(unsigned long nbNal, NALUnit_t* nals, bool isH265)
{
    // Create sei message and send to ground
    static unsigned long frameNb = 0;
    sei_message_t sei;
    create_sei_message(&sei, frameNb);

    uint8_t sei_nalu[256];
    size_t sei_nalu_size;
    format_sei_nalu_h265(sei_nalu, &sei, &sei_nalu_size);
    // TODO : do it also for h264
    __transfer_nal_h26x_rtp(sei_nalu, sei_nalu_size, isH265);

    // Envoie des unités NAL 
    for (size_t i = 0; i < nbNal; i++) {
        
        /* do not set NAL header */
        __transfer_nal_h26x_rtp(nals[i].data + 4, nals[i].size - 4, isH265);
    }

    // send timestamps to ground
    timestamp_send_finished(frameNb);
    frameNb++;
    
}


