#pragma once

#include "gm_common.h"
#include "gm_cap.h"
#include "gm_osd.h"
#include "gm_venc.h"

#define GM_LIB_API "1.0"

extern char keepRunning;

extern hal_chnstate gm_state[GM_VENC_CHN_NUM];
extern int (*gm_aud_cb)(hal_audframe*);
extern int (*gm_venc_cb)(char, hal_vidstream*);

void gm_hal_deinit(void);
int gm_hal_init(void);

int gm_channel_bind(char index);
int gm_channel_unbind(char index);

int gm_pipeline_create(char mirror, char flip);
void gm_pipeline_destroy(void);

int gm_video_create(char index, hal_vidconfig *config);
int gm_video_destroy(char index);
int gm_video_destroy_all(void);
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

    int (*fnDeclareStruct)(void *name, char *string, int size, int libVersion);
    int (*fnExit)(void);
    int (*fnInit)(int libVersion);

    int (*fnCreateDevice)(gm_lib_dev type);
    int (*fnDestroyDevice)(int device);
    int (*fnSetDeviceConfig)(int device, void *config);

    int (*fnCreateGroup)(void);
    int (*fnDestroyGroup)(int group);
    int (*fnRefreshGroup)(int group);

    int (*fnSetRegionBitmaps)(gm_osd_imgs *bitmaps);
    int (*fnSetRegionConfig)(int device, gm_osd_cnf *config);

    int (*fnBind)(int group, int source, int dest);
    int (*fnUnbind)(int group);

    int (*fnPollStream)(gm_venc_fds *fds, int count, int millis);
    int (*fnReceiveStream)(gm_venc_strm *strms, int count);

    int (*fnSnapshot)(gm_venc_snap *snapshot, int millis);
} gm_lib_impl;

static int gm_lib_load(gm_lib_impl *aio_lib) {
    if (!(aio_lib->handle = dlopen("libgm.so", RTLD_LAZY | RTLD_GLOBAL))) {
        fprintf(stderr, "[gm_lib] Failed to load library!\nError: %s\n", dlerror());
        return EXIT_FAILURE;
    }

    if (!(aio_lib->fnDeclareStruct = (int(*)(void *name, char *string, int size, int libVersion))
        dlsym(aio_lib->handle, "gm_init_attr"))) {
        fprintf(stderr, "[gm_lib] Failed to acquire symbol gm_init_attr!\n");
        return EXIT_FAILURE;
    }

    if (!(aio_lib->fnExit = (int(*)(void))
        dlsym(aio_lib->handle, "gm_release"))) {
        fprintf(stderr, "[gm_lib] Failed to acquire symbol gm_release!\n");
        return EXIT_FAILURE;
    }

    if (!(aio_lib->fnInit = (int(*)(int libVersion))
        dlsym(aio_lib->handle, "gm_init_private"))) {
        fprintf(stderr, "[gm_lib] Failed to acquire symbol gm_init_private!\n");
        return EXIT_FAILURE;
    }

    if (!(aio_lib->fnCreateDevice = (int (*)(gm_lib_dev type))
        dlsym(aio_lib->handle, "gm_new_obj"))) {
        fprintf(stderr, "[gm_lib] Failed to acquire symbol gm_new_obj!\n");
        return EXIT_FAILURE;
    }

    if (!(aio_lib->fnDestroyDevice = (int (*)(int device))
        dlsym(aio_lib->handle, "gm_delete_obj"))) {
        fprintf(stderr, "[gm_lib] Failed to acquire symbol gm_delete_obj!\n");
        return EXIT_FAILURE;
    }

    if (!(aio_lib->fnSetDeviceConfig = (int (*)(int device, void *config))
        dlsym(aio_lib->handle, "gm_set_attr"))) {
        fprintf(stderr, "[gm_lib] Failed to acquire symbol gm_set_attr!\n");
        return EXIT_FAILURE;
    }

    if (!(aio_lib->fnCreateGroup = (int (*)(void))
        dlsym(aio_lib->handle, "gm_new_groupfd"))) {
        fprintf(stderr, "[gm_lib] Failed to acquire symbol gm_new_groupfd!\n");
        return EXIT_FAILURE;
    }

    if (!(aio_lib->fnDestroyGroup = (int (*)(int group))
        dlsym(aio_lib->handle, "gm_delete_groupfd"))) {
        fprintf(stderr, "[gm_lib] Failed to acquire symbol gm_delete_groupfd!\n");
        return EXIT_FAILURE;
    }

    if (!(aio_lib->fnRefreshGroup = (int (*)(int group))
        dlsym(aio_lib->handle, "gm_apply"))) {
        fprintf(stderr, "[gm_lib] Failed to acquire symbol gm_apply!\n");
        return EXIT_FAILURE;
    }

    if (!(aio_lib->fnBind = (int (*)(int group, int source, int dest))
        dlsym(aio_lib->handle, "gm_bind"))) {
        fprintf(stderr, "[gm_lib] Failed to acquire symbol gm_bind!\n");
        return EXIT_FAILURE;
    }

    if (!(aio_lib->fnUnbind = (int (*)(int group))
        dlsym(aio_lib->handle, "gm_unbind"))) {
        fprintf(stderr, "[gm_lib] Failed to acquire symbol gm_unbind!\n");
        return EXIT_FAILURE;
    }

    if (!(aio_lib->fnPollStream = (int (*)(gm_venc_fds *fds, int count, int millis))
        dlsym(aio_lib->handle, "gm_poll"))) {
        fprintf(stderr, "[gm_lib] Failed to acquire symbol gm_poll!\n");
        return EXIT_FAILURE;
    }

    if (!(aio_lib->fnReceiveStream = (int (*)(gm_venc_strm *strms, int count))
        dlsym(aio_lib->handle, "gm_recv_multi_bitstreams"))) {
        fprintf(stderr, "[gm_lib] Failed to acquire symbol gm_recv_multi_bitstreams!\n");
        return EXIT_FAILURE;
    }

    if (!(aio_lib->fnSnapshot = (int (*)(gm_venc_snap *snapshot, int millis))
        dlsym(aio_lib->handle, "gm_request_snapshot"))) {
        fprintf(stderr, "[gm_lib] Failed to acquire symbol gm_request_snapshot!\n");
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}

static void gm_lib_unload(gm_lib_impl *aio_lib) {
    if (aio_lib->handle) dlclose(aio_lib->handle);
    aio_lib->handle = NULL;
    memset(aio_lib, 0, sizeof(*aio_lib));
}