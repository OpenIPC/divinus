#include "rtputils.h"

#include <errno.h>
#include <net/if.h>
#include <netdb.h>
#include <netinet/in.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>

#include "rtspservice.h"
#include "rtsputils.h"

typedef struct {
    /**/                                   /* byte 0 */
    unsigned char u4CSrcLen : 4; /**/    /* expect 0 */
    unsigned char u1Externsion : 1; /**/ /* expect 1, see RTP_OP below */
    unsigned char u1Padding : 1; /**/    /* expect 0 */
    unsigned char u2Version : 2; /**/    /* expect 2 */
    /**/                                   /* byte 1 */
    unsigned char u7Payload : 7; /**/    /* RTP_PAYLOAD_RTSP */
    unsigned char u1Marker : 1; /**/     /* expect 1 */
    /**/                                   /* bytes 2, 3 */
    unsigned short u16SeqNum;
    /**/ /* bytes 4-7 */
    unsigned long long u32TimeStamp;
    /**/                          /* bytes 8-11 */
    unsigned long u32SSrc; /**/ /* stream number is used here. */
} StRtpFixedHdr;

typedef struct {
    unsigned char u5Type : 5;
    unsigned char u2Nri : 2;
    unsigned char u1F : 1;
} StNaluHdr;

typedef struct {
    unsigned char u5Type : 5;
    unsigned char u2Nri : 2;
    unsigned char u1F : 1;
} StFuIndicator;

typedef struct {
    unsigned char u5Type : 5;
    unsigned char u1R : 1;
    unsigned char u1E : 1;
    unsigned char u1S : 1;
} StFuHdr;

typedef struct _tagStRtpHandle {
    int s32Sock;
    struct sockaddr_in stServAddr;
    unsigned short u16SeqNum;
    unsigned long long u32TimeStampInc;
    unsigned long long u32TimeStampCurr;
    unsigned long long u32CurrTime;
    unsigned long long u32PrevTime;
    unsigned int u32SSrc;
    StRtpFixedHdr *pRtpFixedHdr;
    StNaluHdr *pNaluHdr;
    StFuIndicator *pFuInd;
    StFuHdr *pFuHdr;
    rtpPayload emPayload;

} StRtpObj, *rtpHandle;

unsigned int rtp_create(unsigned int ip, int port, rtpPayload payload) {
    rtpHandle handle = NULL;
    struct timeval stTimeval;
    struct ifreq stIfr;
    int s32Broadcast = 1;

    handle = (rtpHandle)calloc(1, sizeof(StRtpObj));
    if (!hRtp) {
        printf("Failed to create RTP handle\n");
        goto cleanup;
    }

    handle->s32Sock = -1;
    if ((handle->s32Sock = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
        printf("Failed to create socket\n");
        goto cleanup;
    }

    if (0xFF000000 == (ip & 0xFF000000)) {
        if (-1 == setsockopt(
                      handle->s32Sock, SOL_SOCKET, SO_BROADCAST,
                      (char *)&s32Broadcast, sizeof(s32Broadcast))) {
            printf("Failed to set socket\n");
            goto cleanup;
        }
    }

    handle->stServAddr.sin_family = AF_INET;
    handle->stServAddr.sin_port = htons(port);
    handle->stServAddr.sin_addr.s_addr = ip;
    bzero(&(handle->stServAddr.sin_zero), 8);

    handle->u16SeqNum = 0;
    handle->u32TimeStampInc = 0;
    handle->u32TimeStampCurr = 0;

    if (gettimeofday(&stTimeval, NULL) == -1) {
        printf("Failed to get os time\n");
        goto cleanup;
    }

    handle->u32PrevTime = stTimeval.tv_sec * 1000 + stTimeval.tv_usec / 1000;

    handle->emPayload = payload;

    //获取本机网络设备名
    strcpy(stIfr.ifr_name, "eth0");
    if (ioctl(handle->s32Sock, SIOCGIFADDR, &stIfr) < 0) {
        // printf("Failed to get host ip\n");
        strcpy(stIfr.ifr_name, "wlan0");
        if (ioctl(handle->s32Sock, SIOCGIFADDR, &stIfr) < 0) {
            printf("Failed to get host eth0 or wlan0 ip\n");
            goto cleanup;
        }
    }

    handle->u32SSrc =
        htonl(((struct sockaddr_in *)(&stIfr.ifr_addr))->sin_addr.s_addr);

    printf("<><><><>success creat RTP<><><><>\n");

    return (unsigned int)handle;

cleanup:
    if (handle) {
        if (hRtp->s32Sock >= 0) {
            close(hRtp->s32Sock);
        }

        free(handle);
    }

    return 0;
}

void rtp_delete(unsigned int rtp) {
    rtpHandle handle = (rtpHandle)rtp;

    if (handle) {
        if (handle->s32Sock >= 0) {
            close(handle->s32Sock);
        }

        free(handle);
    }
}

static int rtp_send_naluh264(rtpHandle handle, char *nalBuf, int nalSize) {
    char *naluPayload;
    char *sendBuf;
    int s32Bytes = 0;
    int ret = 0;
    struct timeval stTimeval;
    char *pNaluCurr;
    int s32NaluRemain;
    unsigned char u8NaluBytes;

    sendBuf = (char*)calloc(MAX_RTP_PKT_LENGTH + 100, sizeof(char));
    if (!sendBuf) {
        ret = -1;
        goto cleanup;
    }

    handle->pRtpFixedHdr = (StRtpFixedHdr *)sendBuf;
    handle->pRtpFixedHdr->u2Version = 2;
    handle->pRtpFixedHdr->u1Marker = 0;
    handle->pRtpFixedHdr->u7Payload = H264;

    handle->pRtpFixedHdr->u32TimeStamp =
        htonl(handle->u32TimeStampCurr * (90000 / 1000));

    handle->pRtpFixedHdr->u32SSrc = handle->u32SSrc;

    if (gettimeofday(&stTimeval, NULL) == -1) {
        printf("Failed to get os time\n");
        ret = -1;
        goto cleanup;
    }

    u8NaluBytes = *nalBuf;
    pNaluCurr = nalBuf + 1;
    s32NaluRemain = nalSize - 1;

    if (s32NaluRemain <= MAX_RTP_PKT_LENGTH) {
        handle->pRtpFixedHdr->u1Marker = 1;
        handle->pRtpFixedHdr->u16SeqNum =
            htons(handle->u16SeqNum++);
        handle->pNaluHdr = (StNaluHdr *)(sendBuf + 12);
        handle->pNaluHdr->u1F = (u8NaluBytes & 0x80) >> 7;
        handle->pNaluHdr->u2Nri = (u8NaluBytes & 0x60) >> 5;
        handle->pNaluHdr->u5Type = u8NaluBytes & 0x1f;

        naluPayload = (sendBuf + 13);
        memcpy(naluPayload, pNaluCurr, s32NaluRemain);

        s32Bytes = s32NaluRemain + 13;
        if (sendto(
                handle->s32Sock, sendBuf, s32Bytes, 0,
                (struct sockaddr*)&handle->stServAddr,
                sizeof(handle->stServAddr)) < 0) {
            ret = -1;
            goto cleanup;
        }
    } else {
        handle->pFuInd = (StFuIndicator*)(sendBuf + 12);
        handle->pFuInd->u1F = (u8NaluBytes & 0x80) >> 7;
        handle->pFuInd->u2Nri = (u8NaluBytes & 0x60) >> 5;
        handle->pFuInd->u5Type = 28;

        handle->pFuHdr = (StFuHdr *)(sendBuf + 13);
        handle->pFuHdr->u1R = 0;
        handle->pFuHdr->u5Type = u8NaluBytes & 0x1f;

        naluPayload = (sendBuf + 14);

        while (s32NaluRemain > 0) {
            handle->pRtpFixedHdr->u16SeqNum = htons(handle->u16SeqNum++);
            handle->pRtpFixedHdr->u1Marker =
                (s32NaluRemain <= MAX_RTP_PKT_LENGTH) ? 1 : 0;

            handle->pFuHdr->u1E = 
                (s32NaluRemain <= MAX_RTP_PKT_LENGTH) ? 1 : 0;
            handle->pFuHdr->u1S =
                (s32NaluRemain == (nalSize - 1)) ? 1 : 0;

            s32Bytes = (s32NaluRemain < MAX_RTP_PKT_LENGTH)
                           ? s32NaluRemain
                           : MAX_RTP_PKT_LENGTH;

            memcpy(naluPayload, pNaluCurr, s32Bytes);

            s32Bytes = s32Bytes + 14;
            if (sendto(
                    handle->s32Sock, sendBuf, s32Bytes, 0,
                    (struct sockaddr *)&handle->stServAddr,
                    sizeof(handle->stServAddr)) < 0) {
                ret = -1;
                goto cleanup;
            }

            pNaluCurr += MAX_RTP_PKT_LENGTH;
            s32NaluRemain -= MAX_RTP_PKT_LENGTH;
        }
    }

cleanup:
    if (sendBuf)
        free((void*)sendBuf);

    return ret;
}

static int rtp_send_nalug711(rtpHandle handle, char *buf, int bufsize) {
    char *sendBuf;
    int s32Bytes = 0;
    int ret = 0;

    sendBuf = (char *)calloc(MAX_RTP_PKT_LENGTH + 100, sizeof(char));
    if (!sendBuf) {
        ret = -1;
        goto cleanup;
    }
    handle->pRtpFixedHdr = (StRtpFixedHdr *)sendBuf;
    handle->pRtpFixedHdr->u7Payload = G711;
    handle->pRtpFixedHdr->u2Version = 2;

    handle->pRtpFixedHdr->u1Marker = 1;

    handle->pRtpFixedHdr->u32SSrc = handle->u32SSrc;

    handle->pRtpFixedHdr->u16SeqNum = htons(handle->u16SeqNum++);

    memcpy(sendBuf + 12, buf, bufsize);

    handle->pRtpFixedHdr->u32TimeStamp = htonl(handle->u32TimeStampCurr);
    s32Bytes = bufsize + 12;
    if (sendto(
            handle->s32Sock, sendBuf, s32Bytes, 0,
            (struct sockaddr*)&handle->stServAddr,
            sizeof(handle->stServAddr)) < 0) {
        printf("Failed to send!");
        ret = -1;
        goto cleanup;
    }
cleanup:
    if (sendBuf)
        free((void*)sendBuf);

    return ret;
}

unsigned int rtp_send(unsigned int rtp, char *data, int size,
    unsigned int tstamp) {
    char *nalBuf, *dataEnd;
    rtpHandle handle = (rtpHandle)rtp;
    unsigned int naluToken;
    int nalSize = 0;

    handle->u32TimeStampCurr = tstamp;

    if (_h264 == handle->emPayload) {
        dataEnd = data + size;
        for (;data < dataEnd - 5; data++) {
            memcpy(&naluToken, data, 4 * sizeof(char));
            if (0x01000000 == naluToken) {
                data += 4;
                nalBuf = data;
                break;
            }
        }
        for (;data < dataEnd - 5; data++) {
            memcpy(&naluToken, data, 4 * sizeof(char));
            if (0x01000000 == naluToken) {
                nalSize = (int)(data - nalBuf);
                if (rtp_send_naluh264(handle, nalBuf, nalSize) == -1) {
                    return -1;
                }
                data += 4;
                nalBuf = data;
            }
        }
        if (data > nalBuf) {
            nalSize = (int)(data - nalBuf);
            if (rtp_send_naluh264(handle, nalBuf, nalSize) == -1) {
                return -1;
            }
        }
    } else if (_h264nalu == handle->emPayload) {
        if (rtp_send_naluh264(handle, data, size) == -1)
            return -1;
    } else if (_g711 == handle->emPayload) {
        if (rtp_send_nalug711(handle, data, size) == -1)
            return -1;
    } else return -1;

    return 0;
}