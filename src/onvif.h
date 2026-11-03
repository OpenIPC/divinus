#pragma once

#include <net/if.h>
#include <stdio.h>
#include <string.h>
#include <sys/select.h>

#include "app_config.h"
#include "network.h"

int start_onvif(void);
void stop_onvif(void);

void *onvif_thread();

char* onvif_extract_soap_action(const char* soap_data);
bool onvif_is_preauth_action(const char *action);
bool onvif_validate_soap_auth(const char *soap_data);

void onvif_respond_capabilities(char *response, int *respLen);
void onvif_respond_deviceinfo(char *response, int *respLen);
void onvif_respond_mediaprofiles(char *response, int *respLen);
void onvif_respond_snapshot(char *response, int *respLen);
void onvif_respond_stream(char *response, int *respLen, const char *soap);
void onvif_respond_systemtime(char *response, int *respLen);
void onvif_respond_settime(char *response, int *respLen, const char *soap);
void onvif_respond_videosources(char *response, int *respLen);
void onvif_respond_services(char *response, int *respLen);
void onvif_respond_scopes(char *response, int *respLen);
void onvif_respond_network(char *response, int *respLen);
void onvif_respond_hostname(char *response, int *respLen);
void onvif_respond_dns(char *response, int *respLen);
void onvif_respond_protocols(char *response, int *respLen);
void onvif_respond_device_caps(char *response, int *respLen);
void onvif_respond_media_caps(char *response, int *respLen);
void onvif_respond_profile(char *response, int *respLen, const char *soap);
void onvif_respond_encoders(char *response, int *respLen);
void onvif_respond_source_configs(char *response, int *respLen);
void onvif_respond_gateway(char *response, int *respLen);
void onvif_respond_encoder_options(char *response, int *respLen);
int onvif_dispatch_extra(const char *path, const char *action,
    char *response, int *respLen, const char *soap);