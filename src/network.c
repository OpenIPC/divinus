#include "network.h"

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
IMPORT_STR(.rodata, "../res/onvif/scopes.xml", scopesxml);
extern const char scopesxml[];
IMPORT_STR(.rodata, "../res/onvif/snapshot.xml", snapshotxml);
extern const char snapshotxml[];
IMPORT_STR(.rodata, "../res/onvif/stream.xml", streamxml);
extern const char streamxml[];
IMPORT_STR(.rodata, "../res/onvif/systemtime.xml", systemtimexml);
extern const char systemtimexml[];

const char onvifgood[] = "HTTP/1.1 200 OK\r\n" \
                         "Content-Type: application/soap+xml; charset=utf-8\r\n" \
                         "Connection: close\r\n" \
                         "\r\n";

struct {
    char intf[3][16];
    char ipaddr[3][INET_ADDRSTRLEN];
    char count;
    char host[65];
} netinfo;

char configured = 0;

struct mdnsd *mdns = {0};
pthread_t onvifPid = 0;

void init_network(void) {
    if (configured) return;

    struct ifaddrs *ifa, *ifaddr;
    if (getifaddrs(&ifaddr) == -1) {
        HAL_DANGER("network", "Failed to get network interfaces!\n");
        return;
    }

    struct utsname uts;
    if (uname(&uts) == -1) {
        HAL_DANGER("network", "Failed to get system information!\n");
        return;
    }
    strcpy(netinfo.host, uts.nodename);

    for (ifa = ifaddr; ifa; ifa = ifa->ifa_next) {
        if (netinfo.count >= 3) break;
        if (!ifa->ifa_addr || ifa->ifa_addr->sa_family != AF_INET) continue;
        if (ifa->ifa_flags & IFF_LOOPBACK) continue;
        struct sockaddr_in *addr = (struct sockaddr_in *)ifa->ifa_addr;
        strcpy(netinfo.intf[netinfo.count], ifa->ifa_name);
        inet_ntop(AF_INET, &addr->sin_addr.s_addr, 
            netinfo.ipaddr[netinfo.count], sizeof(*netinfo.ipaddr));
        HAL_INFO("network", "Interface %s has address %s\n", 
            netinfo.intf[netinfo.count], netinfo.ipaddr[netinfo.count]);
        netinfo.count++;
    }
    freeifaddrs(ifaddr);

    configured = 1;
}

int start_network(void) {
    init_network();
    if (!configured)
        return EXIT_FAILURE;

    if (app_config.mdns_enable)
        start_mdns();

    if (app_config.onvif_enable)
        start_onvif();

    return EXIT_SUCCESS;
}

void stop_network(void) {
    if (app_config.mdns_enable)
        stop_mdns();

    if (app_config.onvif_enable)
        stop_onvif();
}

int start_mdns(void) {
    char hostname[71];

    if (!(mdns = mdnsd_start()))
        HAL_ERROR("mdns", "Failed to start mDNS server!\n");

    strcpy(hostname, netinfo.host);
    strcat(hostname, ".local");
    mdnsd_set_hostname(mdns, hostname, ip_to_int(netinfo.ipaddr[0]));

    for (char i = 0; i < netinfo.count; i++) {
        struct rr_entry *entry = NULL;
        HAL_INFO("mdns", "Adding an A entry for IP %s...\n", netinfo.ipaddr[i]);
        entry = rr_create_a(create_nlabel(hostname), ip_to_int(netinfo.ipaddr[i]));
        mdnsd_add_rr(mdns, entry);
    }
}

void stop_mdns(void) {
    if (mdns)
        mdnsd_stop(mdns);

    mdns = NULL;
}

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

void onvif_respond_capabilities(char *response, int *respLen) {
    if (!response || !respLen) return;

    char device_url[128], media_url[128], ptz_section[128] = {0};
    snprintf(device_url, sizeof(device_url), "http://%s:%d/onvif/device_service",
        netinfo.ipaddr[0], app_config.web_port);
    snprintf(media_url, sizeof(media_url), "http://%s:%d/onvif/media_service",
        netinfo.ipaddr[0], app_config.web_port);
#if 0
    snprintf(ptz_section, sizeof(ptz_section),
        "        <tds:PTZ>\n"
        "          <tds:XAddr>http://%s:%d/onvif/ptz_service</tds:XAddr>\n"
        "        </td:PTZ>\n", netinfo.ipaddr[0], app_config.web_port);
        netinfo.ipaddr[0], app_config.web_port);
#endif

    int maxLen = *respLen;
    int headerLen = strlen(onvifgood);
    memcpy(response, onvifgood, headerLen);
    *respLen = headerLen;

    *respLen += snprintf(response + headerLen, maxLen - headerLen,
        capabilitiesxml,
        device_url, app_config.web_enable_auth ? "true" : "false",
        media_url, ptz_section);
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

void onvif_respond_scopes(char *response, int *respLen) {
    if (!response || !respLen) return;

    char scopes[2048];
    int scopesLen = 0;

    const char* scope_items[] = {
        "onvif://www.onvif.org/type/video_encoder",
        "onvif://www.onvif.org/type/audio_encoder",
        "onvif://www.onvif.org/location/Unknown",
        "onvif://www.onvif.org/Profile/Streaming"
    };

    for (char i = 0; i < sizeof(scope_items) / sizeof(*scope_items); i++)
        scopesLen += snprintf(&scopes[scopesLen], sizeof(scopes) - scopesLen,
            "      <tds:Scopes>\n"
            "        <tds:ScopeDef>Fixed</tds:ScopeDef>\n"
            "        <tds:ScopeItem>%s</tds:ScopeItem>\n"
            "      </tds:Scopes>\n", scope_items[i]);

    int maxLen = *respLen;
    int headerLen = strlen(onvifgood);
    memcpy(response, onvifgood, headerLen);
    *respLen = headerLen;

    *respLen += snprintf(response + headerLen, maxLen - headerLen,
        scopesxml,
        scopes);
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