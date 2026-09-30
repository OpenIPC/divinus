#include "onvif.h"

IMPORT_STR(.rodata, "../res/onvif/capabilities.xml", capabilitiesxml);
extern const char capabilitiesxml[];
IMPORT_STR(.rodata, "../res/onvif/deviceinfo.xml", deviceinfoxml);
extern const char deviceinfoxml[];
IMPORT_STR(.rodata, "../res/onvif/fault.xml", faultxml);
extern const char faultxml[];
IMPORT_STR(.rodata, "../res/onvif/discovery.xml", discoveryxml);
extern const char discoveryxml[];
IMPORT_STR(.rodata, "../res/onvif/mediaprofile.xml", mediaprofilexml);
extern const char mediaprofilexml[];
IMPORT_STR(.rodata, "../res/onvif/mediaprofiles.xml", mediaprofilesxml);
extern const char mediaprofilesxml[];
IMPORT_STR(.rodata, "../res/onvif/snapshot.xml", snapshotxml);
extern const char snapshotxml[];
IMPORT_STR(.rodata, "../res/onvif/stream.xml", streamxml);
extern const char streamxml[];
IMPORT_STR(.rodata, "../res/onvif/systemtime.xml", systemtimexml);
extern const char systemtimexml[];
IMPORT_STR(.rodata, "../res/onvif/videosources.xml", videosourcesxml);
extern const char videosourcesxml[];
IMPORT_STR(.rodata, "../res/onvif/event.xml", eventxml);
extern const char eventxml[];
IMPORT_STR(.rodata, "../res/onvif/eventcaps.xml", eventcapsxml);
extern const char eventcapsxml[];
IMPORT_STR(.rodata, "../res/onvif/eventprops.xml", eventpropsxml);
extern const char eventpropsxml[];
IMPORT_STR(.rodata, "../res/onvif/notification.xml", notificationxml);
extern const char notificationxml[];
IMPORT_STR(.rodata, "../res/onvif/pullmessages.xml", pullmessagesxml);
extern const char pullmessagesxml[];
IMPORT_STR(.rodata, "../res/onvif/pullpoint.xml", pullpointxml);
extern const char pullpointxml[];
IMPORT_STR(.rodata, "../res/onvif/renew.xml", renewxml);
extern const char renewxml[];
IMPORT_STR(.rodata, "../res/onvif/services.xml", servicesxml);
extern const char servicesxml[];
IMPORT_STR(.rodata, "../res/onvif/syncpoint.xml", syncpointxml);
extern const char syncpointxml[];
IMPORT_STR(.rodata, "../res/onvif/unsubscribe.xml", unsubscribexml);
extern const char unsubscribexml[];

static void onvif_reply(char *response, int *respLen, const char *status, const char *xml, ...) {
    int maxLen = *respLen;
    int headerLen = snprintf(response, maxLen,
        "HTTP/1.1 %s\r\n"
        "Content-Type: application/soap+xml; charset=utf-8\r\n"
        "Connection: close\r\n"
        "\r\n", status);

    va_list args;
    va_start(args, xml);
    int bodyLen = vsnprintf(response + headerLen, maxLen - headerLen, xml, args);
    va_end(args);

    if (bodyLen >= maxLen - headerLen) {
        HAL_WARNING("onvif", "Response truncated to %d bytes!\n", maxLen - 1);
        bodyLen = maxLen - headerLen - 1;
    }
    *respLen = headerLen + bodyLen;
}

void onvif_respond_fault(char *response, int *respLen, bool sender,
    const char *subcode, const char *reason) {
    onvif_reply(response, respLen, sender ? "400 Bad Request" : "500 Internal Server Error",
        faultxml, sender ? "Sender" : "Receiver", subcode, reason);
}

extern NetInfo netinfo;
pthread_t onvifPid = 0;

int start_onvif(void) {
    pthread_attr_t thread_attr;
    pthread_attr_init(&thread_attr);
    size_t stacksize;
    pthread_attr_getstacksize(&thread_attr, &stacksize);
    size_t new_stacksize = 16 * 1024;
    if (pthread_attr_setstacksize(&thread_attr, new_stacksize))
        HAL_DANGER("onvif", "Can't set stack size %zu\n", new_stacksize);
    pthread_create(&onvifPid, &thread_attr, (void *(*)(void *))onvif_thread, NULL);
    if (pthread_attr_setstacksize(&thread_attr, stacksize))
        HAL_DANGER("onvif", "Can't set stack size %zu\n", stacksize);
    pthread_attr_destroy(&thread_attr);
}

void stop_onvif(void) {
    pthread_join(onvifPid, NULL);
}

void *onvif_thread(void) {
    struct ifaddrs *ifaddr, *ifa;
    char request[4096], response[4096];
    int servfd, reqLen;
    struct sockaddr_in servaddr, clntaddr;
    socklen_t clntsz;

    if ((servfd = socket(AF_INET, SOCK_DGRAM, 0)) == -1) {
        HAL_DANGER("onvif", "Failed to create socket!\n");
        return (void*)EXIT_FAILURE;
    }

    memset(&servaddr, 0, sizeof(servaddr));
    servaddr.sin_family = AF_INET;
    servaddr.sin_addr.s_addr = INADDR_ANY;
    servaddr.sin_port = htons(3702);

    struct ip_mreq group = {
        .imr_multiaddr.s_addr = inet_addr("239.255.255.250"),
        .imr_interface.s_addr = inet_addr(netinfo.ipaddr[0])
    };

    if (bind(servfd, (struct sockaddr *)&servaddr, sizeof(servaddr)) == -1) {
        HAL_DANGER("onvif", "Failed to bind socket!\n");
        return (void*)EXIT_FAILURE;
    }

    if (setsockopt(servfd, IPPROTO_IP, IP_ADD_MEMBERSHIP, (char *)&group, sizeof(group)) < 0){
        close(servfd);
        return (void*)EXIT_FAILURE;
    }

    while (keepRunning) {
        fd_set readfds;
        FD_ZERO(&readfds);
        FD_SET(servfd, &readfds);

        struct timeval tv = { .tv_sec = 1, .tv_usec = 0 };
        int ret = select(servfd + 1, &readfds, NULL, NULL, &tv);
        if (ret < 0) {
            HAL_DANGER("onvif", "Polling using select failed: %s\n", strerror(errno));
            continue;
        } else if (!ret) continue;

        clntsz = sizeof(clntaddr);
        if ((reqLen = recvfrom(servfd, request, sizeof(request), 0, (struct sockaddr *)&clntaddr, &clntsz)) < 0)
            continue;

        request[reqLen] = '\0';
#ifdef DEBUG_ONVIF
        HAL_INFO("onvif", "Received message: %s\n", msgbuf);
#endif

        if (!CONTAINS(request, "http://schemas.xmlsoap.org/ws/2005/04/discovery/Probe"))
            continue;

        char device_name[64], device_uuid[64], device_url[128], msgid[100];
        {
            char uuid[37];
            uuid_generate(uuid);
            snprintf(device_uuid, sizeof(device_uuid), "urn:uuid:%s", uuid);
        }
        snprintf(device_name, sizeof(device_name), "Divinus");
        snprintf(device_url, sizeof(device_url), "http://%s:%d/onvif/device_service",
            netinfo.ipaddr[0], app_config.web_port);
    
        char *msgid_init = strstr(request, "MessageID>");
        if (msgid_init) {
            msgid_init += 10;
            char *msgid_end = strstr(msgid_init, "<");
            if (msgid_end && (msgid_end - msgid_init) < sizeof(msgid)) {
                strncpy(msgid, msgid_init, msgid_end - msgid_init);
                msgid[msgid_end - msgid_init] = '\0';
            }
        }
        
        int respLen = snprintf(response, sizeof(response), discoveryxml,
            device_uuid, msgid, device_uuid, device_name, device_url);
    
        HAL_INFO("onvif", "Sending discovery response to %s:%d\n", 
                 inet_ntoa(clntaddr.sin_addr), ntohs(clntaddr.sin_port));
    
        if (sendto(servfd, response, strlen(response), 0, (struct sockaddr *)&clntaddr, clntsz) < 0)
            HAL_WARNING("onvif", "Failed to send discovery response: %s\n", strerror(errno));
    }

    close(servfd);
    return (void*)EXIT_SUCCESS;
}

char* onvif_extract_soap_action(const char* soap_data) {
    static char action[128];

    return soap_action(soap_data, action, sizeof(action)) ? action : NULL;
}

bool onvif_is_preauth_action(const char *action) {
    if (!action || EMPTY(action))
        return false;

    // PRE_AUTH access class in ONVIF Core
    if (EQUALS(action, "GetCapabilities") ||
        EQUALS(action, "GetServiceCapabilities") ||
        EQUALS(action, "GetServices") ||
        EQUALS(action, "GetSystemDateAndTime"))
        return true;

    return false;
}

bool onvif_validate_soap_auth(const char *soap_data) {
    char created[64], nonce[64], pass[64], user[64];
    const char *token = soap_tag(soap_data, "UsernameToken");

    if (!token ||
        !soap_tag_text(token, "Username", user, sizeof(user)) ||
        !soap_tag_text(token, "Password", pass, sizeof(pass))) return false;

    if (!EQUALS(user, app_config.onvif_auth_user)) {
        HAL_WARNING("onvif", "Invalid username: %s\n", user);
        return false;
    }

    bool valid;
    if (soap_tag_attr_has(token, "Password", "PasswordDigest"))
        valid = soap_tag_text(token, "Nonce", nonce, sizeof(nonce)) &&
            soap_tag_text(token, "Created", created, sizeof(created)) &&
            soap_digest_valid(nonce, created, app_config.onvif_auth_pass, pass);
    else
        valid = EQUALS(pass, app_config.onvif_auth_pass);

    if (valid)
        HAL_INFO("onvif", "Valid credentials provided!\n");
    else
        HAL_WARNING("onvif", "Invalid credentials provided!\n");
    return valid;
}

void onvif_respond_capabilities(char *response, int *respLen) {
    if (!response || !respLen) return;

    onvif_reply(response, respLen, "200 OK", capabilitiesxml,
        netinfo.ipaddr[0], app_config.web_port,    // Analytics
        netinfo.ipaddr[0], app_config.web_port,    // Device
        netinfo.ipaddr[0], app_config.web_port,    // Events
        netinfo.ipaddr[0], app_config.web_port,    // Imaging
        netinfo.ipaddr[0], app_config.web_port,    // Media
        netinfo.ipaddr[0], app_config.web_port);   // PTZ
}

void onvif_respond_deviceinfo(char *response, int *respLen) {
    if (!response || !respLen) return;

    onvif_reply(response, respLen, "200 OK", deviceinfoxml,
        "OpenIPC", "IP Camera", "1.0", "To be replaced", chip);
}

void onvif_respond_mediaprofiles(char *response, int *respLen) {
    if (!response || !respLen) return;

    char profile[4096];
    char profileCnt = 0;
    int profileLen = 0;

    if (app_config.mp4_enable) {
        profileLen += sprintf(&profile[profileLen], mediaprofilexml,
            "MainStream", "profile_1",
            profileCnt + 1, profileCnt + 1,
            app_config.mp4_height, app_config.mp4_width,
            profileCnt + 1, profileCnt + 1,
            app_config.mp4_codecH265 ? "H265" : "H264",
            app_config.mp4_width, app_config.mp4_height,
            app_config.mp4_fps, app_config.mp4_bitrate);
        profileCnt++;
    }

    if (app_config.mjpeg_enable) {
        profileLen += sprintf(&profile[profileLen], mediaprofilexml,
            "SubStream", "profile_2",
            profileCnt + 1, profileCnt + 1,
            app_config.mjpeg_height, app_config.mjpeg_width,
            profileCnt + 1, profileCnt + 1,
            "JPEG", app_config.mjpeg_width, app_config.mjpeg_height,
            app_config.mjpeg_fps, app_config.mjpeg_bitrate);
        profileCnt++;
    }

    onvif_reply(response, respLen, "200 OK", mediaprofilesxml,
        profile);
}

void onvif_respond_snapshot(char *response, int *respLen) {
    if (!response || !respLen) return;

    char snapshot_url[256];

    if (app_config.web_enable_auth && 
        *app_config.web_auth_user && *app_config.web_auth_pass) {
        char user[96], pass[96];
        escape_url(user, app_config.web_auth_user, sizeof(user));
        escape_url(pass, app_config.web_auth_pass, sizeof(pass));
        snprintf(snapshot_url, sizeof(snapshot_url), "http://%s:%s@%s:%d/image.jpg",
            user, pass, netinfo.ipaddr[0], app_config.web_port);
    } else
        snprintf(snapshot_url, sizeof(snapshot_url), "http://%s:%d/image.jpg",
            netinfo.ipaddr[0], app_config.web_port);

    onvif_reply(response, respLen, "200 OK", snapshotxml,
        snapshot_url);
}

void onvif_respond_stream(char *response, int *respLen) {
    if (!response || !respLen) return;

    char stream_url[256];

    if (app_config.rtsp_enable_auth && 
        *app_config.rtsp_auth_user && *app_config.rtsp_auth_pass) {
        char user[96], pass[96];
        escape_url(user, app_config.rtsp_auth_user, sizeof(user));
        escape_url(pass, app_config.rtsp_auth_pass, sizeof(pass));
        snprintf(stream_url, sizeof(stream_url), "rtsp://%s:%s@%s:%d/",
            user, pass, netinfo.ipaddr[0], app_config.rtsp_port);
    } else
        snprintf(stream_url, sizeof(stream_url), "rtsp://%s:%d/",
            netinfo.ipaddr[0], app_config.rtsp_port);

    onvif_reply(response, respLen, "200 OK", streamxml,
        stream_url);
}

void onvif_respond_systemtime(char *response, int *respLen) {
    if (!response || !respLen) return;

    time_t now;
    struct tm *tm_info;

    time(&now);
    tm_info = gmtime(&now);

    onvif_reply(response, respLen, "200 OK", systemtimexml,
        tm_info->tm_hour, tm_info->tm_min, tm_info->tm_sec,
        tm_info->tm_year + 1900, tm_info->tm_mon + 1, tm_info->tm_mday);
}

void onvif_respond_videosources(char *response, int *respLen) {
    if (!response || !respLen) return;

    int width = app_config.mp4_enable ?
        app_config.mp4_width : app_config.mjpeg_width;
    int height = app_config.mp4_enable ?
        app_config.mp4_height : app_config.mjpeg_height;
    int framerate = app_config.mp4_enable ?
        app_config.mp4_fps : app_config.mjpeg_fps;

    onvif_reply(response, respLen, "200 OK", videosourcesxml,
        framerate, width, height);
}

#define ONVIF_EVENTS_WSDL "http://www.onvif.org/ver10/events/wsdl/"
#define ONVIF_WSN_BW2 "http://docs.oasis-open.org/wsn/bw-2/"
#define ONVIF_SOURCE_TOKEN "VideoSource_1"
#define ONVIF_SUB_DEFAULT_S 3600
#define ONVIF_SUB_MAX_S 86400
#define ONVIF_PULL_MAX_S 60

static void onvif_message_id(const char *request, char *messageId, size_t size) {
    if (!soap_tag_text(request, "MessageID", messageId, size))
        *messageId = '\0';
}

static void onvif_reply_event(char *response, int *respLen, const char *messageId,
    const char *action, const char *body) {
    char relates[176] = "";

    if (*messageId)
        snprintf(relates, sizeof(relates), "\n    <wsa:RelatesTo>%s</wsa:RelatesTo>", messageId);
    onvif_reply(response, respLen, "200 OK", eventxml, action, relates, body);
}

static void onvif_respond_unknown_sub(char *response, int *respLen) {
    onvif_respond_fault(response, respLen, true, "wsrf-rw:ResourceUnknownFault",
        "Unknown or expired subscription");
}

void onvif_respond_services(char *response, int *respLen) {
    onvif_reply(response, respLen, "200 OK", servicesxml,
        netinfo.ipaddr[0], app_config.web_port,
        netinfo.ipaddr[0], app_config.web_port,
        netinfo.ipaddr[0], app_config.web_port);
}

void onvif_respond_eventcaps(char *response, int *respLen, const char *request) {
    char messageId[128];

    onvif_message_id(request, messageId, sizeof(messageId));
    onvif_reply_event(response, respLen, messageId,
        ONVIF_EVENTS_WSDL "EventPortType/GetServiceCapabilitiesResponse", eventcapsxml);
}

void onvif_respond_eventprops(char *response, int *respLen, const char *request) {
    char messageId[128];

    onvif_message_id(request, messageId, sizeof(messageId));
    onvif_reply_event(response, respLen, messageId,
        ONVIF_EVENTS_WSDL "EventPortType/GetEventPropertiesResponse", eventpropsxml);
}

void onvif_respond_pullpoint(char *response, int *respLen, const char *request) {
    char body[1024], messageId[128], current[32], termination[32];
    time_t now = time(NULL), expires;
    int seconds = soap_term_seconds(request, "InitialTerminationTime", now,
        ONVIF_SUB_DEFAULT_S, ONVIF_SUB_MAX_S);

    int id = onvif_event_subscribe(onvif_event_clock(), now, seconds, &expires);
    HAL_INFO("onvif", "Subscription #%d created for %d seconds\n", id, seconds);

    soap_datetime_format(now, current, sizeof(current));
    soap_datetime_format(now + seconds, termination, sizeof(termination));
    snprintf(body, sizeof(body), pullpointxml,
        netinfo.ipaddr[0], app_config.web_port, id, current, termination);
    onvif_message_id(request, messageId, sizeof(messageId));
    onvif_reply_event(response, respLen, messageId,
        ONVIF_EVENTS_WSDL "EventPortType/CreatePullPointSubscriptionResponse", body);
}

void onvif_pull_parse(const char *request, int id, onvif_pull_req *pull) {
    char text[32];

    pull->id = id;
    pull->timeout = soap_tag_text(request, "Timeout", text, sizeof(text)) ?
        soap_duration(text) : 0;
    if (pull->timeout < 0) pull->timeout = 0;
    if (pull->timeout > ONVIF_PULL_MAX_S) pull->timeout = ONVIF_PULL_MAX_S;
    pull->limit = soap_tag_text(request, "MessageLimit", text, sizeof(text)) ?
        atoi(text) : ONVIF_EVENT_QUEUE;
    if (pull->limit < 1) pull->limit = 1;
    if (pull->limit > ONVIF_EVENT_QUEUE) pull->limit = ONVIF_EVENT_QUEUE;
    onvif_message_id(request, pull->messageId, sizeof(pull->messageId));
}

void onvif_respond_pullmessages(char *response, int *respLen, const onvif_pull_req *pull) {
    onvif_event_msg msgs[ONVIF_EVENT_QUEUE];
    char body[6656], notes[5632] = "", current[32], termination[32], when[32];
    time_t expires;
    int notesLen = 0;

    int count = onvif_event_pull(pull->id, onvif_event_clock(), pull->timeout, pull->limit,
        msgs, &expires);
    if (count < 0) {
        onvif_respond_unknown_sub(response, respLen);
        return;
    }

    for (int i = 0; i < count && notesLen < sizeof(notes); i++) {
        soap_datetime_format(msgs[i].time, when, sizeof(when));
        notesLen += snprintf(notes + notesLen, sizeof(notes) - notesLen, notificationxml,
            when, msgs[i].initial ? "Initialized" : "Changed", ONVIF_SOURCE_TOKEN,
            msgs[i].state ? "true" : "false");
    }

    time_t now = time(NULL);
    soap_datetime_format(now, current, sizeof(current));
    soap_datetime_format(now + (expires - onvif_event_clock()), termination, sizeof(termination));
    snprintf(body, sizeof(body), pullmessagesxml, current, termination, notes);
    onvif_reply_event(response, respLen, pull->messageId,
        ONVIF_EVENTS_WSDL "PullPointSubscription/PullMessagesResponse", body);
}

void onvif_respond_renew(char *response, int *respLen, int id, const char *request) {
    char body[512], messageId[128], current[32], termination[32];
    time_t now = time(NULL), expires;
    int seconds = soap_term_seconds(request, "TerminationTime", now,
        ONVIF_SUB_DEFAULT_S, ONVIF_SUB_MAX_S);

    if (!onvif_event_renew(id, onvif_event_clock(), seconds, &expires)) {
        onvif_respond_unknown_sub(response, respLen);
        return;
    }

    soap_datetime_format(now, current, sizeof(current));
    soap_datetime_format(now + seconds, termination, sizeof(termination));
    snprintf(body, sizeof(body), renewxml, termination, current);
    onvif_message_id(request, messageId, sizeof(messageId));
    onvif_reply_event(response, respLen, messageId,
        ONVIF_WSN_BW2 "SubscriptionManager/RenewResponse", body);
}

void onvif_respond_unsubscribe(char *response, int *respLen, int id, const char *request) {
    char messageId[128];

    if (!onvif_event_unsubscribe(id)) {
        onvif_respond_unknown_sub(response, respLen);
        return;
    }
    HAL_INFO("onvif", "Subscription #%d removed\n", id);

    onvif_message_id(request, messageId, sizeof(messageId));
    onvif_reply_event(response, respLen, messageId,
        ONVIF_WSN_BW2 "SubscriptionManager/UnsubscribeResponse", unsubscribexml);
}

void onvif_respond_syncpoint(char *response, int *respLen, int id, const char *request) {
    char messageId[128];

    if (!onvif_event_sync(id, time(NULL))) {
        onvif_respond_unknown_sub(response, respLen);
        return;
    }

    onvif_message_id(request, messageId, sizeof(messageId));
    onvif_reply_event(response, respLen, messageId,
        ONVIF_EVENTS_WSDL "PullPointSubscription/SetSynchronizationPointResponse", syncpointxml);
}
