#pragma once

#include "gm_common.h"
#include "gm_aud.h"
#include "gm_cap.h"
#include "gm_osd.h"
#include "gm_venc.h"

#define GM_LIB_API "1.0"
#define GM_MAX_SNAP (1024 * 1024)

extern char audioOn, keepRunning;

extern hal_chnstate gm_state[GM_VENC_CHN_NUM];
extern int (*gm_aud_cb)(hal_audframe*);
extern int (*gm_vid_cb)(char, hal_vidstream*);

void gm_hal_deinit(void);
int gm_hal_init(void);

void gm_audio_deinit(void);
int gm_audio_init(int samplerate);
void *gm_audio_thread(void);

int gm_channel_bind(char index);
int gm_channel_unbind(char index);

int gm_pipeline_create(char mirror, char flip);
void gm_pipeline_destroy(void);

int gm_region_create(char handle, hal_rect rect, short opacity);
void gm_region_destroy(char handle);
int gm_region_setbitmap(char handle, hal_bitmap *bitmap);

int gm_video_create(char index, hal_vidconfig *config);
int gm_video_destroy(char index);
int gm_video_destroy_all(void);
void gm_video_request_idr(char index);
int gm_video_snapshot_grab(short width, short height, char quality, hal_jpegdata *jpeg);
void *gm_video_thread(void);

void gm_system_deinit(void);
int gm_system_init(void);

enum {
    GM_ERR_TIMEOUT = -4,
    GM_ERR_MDBUF_TOOSMALL,
    GM_ERR_BSBUF_TOOSMALL,
    GM_ERR_NONEXISTENT_FD
};

enum {
    GM_POLL_READ = 1,
    GM_POLL_WRITE
};

typedef struct {
    void *handle;

    void  (*fnDeclareStruct)(void *name, char *string, int size, int libVersion);
    int   (*fnExit)(void);
    int   (*fnInit)(int libVersion);

    void* (*fnCreateDevice)(gm_lib_dev type);
    int   (*fnDestroyDevice)(void *device);
    int   (*fnSetDeviceConfig)(void *device, void *config);

    void* (*fnCreateGroup)(void);
    int   (*fnDestroyGroup)(void *group);
    int   (*fnRefreshGroup)(void *group);

    int   (*fnSetRegionBitmaps)(gm_osd_imgs *bitmaps);
    int   (*fnSetRegionConfig)(void *device, gm_osd_cnf *config);

    void* (*fnBind)(void *group, void *source, void *dest);
    int   (*fnUnbind)(void *group);

    int   (*fnPollStream)(gm_common_pollfd *fds, int count, int millis);
    int   (*fnReceiveStream)(gm_common_strm *strms, int count);

    int   (*fnSnapshot)(gm_venc_snap *snapshot, int millis);

    int   (*fnRequestIdr)(void *device);
} gm_lib_impl;

static int gm_lib_load(gm_lib_impl *aio_lib) {
    if (!(aio_lib->handle = dlopen("libgm.so", RTLD_LAZY | RTLD_GLOBAL)))
        HAL_ERROR("gm_lib", "Failed to load library!\nError: %s\n", dlerror());

    if (!(aio_lib->fnDeclareStruct = (void(*)(void *name, char *string, int size, int libVersion))
        hal_symbol_load("gm_lib", aio_lib->handle, "gm_init_attr")))
        return EXIT_FAILURE;

    if (!(aio_lib->fnExit = (int(*)(void))
        hal_symbol_load("gm_lib", aio_lib->handle, "gm_release")))
        return EXIT_FAILURE;

    if (!(aio_lib->fnInit = (int(*)(int libVersion))
        hal_symbol_load("gm_lib", aio_lib->handle, "gm_init_private")))
        return EXIT_FAILURE;

    if (!(aio_lib->fnCreateDevice = (void*(*)(gm_lib_dev type))
        hal_symbol_load("gm_lib", aio_lib->handle, "gm_new_obj")))
        return EXIT_FAILURE;

    if (!(aio_lib->fnDestroyDevice = (int(*)(void *device))
        hal_symbol_load("gm_lib", aio_lib->handle, "gm_delete_obj")))
        return EXIT_FAILURE;

    if (!(aio_lib->fnSetDeviceConfig = (int(*)(void *device, void *config))
        hal_symbol_load("gm_lib", aio_lib->handle, "gm_set_attr")))
        return EXIT_FAILURE;

    if (!(aio_lib->fnCreateGroup = (void*(*)(void))
        hal_symbol_load("gm_lib", aio_lib->handle, "gm_new_groupfd")))
        return EXIT_FAILURE;

    if (!(aio_lib->fnDestroyGroup = (int(*)(void *group))
        hal_symbol_load("gm_lib", aio_lib->handle, "gm_delete_groupfd")))
        return EXIT_FAILURE;

    if (!(aio_lib->fnRefreshGroup = (int(*)(void *group))
        hal_symbol_load("gm_lib", aio_lib->handle, "gm_apply")))
        return EXIT_FAILURE;

    if (!(aio_lib->fnSetRegionBitmaps = (int(*)(gm_osd_imgs *bitmaps))
        hal_symbol_load("gm_lib", aio_lib->handle, "gm_set_osd_mark_image")))
        return EXIT_FAILURE;

    if (!(aio_lib->fnSetRegionConfig = (int(*)(void *device, gm_osd_cnf *config))
        hal_symbol_load("gm_lib", aio_lib->handle, "gm_set_osd_mark")))
        return EXIT_FAILURE;

    if (!(aio_lib->fnBind = (void*(*)(void *group, void *source, void *dest))
        hal_symbol_load("gm_lib", aio_lib->handle, "gm_bind")))
        return EXIT_FAILURE;

    if (!(aio_lib->fnUnbind = (int(*)(void *group))
        hal_symbol_load("gm_lib", aio_lib->handle, "gm_unbind")))
        return EXIT_FAILURE;

    if (!(aio_lib->fnPollStream = (int(*)(gm_common_pollfd *fds, int count, int millis))
        hal_symbol_load("gm_lib", aio_lib->handle, "gm_poll")))
        return EXIT_FAILURE;

    if (!(aio_lib->fnReceiveStream = (int(*)(gm_common_strm *strms, int count))
        hal_symbol_load("gm_lib", aio_lib->handle, "gm_recv_multi_bitstreams")))
        return EXIT_FAILURE;

    if (!(aio_lib->fnSnapshot = (int(*)(gm_venc_snap *snapshot, int millis))
        hal_symbol_load("gm_lib", aio_lib->handle, "gm_request_snapshot")))
        return EXIT_FAILURE;

    if (!(aio_lib->fnRequestIdr = (int(*)(void *device))
        hal_symbol_load("gm_lib", aio_lib->handle, "gm_request_keyframe")))
        return EXIT_FAILURE;

    return EXIT_SUCCESS;
}

static void gm_lib_unload(gm_lib_impl *aio_lib) {
    if (aio_lib->handle) dlclose(aio_lib->handle);
    aio_lib->handle = NULL;
    memset(aio_lib, 0, sizeof(*aio_lib));
}