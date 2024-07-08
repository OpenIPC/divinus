#ifndef __RTSP_SERVER_H
#define __RTSP_SERVER_H

#if defined (__cplusplus)
extern "C" {
#endif
#include <unistd.h>
#include <time.h>

/******************************************************************************
 *              DEFINITIONS
 ******************************************************************************/
#define SERVER_RTSP_PORT 554
#define SERVER_RTP_PORT 5004
#define SERVER_RTCP_PORT 5025
#define RTSP_MAXIMUM_FRAMERATE 60
#define RTSP_MAXIMUM_CONNECTIONS 16

#define STR_RTSP_VERSION "RTSP/1.0"

/* __rtsp_obj_t is private. you will not see it */
typedef struct __rtsp_obj_t *rtsp_handle;

/******************************************************************************
 *              LIBRARY FUNCTIONS
 ******************************************************************************/
/* put virtual pointer to 'buf', which consists of 1 or more NALUs (start code required). 
   SPS and PPS parameters are automatically collected during execution. */

int rtp_send_h26x(rtsp_handle h, unsigned char *buf, size_t len, char isH265);
int rtp_send_mp3(rtsp_handle h, unsigned char *buf, size_t len);

extern void rtsp_finish(rtsp_handle h);

extern rtsp_handle rtsp_create(unsigned char max_con, int priority);

#if defined (__cplusplus)
}
#endif

#endif