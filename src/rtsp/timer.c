#ifndef _RTSP_TIMER_H
#define _RTSP_TIMER_H
#include <sys/time.h>
#include "common.h"

typedef struct timekeeper_object {
    struct timespec tv1;
    struct timespec tv2;
	long     sum;
	long     max;
	long     min;
	unsigned int cnt;
} timekeeper_object;


typedef timekeeper_object *timekeeper_handle;

static inline timekeeper_handle timekeeper_create(void);
static inline void timekeeper_delete(timekeeper_handle h);
static inline void timekeeper_print(timekeeper_handle h,const char *reporter);
static inline void timekeeper_start(timekeeper_handle h);
static inline void timekeeper_stop(timekeeper_handle h);

static inline void
timesub(timekeeper_handle h);

static inline void timekeeper_delete(timekeeper_handle h)
{
    FREE(h);
}

static inline timekeeper_handle timekeeper_create()
{
    timekeeper_handle nh;
    TALLOC(nh,return NULL);
	nh->sum = 0;
	nh->max = 0;
	nh->min = 0x7fffffff;
	nh->cnt = 0;
    return nh;
}

static inline void timekeeper_print(timekeeper_handle h,const char *reporter)
{
    double avg = 0;
    
    ASSERT(h,ERR("timekeeper_handle is null\n"));
   
    avg = (double)h->sum / (double)h->cnt / 1000.0;
	printf("%s spends: min[%ldus], max[%ldus], avg[%.3fms], in %u times\n", reporter,h->min, h->max, avg,h->cnt);
}

static inline void timekeeper_start(timekeeper_handle h)
{
    ASSERT(h,ERR("timekeeper_handle is null\n"));

    clock_gettime(CLOCK_REALTIME, &h->tv1);
    //gettimeofday(&h->tv1,NULL);
}

/*
void timekeeper_get_sec(timekeeper_handle h)
{
    clock_gettime(CLOCK_REALTIME_HR, h->tv1);
}
*/

static inline void timekeeper_stop(timekeeper_handle h)
{
    ASSERT(h,ERR("timekeeper_handle is null\n"));

    clock_gettime(CLOCK_REALTIME, &h->tv2);
    //gettimeofday(&h->tv2,NULL);

    timesub(h);
}


static inline void
timesub(timekeeper_handle h)
{
	long            sec;
	long            nsec;

	long            usec;
	long            avg = 0;

	sec = h->tv2.tv_sec - h->tv1.tv_sec;
	nsec = h->tv2.tv_nsec - h->tv1.tv_nsec;

	if (nsec < 0) {
		sec--;
		nsec += 1000000000;
	}

	usec = nsec / 1000;
    
	if (h->max < usec)
		h->max = usec;
	if (h->min > usec)
		h->min = usec;
	h->sum += usec;
	h->cnt++;
	avg = h->sum / h->cnt;
}
#endif