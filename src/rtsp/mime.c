#include "common.h"
#include "mime.h"
/******************************************************************************
 *              PRIVATE DEFINITIONS
 ******************************************************************************/
#define __MIME_BASE64_TEST_SOURCE "ABCDEFGnzmk.ghijdtyo9ryfyhszldfho;asrupq8w49ryaishv;zxvj;oalsurp89w4qytzjhv;sadzaas;fdh;qwoe45yp9gha;ovhajvas;fguasd;f2139457398579 "

#define __MIME_BASE64_TEST_RESULT "QUJDREVGR256bWsuZ2hpamR0eW85cnlmeWhzemxkZmhvO2FzcnVwcTh3NDlyeWFpc2h2O3p4dmo7b2Fsc3VycDg5dzRxeXR6amh2O3NhZHphYXM7ZmRoO3F3b2U0NXlwOWdoYTtvdmhhanZhcztmZ3Vhc2Q7ZjIxMzk0NTczOTg1Nzk="

/******************************************************************************
 *              PRIVATE DATA
 ******************************************************************************/
static char __base64_charmap[64] = 
{'A','B','C','D','E','F','G','H','I','J','K','L','M','N','O','P','Q','R','S','T','U','V','W','X','Y','Z','a','b','c','d','e','f','g','h','i','j','k','l','m','n','o','p','q','r','s','t','u','v','w','x','y','z','0','1','2','3','4','5','6','7','8','9','+','/'};
   
static char __base16_charmap[16] = 
{'0','1','2','3','4','5','6','7','8','9','A','B','C','D','E','F'};

/******************************************************************************
 *              PRIATE FUNCTIONS
 ******************************************************************************/

/******************************************************************************
 *              PUBLIC FUNCTIONS
 ******************************************************************************/
mime_encoded_handle mime_base16_create(char *src, size_t len)
{
    mime_encoded_handle nh = NULL;
    int i;
    char *result = NULL;
    char *p;
    
    DASSERT(src, return NULL);
    DASSERT(len > 0, return NULL);
    
    TALLOC(nh, return NULL);
    
    ASSERT(result = calloc(len * 2 + 1, sizeof(char)), goto error);

    p = result;

    for (i = 0; i < len; i++) {
        *(p++) = __base16_charmap[(src[i] & 0xF0) >> 4];
        *(p++) = __base16_charmap[(src[i] & 0x0F)];
    }

    *p = 0;

    nh->result = result;
    nh->len_result = len * 2 + 1;
    nh->len_src = len;
    nh->base = 16;

    DASSERT(strlen(nh->result) + 1 == nh->len_result, goto error);

    return nh;
error:
    mime_encoded_delete(nh);
    return NULL;
}

mime_encoded_handle mime_base64_create(char *src, size_t len)
{
    mime_encoded_handle nh = NULL;
    int i;
    char *result = NULL;
    char *p;
    DASSERT(src, return NULL);
    DASSERT(len > 0, return NULL);


    TALLOC(nh, return NULL);
   
    ASSERT(result = calloc(len * 2 + 1, sizeof(char)), goto error);
    p = result;

    for(i = 0; i <= len - 3; i+=3) {
        *(p++) = __base64_charmap[(src[i] & 0xFC) >> 2];
        *(p++) = __base64_charmap[((src[i] & 0x03) << 4) | ((src[i+1] & 0xF0) >> 4)];
        *(p++) = __base64_charmap[((src[i+1] & 0x0F) << 2) | ((src[i+2] & 0xC0) >> 6)];
        *(p++) = __base64_charmap[src[i+2] & 0x3F];
    }

    switch(len - i) {
        /* 111111 _ 11 | 1111 _ 1111 | 00  _ '=' */
        case 2:
            *(p++) = __base64_charmap[(src[i] & 0xFC) >> 2];
            *(p++) = __base64_charmap[((src[i] & 0x03) << 4) | ((src[i+1] & 0xF0) >> 4)];
            *(p++) = __base64_charmap[(src[i+1] & 0x0F) << 2];
            *(p++) = '=';
            break;

        /* 111111 _ 11 | 0000 _ '=' _ '=' */
        case 1:
            *(p++) = __base64_charmap[(src[i] & 0xFC) >> 2];
            *(p++) = __base64_charmap[(src[i] & 0x3) << 4];
            *(p++) = '=';
            *(p++) = '=';
            break;

        case 0: 
            break;

        default: 
            ERR("BUG in base64 encoding\n"); 
            goto error;
    }

    *p = 0;

    nh->result = result;
    nh->len_result= 1 + p - result;
    nh->len_src = len;
    nh->base = 64;

    DASSERT(strlen(nh->result) + 1 == nh->len_result, goto error);

    return nh;
error:
    mime_encoded_delete(nh);
    return NULL;
}