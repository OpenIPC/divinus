#include "onvif.h"

IMPORT_STR(.rodata, "../res/onvif/capabilities.xml", capabilitiesxml);
extern const char capabilitiesxml[];
IMPORT_STR(.rodata, "../res/onvif/deviceinfo.xml", deviceinfoxml);
extern const char deviceinfoxml[];
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

const char onvifgood[] = "HTTP/1.1 200 OK\r\n" \
                         "Content-Type: application/soap+xml; charset=utf-8\r\n" \
                         "Connection: close\r\n" \
                         "\r\n";

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
    char *action_start = NULL;
    
    char *body_start = strstr(soap_data, "Body");
    if (!body_start) return NULL;
    
    body_start = strchr(body_start, '>');
    if (!body_start) return NULL;
    body_start++;

    while (*body_start && isspace(*body_start)) body_start++;
    
    if (*body_start != '<') return NULL;
    body_start++;
    
    char *action_end = strchr(body_start, ' ');
    if (!action_end) action_end = strchr(body_start, '>');
    if (!action_end) return NULL;
    
    int action_len = action_end - body_start;
    if (action_len >= sizeof(action)) action_len = sizeof(action) - 1;
    
    strncpy(action, body_start, action_len);
    action[action_len] = '\0';
    
    return action;
}

bool onvif_validate_soap_auth(const char *soap_data) {
    const char *created_tag = "Created", *digest_tag = "PasswordDigest", *nonce_tag = "<Nonce", 
        *pass_tag = "<Password", *type_attr = "Type=\"", *user_tag = "<Username>";
    char *pos, *end, *start;
    char digest = 0, created[64], nonce[64], pass[64], user[64];

    if (!(start = strstr(soap_data, user_tag)) ||
        !(start += strlen(user_tag))) return false;
    if (!(end = strstr(start, "</Username>"))) return false;
    memcpy(user, start, end - start);
    user[end - start] = '\0';

    if (!EQUALS(user, app_config.onvif_auth_user)) {
        HAL_WARNING("onvif", "Invalid username: %s\n", user);
        return false;
    }

    if (!(start = strstr(soap_data, pass_tag)) ||
        !(start += strlen(pass_tag))) return false;
    if ((pos = strstr(start, type_attr)) < (start = strchr(start, '>')) &&
        (pos += strlen(type_attr)) && strstr(pos, digest_tag)) digest = 1;
    if (!(end = strstr(start, "</Password>"))) return false;
    memcpy(pass, ++start, end - start);
    pass[end - start] = '\0';

    if (digest) {
        char digest_comp[SHA1_DIGEST_SIZE] = {0}, nonce_dec[64], pass_dec[64];
        sha1_context ctx;

        if (!(start = strstr(soap_data, nonce_tag)) ||
            !(start = strchr(start, '>'))) return false;
        if (!(end = strstr(++start, "</Nonce>"))) return false;
        memcpy(nonce, start, end - start);
        nonce[end - start] = '\0';

        if (!(start = strstr(soap_data, created_tag)) ||
            !(start = strchr(start, '>'))) return false;
        if (!(end = strstr(++start, "</Created>"))) return false;
        memcpy(created, start, end - start);
        created[end - start] = '\0';

        int nonce_len = base64_decode(nonce_dec, nonce, sizeof(nonce_dec));
        if (nonce_len < 0) return false;

        sha1_init(&ctx);
        sha1_update(&ctx, (unsigned char *)nonce_dec, nonce_len - 1);
        sha1_update(&ctx, (unsigned char *)created, strlen(created));
        sha1_update(&ctx, (unsigned char *)app_config.onvif_auth_pass, strlen(app_config.onvif_auth_pass));
        sha1_final(digest_comp, &ctx);

        int pass_len = base64_encode(pass_dec, digest_comp, SHA1_DIGEST_SIZE);
        if (pass_len < 0) return false;
        pass_dec[pass_len] = '\0';

        bool valid = !memcmp(pass, pass_dec, pass_len);
        if (valid)
            HAL_INFO("onvif", "Valid password digest!\n");
        else
            HAL_WARNING("onvif", "Invalid password digest!\n");
        return valid;
    } else {
        bool valid = EQUALS(pass, app_config.onvif_auth_pass);
        if (valid)
            HAL_INFO("onvif", "Valid password provided!\n");
        else
            HAL_WARNING("onvif", "Invalid password provided!\n");
        return valid;
    }
}

void onvif_respond_capabilities(char *response, int *respLen) {
    if (!response || !respLen) return;

    int maxLen = *respLen;
    int headerLen = strlen(onvifgood);
    memcpy(response, onvifgood, headerLen);
    *respLen = headerLen;

    *respLen += snprintf(response + headerLen, maxLen - headerLen,
        capabilitiesxml,
        netinfo.ipaddr[0], app_config.web_port,    // Analytics
        netinfo.ipaddr[0], app_config.web_port,    // Device
        netinfo.ipaddr[0], app_config.web_port,    // Events
        netinfo.ipaddr[0], app_config.web_port,    // Imaging
        netinfo.ipaddr[0], app_config.web_port,    // Media
        netinfo.ipaddr[0], app_config.web_port);   // PTZ
}

void onvif_respond_deviceinfo(char *response, int *respLen) {
    if (!response || !respLen) return;

    int maxLen = *respLen;
    int headerLen = strlen(onvifgood);
    memcpy(response, onvifgood, headerLen);
    *respLen = headerLen;

    *respLen += snprintf(response + headerLen, maxLen - headerLen,
        deviceinfoxml,
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

    int maxLen = *respLen;
    int headerLen = strlen(onvifgood);
    memcpy(response, onvifgood, headerLen);
    *respLen = headerLen;

    *respLen += snprintf(response + headerLen, maxLen - headerLen,
        mediaprofilesxml,
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
        snprintf(snapshot_url, sizeof(snapshot_url), "http:///%s:%d/image.jpg",
            netinfo.ipaddr[0], app_config.web_port);

    int maxLen = *respLen;
    int headerLen = strlen(onvifgood);
    memcpy(response, onvifgood, headerLen);
    *respLen = headerLen;

    *respLen += snprintf(response + headerLen, maxLen - headerLen,
        snapshotxml,
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

    int maxLen = *respLen;
    int headerLen = strlen(onvifgood);
    memcpy(response, onvifgood, headerLen);
    *respLen = headerLen;

    *respLen += snprintf(response + headerLen, maxLen - headerLen,
        streamxml,
        stream_url);
}

void onvif_respond_systemtime(char *response, int *respLen) {
    if (!response || !respLen) return;

    time_t now;
    struct tm *tm_info;

    time(&now);
    tm_info = gmtime(&now);

    int maxLen = *respLen;
    int headerLen = strlen(onvifgood);
    memcpy(response, onvifgood, headerLen);
    *respLen = headerLen;

    *respLen += snprintf(response + headerLen, maxLen - headerLen,
        systemtimexml,
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

    int maxLen = *respLen;
    int headerLen = strlen(onvifgood);
    memcpy(response, onvifgood, headerLen);
    *respLen = headerLen;

    *respLen += snprintf(response + headerLen, maxLen - headerLen,
        videosourcesxml,
        framerate, width, height);
}