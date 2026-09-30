#include <pthread.h>
#include <unistd.h>

#include "check.h"
#include "onvif_event.h"

static void *second_pull(void *arg) {
    onvif_event_msg msgs[ONVIF_EVENT_QUEUE];
    time_t expires;

    usleep(200000);
    onvif_event_pull(*(int *)arg, 1000, 0, 8, msgs, &expires);
    return NULL;
}

static void *late_notify(void *arg) {
    usleep(200000);
    onvif_motion_notify(true, 1010);
    return NULL;
}

static void *waiting_pull(void *arg) {
    onvif_event_msg msgs[ONVIF_EVENT_QUEUE];
    time_t expires;

    *(int *)arg = onvif_event_pull(*(int *)arg, 4001, 1, 8, msgs, &expires);
    return NULL;
}

int main(void) {
    onvif_event_msg msgs[ONVIF_EVENT_QUEUE];
    time_t expires, now = 1000, start;
    pthread_t thread;

    int id = onvif_event_subscribe(now, now, 60, &expires);
    CHECK(id > 0 && expires == 1060);

    // The first pull gets the Initialized message with the current state
    CHECK(onvif_event_pull(id, 1000, 0, 8, msgs, &expires) == 1);
    CHECK(msgs[0].initial && !msgs[0].state && msgs[0].time == 1000);

    onvif_motion_notify(true, 1001);
    onvif_motion_notify(false, 1002);
    CHECK(onvif_event_pull(id, 1000, 0, 1, msgs, &expires) == 1);
    CHECK(!msgs[0].initial && msgs[0].state && msgs[0].time == 1001);
    CHECK(onvif_event_pull(id, 1000, 0, 8, msgs, &expires) == 1);
    CHECK(!msgs[0].state && msgs[0].time == 1002);

    // A full queue drops the oldest message
    for (int i = 0; i < ONVIF_EVENT_QUEUE + 2; i++)
        onvif_motion_notify(i % 2 == 0, 2000 + i);
    CHECK(onvif_event_pull(id, 1000, 0, 8, msgs, &expires) == ONVIF_EVENT_QUEUE);
    CHECK(msgs[0].time == 2002 && msgs[ONVIF_EVENT_QUEUE - 1].time == 2009);

    // An empty pull waits for its timeout
    start = time(NULL);
    CHECK(onvif_event_pull(id, 1000, 1, 8, msgs, &expires) == 0);
    CHECK(time(NULL) - start >= 1);

    // A notification wakes a waiting pull right away
    pthread_create(&thread, NULL, late_notify, NULL);
    start = time(NULL);
    CHECK(onvif_event_pull(id, 1000, 5, 8, msgs, &expires) == 1 && msgs[0].time == 1010);
    CHECK(time(NULL) - start < 2);
    pthread_join(thread, NULL);

    // A second pull on the same subscription releases the first one empty
    pthread_create(&thread, NULL, second_pull, &id);
    start = time(NULL);
    CHECK(onvif_event_pull(id, 1000, 5, 8, msgs, &expires) == 0);
    CHECK(time(NULL) - start < 2);
    pthread_join(thread, NULL);

    // Renew, sync, unsubscribe
    CHECK(onvif_event_renew(id, 1100, 30, &expires) && expires == 1130);
    CHECK(onvif_event_sync(id, 1101));
    CHECK(onvif_event_pull(id, 1000, 0, 8, msgs, &expires) == 1 && msgs[0].initial && msgs[0].state);
    CHECK(onvif_event_unsubscribe(id));
    CHECK(onvif_event_pull(id, 1000, 0, 8, msgs, &expires) == -1);
    CHECK(!onvif_event_renew(id, 1100, 30, &expires));

    // Expired subscriptions free their slot
    int ids[ONVIF_EVENT_MAX_SUBS];
    for (int i = 0; i < ONVIF_EVENT_MAX_SUBS; i++)
        CHECK((ids[i] = onvif_event_subscribe(3000, 3000, 10 + i, &expires)) > 0);
    onvif_event_expire(3010);
    CHECK(onvif_event_pull(ids[0], 3010, 0, 8, msgs, &expires) == -1);
    CHECK(onvif_event_pull(ids[1], 3010, 0, 8, msgs, &expires) == 1);

    // When full, a new subscription replaces the one idle for the longest time
    CHECK((ids[0] = onvif_event_subscribe(3010, 3010, 60, &expires)) > 0);
    CHECK(onvif_event_pull(ids[2], 3011, 0, 8, msgs, &expires) == 1);
    CHECK(onvif_event_pull(ids[3], 3012, 0, 8, msgs, &expires) == 1);
    CHECK(onvif_event_pull(ids[0], 3013, 0, 8, msgs, &expires) == 1);
    int newest = onvif_event_subscribe(3014, 3014, 60, &expires);
    CHECK(newest > 0);
    CHECK(onvif_event_pull(ids[1], 3014, 0, 8, msgs, &expires) == -1);
    CHECK(onvif_event_pull(ids[2], 3014, 0, 8, msgs, &expires) == 0);
    CHECK(onvif_event_pull(newest, 3014, 0, 8, msgs, &expires) == 1);

    // A subscription with a pull waiting is not idle, even if seen the longest ago
    for (int i = 0; i < ONVIF_EVENT_MAX_SUBS; i++) {
        CHECK((ids[i] = onvif_event_subscribe(4000, 4000, 60, &expires)) > 0);
        CHECK(onvif_event_pull(ids[i], 4000, 0, 8, msgs, &expires) == 1);
    }
    int waiter = ids[0];
    pthread_create(&thread, NULL, waiting_pull, &waiter);
    usleep(200000);
    for (int i = 1; i < ONVIF_EVENT_MAX_SUBS; i++)
        CHECK(onvif_event_pull(ids[i], 4002, 0, 8, msgs, &expires) == 0);
    CHECK(onvif_event_subscribe(4003, 4003, 60, &expires) > 0);
    pthread_join(thread, NULL);
    CHECK(waiter == 0);
    CHECK(onvif_event_pull(ids[1], 4003, 0, 8, msgs, &expires) == -1);

    CHECK_DONE();
}
