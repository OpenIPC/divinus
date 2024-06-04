#ifndef _RTSP_THREAD_H
#define _RTSP_THREAD_H

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <pthread.h>

#include "common.h"

#define MAX_NUMTHREAD 20
#define MAX_THREADNAME 48
#define THREAD_SUCCESS ((void *) 0)
#define THREAD_FAILURE ((void *) -1)

#if defined (__cplusplus)
extern "C" {
#endif

/******************************************************************************/
/******************************************************************************/
/******************************************************************************/
/******************************************************************************/
/******************************************************************************/

typedef struct rendezvous_obj {
    int             orig;
    int             count;
    int             force;
    pthread_mutex_t mutex;
    pthread_cond_t  cond;
} rendezvous_obj;

typedef struct rendezvous_obj *rendezvous_handle;

static inline  rendezvous_handle rendezvous_create(int count);
static inline  void rendezvous_meet(rendezvous_handle hRv);
static inline  void rendezvous_force(rendezvous_handle hRv);
static inline  void rendezvous_reset(rendezvous_handle hRv);
static inline  void rendezvous_force_reset(rendezvous_handle hRv);
static inline  int rendezvous_delete(rendezvous_handle hRv);

/******************************************************************************
 * rendezvous_create
 ******************************************************************************/
static inline rendezvous_handle rendezvous_create(int count)
{
    rendezvous_handle hRv;

    if (count < 0) {
        ERR("Count (%d) must be > 0\n", count);
        return NULL;
    }

    hRv = calloc(1, sizeof(rendezvous_obj));

    if (hRv == NULL) {
        ERR("Failed to allocate space for rendezvous Object\n");
        return NULL;
    }

    pthread_mutex_init(&hRv->mutex, NULL);
    pthread_cond_init(&hRv->cond, NULL);

    hRv->count = count;
    hRv->orig = count;

    return hRv;
}

/******************************************************************************
 * rendezvous_delete
 ******************************************************************************/
static inline int rendezvous_delete(rendezvous_handle hRv)
{
    if (hRv) {
        pthread_mutex_destroy(&hRv->mutex);
        pthread_cond_destroy(&hRv->cond);
        FREE(hRv);
    }

    return SUCCESS;
}

/******************************************************************************
 * rendezvous_meet
 ******************************************************************************/
static inline void rendezvous_meet(rendezvous_handle hRv)
{
    DASSERT(hRv,return);


    
    pthread_mutex_lock(&hRv->mutex);
    
    if (!hRv->force) {
    
        if (hRv->count > 0) {
            hRv->count--;
        }
    
        if (hRv->count > 0 ) {
            pthread_cond_wait(&hRv->cond, &hRv->mutex);
        } 
        else {
            pthread_cond_broadcast(&hRv->cond);
            hRv->count = hRv->orig;
        }
    
    }
    pthread_mutex_unlock(&hRv->mutex);
    
}

/******************************************************************************
 * rendezvous_force
 ******************************************************************************/
static inline void rendezvous_force(rendezvous_handle hRv)
{
    DASSERT(hRv,return);

    pthread_mutex_lock(&hRv->mutex);
    hRv->force = TRUE;
    pthread_cond_broadcast(&hRv->cond);
    pthread_mutex_unlock(&hRv->mutex);
}

/******************************************************************************
 * rendezvous_reset
 ******************************************************************************/
static inline void rendezvous_reset(rendezvous_handle hRv)
{
    DASSERT(hRv,return);

    pthread_mutex_lock(&hRv->mutex);
    hRv->count = hRv->orig;
    hRv->force = FALSE;
    pthread_mutex_unlock(&hRv->mutex);
}

/******************************************************************************
 * rendezvous_forceAndReset
 ******************************************************************************/
static inline void rendezvous_force_reset(rendezvous_handle hRv)
{
    DASSERT(hRv,return);

    pthread_mutex_lock(&hRv->mutex);
    pthread_cond_broadcast(&hRv->cond);
    hRv->count = hRv->orig;
    hRv->force = FALSE;
    pthread_mutex_unlock(&hRv->mutex);
}

/******************************************************************************/
/******************************************************************************/
/******************************************************************************/
/******************************************************************************/
/******************************************************************************/

#define FIFO_EFLUSH      1   /**< The command was flushed (success). */

typedef struct fifo_object_t{
	pthread_mutex_t mutex;
	int             numBufs;
	int             flush;
	int             pipes[2];
	int             rfd;
	int             wfd;
} fifo_object;
typedef fifo_object *fifo_handle;
static inline  fifo_handle fifo_create(void);
static inline  int      fifo_get(fifo_handle hfifo, void *ptrPtr);
static inline  int      fifo_flush(fifo_handle hfifo);
static inline  int      fifo_put(fifo_handle hfifo, void *ptr);
static inline  int      fifo_getNumEntries(fifo_handle hfifo);
static inline  int      fifo_delete(fifo_handle hfifo);

/******************************************************************************
 * fifo_create
 ******************************************************************************/
static inline fifo_handle
//fifo_create(fifo_Attrs * attrs)
fifo_create()
{
	fifo_handle     hfifo;

	hfifo = calloc(1, sizeof(fifo_object));

	if (hfifo == NULL) {
		fprintf(stderr,
			"Failed to allocate space for fifo Object\n");
		return NULL;
	}

	if (pipe(hfifo->pipes)) {
		FREE(hfifo);
		return NULL;
	}

	hfifo->rfd = hfifo->pipes[0];
	hfifo->wfd = hfifo->pipes[1];

	pthread_mutex_init(&hfifo->mutex, NULL);

	return hfifo;
}

/******************************************************************************
 * fifo_delete
 ******************************************************************************/
static inline int
fifo_delete(fifo_handle hfifo)
{
	int             ret = SUCCESS;

	if (hfifo) {
		if (close(hfifo->pipes[0])) {
			ret = FAILURE;
		}

		if (close(hfifo->pipes[1])) {
			ret = FAILURE;
		}

		pthread_mutex_destroy(&hfifo->mutex);

		FREE(hfifo);
	}

	return ret;
}

/******************************************************************************
 * fifo_get
 ******************************************************************************/
static inline int
fifo_get(fifo_handle hfifo, void *ptrPtr)
{
	int             flush;
	int             numBytes;

	DASSERT(hfifo,return FAILURE);
	DASSERT(ptrPtr, return FAILURE);

	pthread_mutex_lock(&hfifo->mutex);
	flush = hfifo->flush;
	pthread_mutex_unlock(&hfifo->mutex);

	if (flush) {
		return FIFO_EFLUSH;
	}

	numBytes = read(hfifo->pipes[0], ptrPtr, sizeof(ptrPtr));

	if (numBytes != sizeof(ptrPtr)) {
		pthread_mutex_lock(&hfifo->mutex);
		flush = hfifo->flush;
		if (flush) {
			hfifo->flush = FALSE;
		}
		pthread_mutex_unlock(&hfifo->mutex);

		if (flush) {
			return FIFO_EFLUSH;
		}
		return FAILURE;
	}

	pthread_mutex_lock(&hfifo->mutex);
	hfifo->numBufs--;
	pthread_mutex_unlock(&hfifo->mutex);

	return SUCCESS;
}

/******************************************************************************
 * fifo_flush
 ******************************************************************************/
static inline int
fifo_flush(fifo_handle hfifo)
{
	char            ch = 0xff;

	DASSERT(hfifo, return FAILURE);

	pthread_mutex_lock(&hfifo->mutex);
	hfifo->flush = TRUE;
	pthread_mutex_unlock(&hfifo->mutex);

	/*
	 * Make sure any fifo_get() calls are unblocked 
	 */
	if (write(hfifo->pipes[1], &ch, 1) != 1) {
		return FAILURE;
	}

	return SUCCESS;
}

/******************************************************************************
 * fifo_put
 ******************************************************************************/
static inline int
fifo_put(fifo_handle hfifo, void *ptr)
{
	DASSERT(hfifo,return FAILURE);
	DASSERT(ptr,return FAILURE);

	pthread_mutex_lock(&hfifo->mutex);
	hfifo->numBufs++;
	pthread_mutex_unlock(&hfifo->mutex);

	if (write(hfifo->pipes[1], &ptr, sizeof(ptr)) != sizeof(ptr)) {
		return FAILURE;
	}

	return SUCCESS;
}

/******************************************************************************
 * fifo_getNumEntries
 ******************************************************************************/
static inline int
fifo_getNumEntries(fifo_handle hfifo)
{
	int             numEntries;

	DASSERT(hfifo,return FAILURE);

	pthread_mutex_lock(&hfifo->mutex);
	numEntries = hfifo->numBufs;
	pthread_mutex_unlock(&hfifo->mutex);

	return numEntries;
}

/******************************************************************************/
/******************************************************************************/
/******************************************************************************/
/******************************************************************************/
/******************************************************************************/

typedef struct shared_interface {
    rendezvous_handle rv_init;
    rendezvous_handle rv_cleanup;
    void *param_shared;
    struct global_state_t *gbl;
} shared_interface;

typedef struct thread_job {
    shared_interface *sharedp;
    void *param_priv;
    void *(*fxn)(void *);
    int priority;
    int started;
    fifo_handle hInPut;
    fifo_handle hInGet;
    fifo_handle hOutPut;
    fifo_handle hOutGet;
    char name[MAX_THREADNAME];
    pthread_t pthread;
} thread_job;

typedef thread_job *thread_handle;
    
typedef struct threadpool {
    thread_job *threads[MAX_NUMTHREAD];
    shared_interface *sharedp;
    int cnt;
    int current_priority;
} threadpool;

typedef threadpool *threadpool_handle;

static inline threadpool_handle threadpool_create(void *argsp);
static inline void threadpool_delete(threadpool_handle h);
static inline int threadpool_add(threadpool_handle,thread_handle);
static inline int threadpool_start(threadpool_handle h);
static inline int threadpool_join(threadpool_handle h);
static inline int thread_check_isoleted_job(thread_handle h);
static inline int thread_check_bypassing_job(thread_handle h);
static inline int thread_check_jointing_job(thread_handle h);
static inline int thread_check_source_job(thread_handle h);
static inline int thread_check_sink_job(thread_handle h);
static inline void thread_sync_init(thread_handle h);
static inline void thread_sync_cleanup(thread_handle h);
static inline int thread_extend(thread_handle lhs, thread_handle rhs);
static inline int thread_close(thread_handle lhs, thread_handle rhs);
static inline int thread_chain(thread_handle lhs, thread_handle rhs);
static inline int thread_joint(thread_handle lhs, thread_handle rhs);
static inline void thread_delete(thread_handle h);
static inline thread_handle create_base_thread(threadpool_handle h_pool, const char *name, void *(*fxn)(void *),int priority, void *param);

#define FIFO_GET(hfifo,p,magic) do {\
    switch(fifo_get(hfifo,p)){\
        case SUCCESS: \
            DASSERT(CHECK_MAGIC(magic,p), \
                ({ERR("invalid fifo element, expect %08x(%s), given %08x\n", \
                    magic,#magic,((unsigned int*)(p))[0]);\
                  goto error;}));\
            break;\
        case FIFO_EFLUSH: \
            goto cleanup;\
            break; \
        default: \
            goto error;\
            break;\
    } /*DBG("got it!\n");*/} while(0)

#define FIFO_PUT(hfifo,p) do {\
    ASSERT(fifo_put(hfifo,p)==SUCCESS,\
             goto error);\
    } while(0)

#define FIFO_INGET(h,p,magic) FIFO_GET(h->hInGet,p,magic)
#define FIFO_OUTGET(h,p,magic) FIFO_GET(h->hOutGet,p,magic)
#define FIFO_INPUT(h,p) FIFO_PUT(h->hInPut,p)
#define FIFO_OUTPUT(h,p) FIFO_PUT(h->hOutPut,p)

#define CREATE_THREAD(h_pool, callback, priority, envp) create_base_thread(h_pool,#callback,callback,priority,envp)

static inline int start_thread(thread_job *threadp);
static inline void thread_delete(thread_handle h);
static inline int _fifo_connect(fifo_handle *lhs, fifo_handle *rhs);

static inline void threadpool_delete(threadpool_handle h)
{
    int i;
    if(h){
        if(h->sharedp){
            if(h->sharedp->rv_init){
                rendezvous_delete(h->sharedp->rv_init);
            }
            if(h->sharedp->rv_cleanup){
                rendezvous_delete(h->sharedp->rv_cleanup);
            }

            gbl_delete(h->sharedp->gbl);

            FREE(h->sharedp);
        }
        
        for(i=0;i<h->cnt;i++){
            thread_delete(h->threads[i]);
        }


        FREE(h);
    }
}

static inline threadpool_handle threadpool_create(void *argsp)
{
    threadpool_handle nh = NULL;
    shared_interface *sharedp;
    rendezvous_obj  *rv_init;
    rendezvous_obj  *rv_cleanup;

    TALLOC(nh,return NULL);
    TALLOC(sharedp,goto error);
    TALLOC(rv_init,goto error);
    TALLOC(rv_cleanup,goto error);
    TALLOC(rv_cleanup,goto error);


    ASSERT(sharedp->rv_init = rendezvous_create(1), goto error);
    ASSERT(sharedp->rv_cleanup = rendezvous_create(1), goto error);
    nh->sharedp = sharedp;

    nh->sharedp->param_shared = argsp;
    ASSERT(nh->sharedp->gbl = gbl_create(), goto error);
    return nh;
error:
    threadpool_delete(nh);
    return NULL;
}

static inline int threadpool_add(threadpool_handle h,thread_handle threadp)
{
    int i;
    char tmp[MAX_THREADNAME];
    DASSERT(h,return FAILURE);
    DASSERT(threadp,return FAILURE);
    DASSERT(h->cnt < MAX_NUMTHREAD, return FAILURE);

    for(i=0;i<h->cnt;i++){
        DASSERT(h->threads[i] != threadp,return FAILURE);
                
    }

    memcpy(tmp,threadp->name,MAX_THREADNAME);
    snprintf(threadp->name, MAX_THREADNAME,"%s:%d",tmp,h->cnt);

    threadp->sharedp = h->sharedp;
    h->threads[(h->cnt)++] = threadp;

    return SUCCESS;
}

static inline int threadpool_start(threadpool_handle h)
{
    int i;
    DBG("Starting %d threads\n",h->cnt);

    h->sharedp->rv_init->count = h->cnt;
    h->sharedp->rv_cleanup->count = h->cnt;

    for(i=0;i<h->cnt;i++){
        ASSERT(start_thread(h->threads[i]) == SUCCESS, return FAILURE);
    }

    return SUCCESS;
}

static inline int threadpool_join(threadpool_handle h)
{
    void *status;
    int ret = SUCCESS;
    int i;

    for(i=0;i<h->cnt;i++){
        if(h->threads[i]->started){
            if(pthread_join(h->threads[i]->pthread, &status) == 0) {
                if(status == THREAD_FAILURE) {
                    ret = FAILURE;
                }
            } else {
                ret = FAILURE;
            }

            h->threads[i]->started = FALSE;
            DBG("join up %s thread (No.%d)\n",h->threads[i]->name,i);
        } else {
            ERR("aborted %s thread\n",h->threads[i]->name);
            ret = FAILURE;
        }
    }

    return ret;
}

static inline thread_handle create_base_thread(threadpool_handle h_pool, const char *name, void * (*fxn)(void *), int priority, void *params)
{
    thread_handle nh = NULL;
    DASSERT(name,goto error);
    DASSERT(fxn,goto error);

    TALLOC(nh,goto error);

    nh->fxn = fxn;
    nh->priority = priority;
    nh->param_priv = params;

    strncpy(nh->name, name,MAX_THREADNAME);
    
    ASSERT(threadpool_add(h_pool,nh) == SUCCESS ,goto error);

    return nh;
error:
    thread_delete(nh);
    return NULL;
}

static inline void thread_sync_init(thread_handle h)
{
    DBG("%s thread successfully initialized\n",h->name);
    rendezvous_meet(h->sharedp->rv_init);
    DBG("Entering %s thread main loop\n",h->name);
}

static inline int start_thread(thread_handle threadp)
{
    struct sched_param  schedParam;
    pthread_attr_t attr;

    DASSERT(threadp->fxn,return FAILURE);
    DASSERT(threadp->sharedp,return FAILURE);

    /* Initialize the thread attributes */
    ASSERT(pthread_attr_init(&attr) == 0, return FAILURE);

    /* Force the thread to use custom scheduling attributes */
    ASSERT(pthread_attr_setinheritsched(&attr, PTHREAD_EXPLICIT_SCHED) == 0,
           return FAILURE);

    /* Set the thread to be fifo real time scheduled */
    ASSERT(pthread_attr_setschedpolicy(&attr, SCHED_FIFO) == 0,
            return FAILURE);

    schedParam.sched_priority = threadp->priority;
    ASSERT(pthread_attr_setschedparam(&attr, &schedParam) == 0,
            return FAILURE);

    ASSERT(pthread_create(&threadp->pthread, &attr, threadp->fxn, threadp) == 0,
            return FAILURE);

    threadp->started = TRUE;

    return SUCCESS;
}

#define __THREAD_CHECK_JOB(h,v1,v2,v3,v4) do {\
        int ret = 0;\
        if(!!(h->hOutPut) == v1) ret |=  1;\
        if(!!(h->hInGet) == v2)  ret |=  2;\
        if(!!(h->hInPut) == v3)  ret |=  4;\
        if(!!(h->hOutGet) == v4) ret |=  8;\
        if(ret !=0){\
            ERR("%s failed :0x%x\n",__FUNCTION__,ret);\
            return FAILURE;\
        }\
        return SUCCESS;\
     } while(0);

static inline int thread_check_source_job(thread_handle h)
{
    __THREAD_CHECK_JOB(h,0,1,1,0);
}
static inline int thread_check_jointing_job(thread_handle h)
{
    __THREAD_CHECK_JOB(h,0,0,0,0);
}

static inline int thread_check_bypassing_job(thread_handle h)
{
    __THREAD_CHECK_JOB(h,1,0,0,1);
}

static inline int thread_check_isoleted_job(thread_handle h)
{
    __THREAD_CHECK_JOB(h,1,1,1,1);
}

static inline int thread_check_sink_job(thread_handle h)
{
    __THREAD_CHECK_JOB(h,1,1,1,0);
}

static inline void thread_sync_cleanup(thread_handle h)
{
    shared_interface *sharedp = h->sharedp;
    gbl_set_quit(sharedp->gbl);

    DBG("%s thread waiting for cleanup\n",h->name);

    if(h->hOutPut != NULL) fifo_flush(h->hOutPut);
    if(h->hInPut != NULL) fifo_flush(h->hInPut);

    
    /* Make sure the other threads aren't waiting for us */
    rendezvous_force(sharedp->rv_init);
    
    /* Meet up with other threads before cleaning up */
    rendezvous_meet(sharedp->rv_cleanup);
    
    DBG("%s thread died\n",h->name);
}

static inline void thread_delete(thread_handle h)
{
    if(h){
        if(h->started){
            ERR("BUG: deleting uninitialized thread %s\n",h->name);
        }

        FREE(h->param_priv);

        FREE(h);
    }
}

static inline int _fifo_connect(fifo_handle *lhs, fifo_handle *rhs)
{

    if(*lhs == NULL && *rhs == NULL) {
        ASSERT(*lhs = fifo_create(),return FAILURE);
        *rhs = *lhs;
    } else if(*lhs != NULL) {
        *rhs = *lhs;
    } else if(*rhs != NULL) {
        *lhs = *rhs;
    } else {
        ERR("duplicated thread connection detected\n");
        return FAILURE;
    }

    return SUCCESS;
}


static inline int thread_joint(thread_handle lhs, thread_handle rhs) 
{
    ASSERT(lhs,return FAILURE);
    ASSERT(rhs,return FAILURE);

    ASSERT(_fifo_connect(&lhs->hOutPut,&rhs->hInGet) == SUCCESS, return FAILURE);
    ASSERT(_fifo_connect(&lhs->hOutGet,&rhs->hInPut) == SUCCESS, return FAILURE);

    return SUCCESS;
}

static inline int thread_chain(thread_handle lhs, thread_handle rhs) 
{
    ASSERT(lhs,return FAILURE);
    ASSERT(rhs,return FAILURE);

    ASSERT(_fifo_connect(&lhs->hOutPut,&rhs->hInGet) == SUCCESS, return FAILURE);

    return SUCCESS;
}

static inline int thread_extend(thread_handle lhs, thread_handle rhs) 
{
    ASSERT(lhs,return FAILURE);
    ASSERT(rhs,return FAILURE);

    ASSERT(_fifo_connect(&lhs->hInPut,&rhs->hInGet) == SUCCESS, return FAILURE);

    return SUCCESS;
}

static inline int thread_close(thread_handle lhs, thread_handle rhs) 
{
    ASSERT(lhs,return FAILURE);
    ASSERT(rhs,return FAILURE);

    ASSERT(_fifo_connect(&lhs->hInPut,&rhs->hOutGet) == SUCCESS, return FAILURE);
    return SUCCESS;

}
/* prohibit thread-unsafe functions */
#define strtok  __NEVER_EVER_USE_THREAD_UNSAFE_FUNCTION_IN_MULTI_THREADED_PROGRAM_YOU_SUCKER__
#define readdir __NEVER_EVER_USE_THREAD_UNSAFE_FUNCTION_IN_MULTI_THREADED_PROGRAM_YOU_SUCKER__
#define asctime __NEVER_EVER_USE_THREAD_UNSAFE_FUNCTION_IN_MULTI_THREADED_PROGRAM_YOU_SUCKER__
#define ctime   __NEVER_EVER_USE_THREAD_UNSAFE_FUNCTION_IN_MULTI_THREADED_PROGRAM_YOU_SUCKER__
#define gmtime  __NEVER_EVER_USE_THREAD_UNSAFE_FUNCTION_IN_MULTI_THREADED_PROGRAM_YOU_SUCKER__
#define localtime  __NEVER_EVER_USE_THREAD_UNSAFE_FUNCTION_IN_MULTI_THREADED_PROGRAM_YOU_SUCKER__
#define rand       __NEVER_EVER_USE_THREAD_UNSAFE_FUNCTION_IN_MULTI_THREADED_PROGRAM_YOU_SUCKER__
#define getgrgid __NEVER_EVER_USE_THREAD_UNSAFE_FUNCTION_IN_MULTI_THREADED_PROGRAM_YOU_SUCKER__
#define getgrnam __NEVER_EVER_USE_THREAD_UNSAFE_FUNCTION_IN_MULTI_THREADED_PROGRAM_YOU_SUCKER__
#define getpwuid __NEVER_EVER_USE_THREAD_UNSAFE_FUNCTION_IN_MULTI_THREADED_PROGRAM_YOU_SUCKER__
#define getpwnam __NEVER_EVER_USE_THREAD_UNSAFE_FUNCTION_IN_MULTI_THREADED_PROGRAM_YOU_SUCKER__


#endif /* _IPC_H */