#pragma once

typedef enum {
    GM_ALIGN_TOP_LEFT,
    GM_ALIGN_TOP_CENTER,
    GM_ALIGN_TOP_RIGHT,
    GM_ALIGN_BOTTOM_LEFT,
    GM_ALIGN_BOTTOM_CENTER,
    GM_ALIGN_BOTTOM_RIGHT,
    GM_ALIGN_CENTER
} gm_osd_align;

typedef enum {
    GM_OSD_DIM_16PX,
    GM_OSD_DIM_32PX,
    GM_OSD_DIM_64PX,
    GM_OSD_DIM_128PX,
    GM_OSD_DIM_256PX,
    GM_OSD_DIM_512PX,
    GM_OSD_DIM_END
} gm_osd_dim;

typedef enum {
    GM_OSD_OPAL_100,
    GM_OSD_OPAL_75,
    GM_OSD_OPAL_62_5,
    GM_OSD_OPAL_50,
    GM_OSD_OPAL_37_5,
    GM_OSD_OPAL_25,
    GM_OSD_OPAL_12_5,
    GM_OSD_OPAL_0
} gm_osd_opal;

typedef enum {
    GM_OSD_ZOOM_1X,
    GM_OSD_ZOOM_2X,
    GM_OSD_ZOOM_4X
} gm_osd_zoom;

typedef struct {
    // Values >= 4 enable the OSG mode
    int channel;
    int enabled;
    unsigned int x;
    unsigned int y;
    gm_osd_opal opacity;
    gm_osd_zoom zoom;
    gm_osd_align align;
    unsigned short osgChan;
    char reserved[18];
} gm_osd_cnf;

typedef struct {
    int exists;
    // Uses YUV422 pixel format
    char *buffer;
    // Must not exceed 16384 bytes, 8192 bytes for some chips
    unsigned int length;
    gm_osd_dim width;
    gm_osd_dim height;
    int osgTpColor;
    unsigned short osgChan;
    char reserved[14];
} gm_osd_img;

typedef struct {
    // OSG mode only uses the first image
    gm_osd_img image[4];
    int reserved[5];
} gm_osd_imgs;