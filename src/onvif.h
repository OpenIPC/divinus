#pragma once

#include <net/if.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <sys/select.h>

#include "app_config.h"
#include "network.h"
#include "onvif_event.h"
#include "soap.h"

int start_onvif(void);
void stop_onvif(void);

void *onvif_thread();

char* onvif_extract_soap_action(const char* soap_data);
bool onvif_is_preauth_action(const char *action);
bool onvif_validate_soap_auth(const char *soap_data);

void onvif_respond_fault(char *response, int *respLen, bool sender,
    const char *subcode, const char *reason);

void onvif_respond_capabilities(char *response, int *respLen);
void onvif_respond_deviceinfo(char *response, int *respLen);
void onvif_respond_mediaprofiles(char *response, int *respLen);
void onvif_respond_snapshot(char *response, int *respLen);
void onvif_respond_stream(char *response, int *respLen);
void onvif_respond_systemtime(char *response, int *respLen);
void onvif_respond_videosources(char *response, int *respLen);

typedef struct {
    int id, timeout, limit;
    char messageId[128];
} onvif_pull_req;

void onvif_respond_services(char *response, int *respLen);
void onvif_respond_eventcaps(char *response, int *respLen, const char *request);
void onvif_respond_eventprops(char *response, int *respLen, const char *request);
void onvif_respond_pullpoint(char *response, int *respLen, const char *request);
void onvif_pull_parse(const char *request, int id, onvif_pull_req *pull);
void onvif_respond_pullmessages(char *response, int *respLen, const onvif_pull_req *pull);
void onvif_respond_renew(char *response, int *respLen, int id, const char *request);
void onvif_respond_unsubscribe(char *response, int *respLen, int id, const char *request);
void onvif_respond_syncpoint(char *response, int *respLen, int id, const char *request);
