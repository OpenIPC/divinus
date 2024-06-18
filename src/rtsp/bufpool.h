#ifndef _RTSP_BUFFPOOL_H
#define _RTSP_BUFFPOOL_H

#include <pthread.h>
#include "common.h"
#include "list.h"
#include "hash.h"

#if defined (__cplusplus)
extern "C" {
#endif

/******************************************************************************
 *              DATA STRUCTURES
 ******************************************************************************/
struct __bufpool_elem_t {
    unsigned int    magic;
    void *buf;
    int ref_count;
    int pos;
    int (*reset)(void *buf);
    struct list_t list_entry; // free list entry
};

struct __bufpool_t {
    pthread_mutex_t mutex;
    struct list_head_t free_list;
    struct __bufpool_elem_t *elems;
    hash_handle buf_table;
    unsigned int num;
};

typedef struct __bufpool_t *bufpool_handle;

/******************************************************************************
 *              FUNCTION DECLARATIONS
 ******************************************************************************/
static inline bufpool_handle bufpool_create(int num, void * (*bufgetter_fxn)(int i), int (*reset)(void *buf), size_t each_size);
static void bufpool_delete(bufpool_handle h);

//static inline int bufpool_get_free(bufpool_handle h,void **p_buf);
static inline int __bufpool_ref_manipulate(bufpool_handle h, void * buf, int val);
static inline int bufpool_detach(bufpool_handle h, void *buf);
static inline int bufpool_attach(bufpool_handle h, void *buf);
static inline void bufpool_statistics(bufpool_handle h);

/******************************************************************************
 *              INLINE FUNCTIONS 
 ******************************************************************************/
 /* to use strict-aliase compiler optimization, we cannot use void **, so use macro : completely dependent of gcc-extension :p */
#define bufpool_get_free(__h,__p_buf) ({ \
    __label__ __unlock;\
    int __ret = FAILURE; \
    struct list_t *__e; \
    struct __bufpool_elem_t *__p; \
    pthread_mutex_lock(&__h->mutex); \
    ASSERT(__e = list_pop(&__h->free_list), goto __unlock); \
    list_upcast(__p,__e); \
    __p->ref_count += 1; \
    *__p_buf = __p->buf; \
    __ret = SUCCESS; \
__unlock: \
    pthread_mutex_unlock(&__h->mutex); \
    __ret; \
})


static inline int __bufpool_ref_manipulate(bufpool_handle h, void *buf, int val)
{
    struct __bufpool_elem_t *p;
    
    DASSERT(h->elems,return FAILURE);
    DASSERT(h->num > 0,return FAILURE);

    ASSERT(p = hash_lookup(h->buf_table,(hash_key_t) buf), return FAILURE);

    DASSERT(CHECK_MAGIC(MAGIC_BUFPOOL_ELEM,&p), return FAILURE);
    
    p->ref_count += val;

    TEST(p->ref_count >= 0, ({
        ERR("double detach at pointer %p: corrupted use of reference counting\n", buf);
        return FAILURE;}));

    if(p->ref_count == 0) {
        if(p->reset) {
            ASSERT(p->reset(buf) == SUCCESS,
                return FAILURE);
        }

        ASSERT(list_push(&h->free_list,&p->list_entry) == SUCCESS,
            return FAILURE);
    }

    //DBG("%p: %d\n", buf, p->ref_count);

    return SUCCESS;
}


static inline void bufpool_statistics(bufpool_handle h)
{
//#ifdef __DEBUG
    DASSERT(h, ERR("null pointer\n"));
    int free_elems;
    float free_elems_ratio;
    int i;
    int j = 0;
    int max = 50;
    char tmp[max+1];

    pthread_mutex_lock(&h->mutex);

    free_elems = list_length(&h->free_list);
    free_elems_ratio = ((float)free_elems/(float)h->num) * max; 

    for(i = 0; i < free_elems_ratio; i++) {
        tmp[j++] = '=';
    }
    for(; i < max; i++) {
        tmp[j++] = '.';
    }
    tmp[j]=0;

    printf("%p[%s]%d/%d\n",h,
        tmp,free_elems,h->num);

    pthread_mutex_unlock(&h->mutex);
//#endif
}

static inline int bufpool_attach(bufpool_handle h, void *buf)
{
    int ret;
    DASSERT(h,return FAILURE);

    pthread_mutex_lock(&h->mutex);
    
    ret = __bufpool_ref_manipulate(h,buf,1);
    
    pthread_mutex_unlock(&h->mutex);

    return ret;
}

static inline int bufpool_detach(bufpool_handle h, void *buf)
{
    int ret;
    DASSERT(h,return FAILURE);

    pthread_mutex_lock(&h->mutex);
    
    ret = __bufpool_ref_manipulate(h,buf,-1);
    
    pthread_mutex_unlock(&h->mutex);

    return ret;
}

static inline bufpool_handle bufpool_create(int num, void * (*bufgetter_fxn)(int i), int (*reset)(void *buf), size_t each_size)
{
    int i;
    bufpool_handle nh = NULL;


    DASSERT(bufgetter_fxn, return NULL);


    TALLOC(nh,return NULL);


    pthread_mutex_init(&nh->mutex,NULL);


    ASSERT(nh->elems = calloc(num,sizeof(struct __bufpool_elem_t)), goto error);


    ASSERT(nh->buf_table = hash_create(num, each_size),
        goto error);


    for(i = 0; i< num; i++) {

        nh->elems[i].magic = MAGIC_BUFPOOL_ELEM;

        nh->elems[i].pos = i;
        nh->elems[i].reset = reset;

        ASSERT(nh->elems[i].buf = bufgetter_fxn(i),
            goto error);

        ASSERT(hash_add(nh->buf_table, (hash_key_t)nh->elems[i].buf,
            &(nh->elems[i])) == SUCCESS,
            goto error);

        MUST(list_push(&nh->free_list, &(nh->elems[i].list_entry)) == SUCCESS,
            goto error);

#ifdef __DEBUG
        struct __bufpool_elem_t *p;

        /* hash health check */
        ASSERT(p = hash_lookup(nh->buf_table,(hash_key_t)(nh->elems[i].buf)), goto error);

        ASSERT(CHECK_MAGIC(MAGIC_BUFPOOL_ELEM,&p),goto error);

        ASSERT(p == &nh->elems[i], goto error);
#endif
    }
    

    nh->num = num;


    return nh;
error:
    bufpool_delete(nh);
    return NULL;
}

static void bufpool_delete(bufpool_handle h)
{
    int i;
    if (h) {
        if (h->elems) {
            for(i = 0;i < h->num; i++) {
                if(h->elems[i].reset) {
                    h->elems[i].reset(h->elems[i].buf);
                }
            }
            FREE(h->elems);
        }

        hash_destroy(h->buf_table);

        pthread_mutex_destroy(&h->mutex);
        FREE(h);
    }
}


#if defined (__cplusplus)
}
#endif
#endif