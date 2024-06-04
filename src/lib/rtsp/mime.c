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
#ifdef I_THOUGHT_THIS_IS_NECESSARY_BUT_NOT_SO_KEEP_IT_FOR_GOODNESS
static unsigned char __byte_adjust_table[256] = 
{0,128,64,192,32,160,96,224,16,144,80,208,48,176,112,240,8,136,72,200,40,168,104,232,24,152,88,216,56,184,120,248,4,132,68,196,36,164,100,228,20,148,84,212,52,180,116,244,12,140,76,204,44,172,108,236,28,156,92,220,60,188,124,252,2,130,66,194,34,162,98,226,18,146,82,210,50,178,114,242,10,138,74,202,42,170,106,234,26,154,90,218,58,186,122,250,6,134,70,198,38,166,102,230,22,150,86,214,54,182,118,246,14,142,78,206,46,174,110,238,30,158,94,222,62,190,126,254,1,129,65,193,33,161,97,225,17,145,81,209,49,177,113,241,9,137,73,201,41,169,105,233,25,153,89,217,57,185,121,249,5,133,69,197,37,165,101,229,21,149,85,213,53,181,117,245,13,141,77,205,45,173,109,237,29,157,93,221,61,189,125,253,3,131,67,195,35,163,99,227,19,147,83,211,51,179,115,243,11,139,75,203,43,171,107,235,27,155,91,219,59,187,123,251,7,135,71,199,39,167,103,231,23,151,87,215,55,183,119,247,15,143,79,207,47,175,111,239,31,159,95,223,63,191,127,255};

static inline char __byte_adjust(unsigned char x)
{
    char ret = x;
#ifndef __RTSP_BIG_ENDIAN
    ret = __byte_adjust_table[x];

#endif
    return ret;
}
#endif

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