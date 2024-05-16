#ifndef ALIGN_BACK
#define ALIGN_BACK(x, a) (((x) / (a)) * (a))
#endif
#ifndef ALIGN_UP
#define ALIGN_UP(x, a) ((((x) + ((a)-1)) / a) * a)
#endif
#ifndef MIN
#define MIN(a, b) (((a) < (b)) ? (a) : (b))
#endif
#ifndef MAX
#define MAX(a, b) (((a) > (b)) ? (a) : (b))
#endif

typedef enum {
    HAL_VIDCODEC_H264,
    HAL_VIDCODEC_H265,
    HAL_VIDCODEC_MJPG,
    HAL_VIDCODEC_JPG
} hal_vidcodec;

typedef enum {
    HAL_VIDMODE_CBR,
    HAL_VIDMODE_VBR,
    HAL_VIDMODE_QP,
    HAL_VIDMODE_ABR,
    HAL_VIDMODE_AVBR
} hal_vidmode;

typedef enum {
    HAL_VIDPROFILE_BASELINE,
    HAL_VIDPROFILE_MAIN,
    HAL_VIDPROFILE_HIGH
} hal_vidprofile;

typedef struct {
    int fileDesc;
    char enable;
    char mainLoop;
} hal_chnstate;

typedef struct {
    unsigned short width, height;
    hal_vidcodec codec;
    hal_vidmode mode;
    hal_vidprofile profile;
    unsigned char gop, framerate, minQual, maxQual;
    unsigned short bitrate, maxBitrate;
} hal_vidconfig;

typedef struct {
	unsigned long long addr;
    unsigned char *data;
    unsigned int length;
    unsigned long long timestamp;
    char endFrame;
    char naluType;
    unsigned int offset;
    unsigned int packetNum;
} hal_vidpack;

typedef struct {
	hal_vidpack *pack;
	unsigned int count;
	unsigned int seq;
} hal_vidstream;