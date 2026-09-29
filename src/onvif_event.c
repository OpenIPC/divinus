#include "onvif_event.h"

#include <errno.h>
#include <pthread.h>
#include <string.h>

typedef struct {
    int id;
    time_t expires, lastSeen;
    unsigned int generation;
    int head, count;
    onvif_event_msg queue[ONVIF_EVENT_QUEUE];
} onvif_event_sub;

static onvif_event_sub subs[ONVIF_EVENT_MAX_SUBS];
static pthread_mutex_t eventMtx = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t eventCond = PTHREAD_COND_INITIALIZER;
static bool motionState = false;
static int lastId = 0, waits = 0;

time_t onvif_event_clock(void) {
    struct timespec now;
    clock_gettime(CLOCK_MONOTONIC, &now);
    return now.tv_sec;
}

static onvif_event_sub *find_sub(int id) {
    for (int i = 0; i < ONVIF_EVENT_MAX_SUBS; i++)
        if (id > 0 && subs[i].id == id) return &subs[i];
    return NULL;
}

static void push_msg(onvif_event_sub *sub, onvif_event_msg msg) {
    if (sub->count == ONVIF_EVENT_QUEUE) {
        sub->head = (sub->head + 1) % ONVIF_EVENT_QUEUE;
        sub->count--;
    }
    sub->queue[(sub->head + sub->count) % ONVIF_EVENT_QUEUE] = msg;
    sub->count++;
}

int onvif_event_subscribe(time_t now, time_t when, int seconds, time_t *expires) {
    onvif_event_sub *sub = &subs[0];

    // A free slot, or else the subscription idle for the longest time:
    // clients that reconnect rarely unsubscribe first
    pthread_mutex_lock(&eventMtx);
    for (int i = 0; i < ONVIF_EVENT_MAX_SUBS && sub->id; i++)
        if (!subs[i].id || subs[i].lastSeen < sub->lastSeen) sub = &subs[i];
    if (sub->id) pthread_cond_broadcast(&eventCond);

    memset(sub, 0, sizeof(*sub));
    int id = sub->id = ++lastId;
    sub->expires = *expires = now + seconds;
    sub->lastSeen = now;
    push_msg(sub, (onvif_event_msg){ .time = when, .initial = true, .state = motionState });
    pthread_mutex_unlock(&eventMtx);

    return id;
}

bool onvif_event_renew(int id, time_t now, int seconds, time_t *expires) {
    pthread_mutex_lock(&eventMtx);
    onvif_event_sub *sub = find_sub(id);
    if (sub) {
        sub->expires = *expires = now + seconds;
        sub->lastSeen = now;
    }
    pthread_mutex_unlock(&eventMtx);

    return sub;
}

bool onvif_event_unsubscribe(int id) {
    pthread_mutex_lock(&eventMtx);
    onvif_event_sub *sub = find_sub(id);
    if (sub) {
        sub->id = 0;
        pthread_cond_broadcast(&eventCond);
    }
    pthread_mutex_unlock(&eventMtx);

    return sub;
}

bool onvif_event_sync(int id, time_t now) {
    pthread_mutex_lock(&eventMtx);
    onvif_event_sub *sub = find_sub(id);
    if (sub) {
        push_msg(sub, (onvif_event_msg){ .time = now, .initial = true, .state = motionState });
        pthread_cond_broadcast(&eventCond);
    }
    pthread_mutex_unlock(&eventMtx);

    return sub;
}

int onvif_event_pull(int id, time_t now, int timeout_s, int limit,
    onvif_event_msg *msgs, time_t *expires) {
    struct timespec deadline;
    clock_gettime(CLOCK_REALTIME, &deadline);
    deadline.tv_sec += timeout_s;

    pthread_mutex_lock(&eventMtx);
    onvif_event_sub *sub = find_sub(id);
    unsigned int generation = 0;

    // A newer pull on the same subscription releases the one already waiting
    if (sub) {
        sub->lastSeen = now;
        generation = ++sub->generation;
        pthread_cond_broadcast(&eventCond);
    }

    if (sub && !sub->count && timeout_s > 0 && waits < ONVIF_EVENT_MAX_WAITS) {
        waits++;
        while ((sub = find_sub(id)) && !sub->count && sub->generation == generation &&
            pthread_cond_timedwait(&eventCond, &eventMtx, &deadline) != ETIMEDOUT);
        waits--;
        sub = find_sub(id);
    }

    int count = -1;
    if (sub) {
        for (count = 0; sub->count && count < limit; count++) {
            msgs[count] = sub->queue[sub->head];
            sub->head = (sub->head + 1) % ONVIF_EVENT_QUEUE;
            sub->count--;
        }
        *expires = sub->expires;
    }
    pthread_mutex_unlock(&eventMtx);

    return count;
}

void onvif_event_expire(time_t now) {
    pthread_mutex_lock(&eventMtx);
    for (int i = 0; i < ONVIF_EVENT_MAX_SUBS; i++)
        if (subs[i].id && subs[i].expires <= now) {
            subs[i].id = 0;
            pthread_cond_broadcast(&eventCond);
        }
    pthread_mutex_unlock(&eventMtx);
}

void onvif_motion_notify(bool state, time_t when) {
    pthread_mutex_lock(&eventMtx);
    motionState = state;
    for (int i = 0; i < ONVIF_EVENT_MAX_SUBS; i++)
        if (subs[i].id)
            push_msg(&subs[i], (onvif_event_msg){ .time = when, .state = state });
    pthread_cond_broadcast(&eventCond);
    pthread_mutex_unlock(&eventMtx);
}
