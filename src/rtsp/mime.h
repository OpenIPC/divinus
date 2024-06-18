#ifndef _RTSP_MIME_H
#define _RTSP_MIME_H

#if defined (__cplusplus)
extern "C" {
#endif

#if defined (__cplusplus)
}
#endif

/******************************************************************************
 *              DATA STRUCTURES
 ******************************************************************************/
 struct __mime_encoded_obj_t {
     char *result;
     size_t len_result;
     size_t len_src;
     unsigned int base; /* 16 or 32 or 64 */
 };
 typedef struct __mime_encoded_obj_t *mime_encoded_handle;

/******************************************************************************
 *              DECLARATIONS
 ******************************************************************************/
mime_encoded_handle mime_base64_create(char *src, size_t len);
mime_encoded_handle mime_base16_create(char *src, size_t len);
static inline void mime_encoded_delete(mime_encoded_handle h);

/******************************************************************************
 *              INLINE FUNCTIONS
 ******************************************************************************/
static inline void mime_encoded_delete(mime_encoded_handle h)
{
    if(h) {
        FREE(h->result);
        FREE(h);
    }
}

#endif