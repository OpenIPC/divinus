#pragma once

#include <stdbool.h>
#include <time.h>

#define ONVIF_EVENT_MAX_SUBS 4
#define ONVIF_EVENT_MAX_WAITS 4
#define ONVIF_EVENT_QUEUE 8

typedef struct {
    time_t time;
    bool initial;
    bool state;
} onvif_event_msg;

int onvif_event_subscribe(time_t now, int seconds, time_t *expires);
bool onvif_event_renew(int id, time_t now, int seconds, time_t *expires);
bool onvif_event_unsubscribe(int id);
bool onvif_event_sync(int id, time_t now);
int onvif_event_pull(int id, int timeout_s, int limit,
    onvif_event_msg *msgs, time_t *expires);
void onvif_event_expire(time_t now);
void onvif_motion_notify(bool state, time_t when);
