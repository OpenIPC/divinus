#include "onvif.h"
#include "media.h"

#include <arpa/inet.h>
#include <sys/ioctl.h>
#include <sys/time.h>
#include <unistd.h>

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
IMPORT_STR(.rodata, "../res/onvif/settime.xml", settimexml);
extern const char settimexml[];
IMPORT_STR(.rodata, "../res/onvif/videosources.xml", videosourcesxml);
extern const char videosourcesxml[];
IMPORT_STR(.rodata, "../res/onvif/services.xml", servicesxml);
extern const char servicesxml[];
IMPORT_STR(.rodata, "../res/onvif/scopes.xml", scopesxml);
extern const char scopesxml[];
IMPORT_STR(.rodata, "../res/onvif/network.xml", networkxml);
extern const char networkxml[];
IMPORT_STR(.rodata, "../res/onvif/hostname.xml", hostnamexml);
extern const char hostnamexml[];
IMPORT_STR(.rodata, "../res/onvif/dns.xml", dnsxml);
extern const char dnsxml[];
IMPORT_STR(.rodata, "../res/onvif/protocols.xml", protocolsxml);
extern const char protocolsxml[];
IMPORT_STR(.rodata, "../res/onvif/devicecaps.xml", devicecapsxml);
extern const char devicecapsxml[];
IMPORT_STR(.rodata, "../res/onvif/mediacaps.xml", mediacapsxml);
extern const char mediacapsxml[];
IMPORT_STR(.rodata, "../res/onvif/profile.xml", profilexml);
extern const char profilexml[];
IMPORT_STR(.rodata, "../res/onvif/encoders.xml", encodersxml);
extern const char encodersxml[];
IMPORT_STR(.rodata, "../res/onvif/encoder.xml", encoderxml);
extern const char encoderxml[];
IMPORT_STR(.rodata, "../res/onvif/sourceconfigs.xml", sourceconfigsxml);
extern const char sourceconfigsxml[];
IMPORT_STR(.rodata, "../res/onvif/gateway.xml", gatewayxml);
extern const char gatewayxml[];
IMPORT_STR(.rodata, "../res/onvif/encoderoptions.xml", encoderoptionsxml);
extern const char encoderoptionsxml[];

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

    if (!soap_data) return NULL;
    char *body_start = strstr(soap_data, "Body");
    if (!body_start) return NULL;

    body_start = strchr(body_start, '>');
    if (!body_start) return NULL;
    body_start++;

    while (*body_start && isspace(*body_start)) body_start++;

    if (*body_start != '<') return NULL;
    body_start++;

    char *action_end = body_start;
    while (*action_end && *action_end != ' ' && *action_end != '>' &&
        *action_end != '/' && *action_end != '\r' && *action_end != '\n')
        action_end++;
    if (action_end == body_start) return NULL;

    int action_len = action_end - body_start;
    if (action_len >= sizeof(action)) action_len = sizeof(action) - 1;

    strncpy(action, body_start, action_len);
    action[action_len] = '\0';

    char *colon = strrchr(action, ':');
    if (colon && colon[1])
        memmove(action, colon + 1, strlen(colon + 1) + 1);

    return action;
}

bool onvif_is_preauth_action(const char *action) {
    if (!action || EMPTY(action))
        return false;

    if (EQUALS(action, "GetCapabilities") ||
        EQUALS(action, "GetServices") ||
        EQUALS(action, "GetSystemDateAndTime"))
        return true;

    return false;
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

static void onvif_hwaddr(char *out, size_t outlen) {
    snprintf(out, outlen, "00:00:00:00:00:00");
    if (!netinfo.count || !outlen) return;
    char path[64];
    snprintf(path, sizeof(path), "/sys/class/net/%s/address", netinfo.intf[0]);
    FILE *file = fopen(path, "r");
    if (!file) return;
    if (fgets(out, outlen, file)) {
        char *nl = strchr(out, '\n');
        if (nl) *nl = '\0';
    }
    fclose(file);
}

static int onvif_prefix(const char *ifname) {
    int prefix = 24;
    int fd;
    struct ifreq ifr;
    if (!ifname) return prefix;
    fd = socket(AF_INET, SOCK_DGRAM, 0);
    if (fd < 0) return prefix;
    memset(&ifr, 0, sizeof(ifr));
    strncpy(ifr.ifr_name, ifname, IFNAMSIZ - 1);
    if (ioctl(fd, SIOCGIFNETMASK, &ifr) == 0) {
        unsigned int mask = ntohl(((struct sockaddr_in *)&ifr.ifr_netmask)->sin_addr.s_addr);
        prefix = 0;
        while (mask & 0x80000000u) {
            prefix++;
            mask <<= 1;
        }
    }
    close(fd);
    return prefix;
}

static void onvif_codec_block(char *dst, size_t dstlen, const char *encoding, int gop) {
    dst[0] = '\0';
    if (!encoding || gop <= 0) return;
    if (!strcmp(encoding, "H264"))
        snprintf(dst, dstlen,
            "          <tt:H264>\n"
            "            <tt:GovLength>%d</tt:GovLength>\n"
            "            <tt:H264Profile>Main</tt:H264Profile>\n"
            "          </tt:H264>\n", gop);
    else if (!strcmp(encoding, "H265"))
        snprintf(dst, dstlen,
            "          <tt:H265>\n"
            "            <tt:GovLength>%d</tt:GovLength>\n"
            "            <tt:H265Profile>Main</tt:H265Profile>\n"
            "          </tt:H265>\n", gop);
}

static int onvif_append_profile(char *dst, int used, const char *token, const char *name,
    int src, int enc, int src_h, int src_w, const char *encoding,
    int width, int height, int fps, int bitrate, int gop) {
    char codec[320];
    onvif_codec_block(codec, sizeof(codec), encoding, gop);
    return used + sprintf(dst + used, mediaprofilexml,
        token, name, src, src, src_h, src_w, enc, enc,
        encoding, width, height, fps, bitrate, codec);
}

static int onvif_fill_profiles(char *dst, int which) {
    int used = 0;
    int gop;
    const char *encoding;
    dst[0] = '\0';
    if (app_config.mp4_enable) {
        gop = app_config.mp4_gop ? app_config.mp4_gop : app_config.mp4_fps * 2;
        encoding = app_config.mp4_codecH265 ? "H265" : "H264";
        if (which != 2)
            used = onvif_append_profile(dst, used, "stream=0", "MainStream", 1, 1,
                app_config.mp4_height, app_config.mp4_width,
                encoding, app_config.mp4_width, app_config.mp4_height,
                app_config.mp4_fps, app_config.mp4_bitrate, gop);
        if (which != 1 && media_sub_ch >= 0)
            used = onvif_append_profile(dst, used, "stream=1", "SubStream", 1, 2,
                app_config.mp4_height, app_config.mp4_width,
                "H264", MEDIA_SUB_WIDTH, MEDIA_SUB_HEIGHT,
                MEDIA_SUB_FPS, MEDIA_SUB_BITRATE, MEDIA_SUB_GOP);
        return used;
    }
    if (app_config.mjpeg_enable && which != 1)
        used = onvif_append_profile(dst, used, "stream=1", "SubStream", 1, 1,
            app_config.mjpeg_height, app_config.mjpeg_width,
            "JPEG", app_config.mjpeg_width, app_config.mjpeg_height,
            app_config.mjpeg_fps, app_config.mjpeg_bitrate, 0);
    return used;
}

static int onvif_fill_encoders(char *dst) {
    int used = 0;
    int index = 1;
    char codec[320];
    dst[0] = '\0';
    if (app_config.mp4_enable) {
        int gop = app_config.mp4_gop ? app_config.mp4_gop : app_config.mp4_fps * 2;
        const char *encoding = app_config.mp4_codecH265 ? "H265" : "H264";
        onvif_codec_block(codec, sizeof(codec), encoding, gop);
        used += sprintf(dst + used, encoderxml, index, index, encoding,
            app_config.mp4_width, app_config.mp4_height,
            app_config.mp4_fps, app_config.mp4_bitrate, codec);
        if (media_sub_ch >= 0) {
            onvif_codec_block(codec, sizeof(codec), "H264", MEDIA_SUB_GOP);
            used += sprintf(dst + used, encoderxml, 2, 2, "H264",
                MEDIA_SUB_WIDTH, MEDIA_SUB_HEIGHT,
                MEDIA_SUB_FPS, MEDIA_SUB_BITRATE, codec);
        }
    } else if (app_config.mjpeg_enable) {
        codec[0] = '\0';
        used += sprintf(dst + used, encoderxml, index, index, "JPEG",
            app_config.mjpeg_width, app_config.mjpeg_height,
            app_config.mjpeg_fps, app_config.mjpeg_bitrate, codec);
    }
    return used;
}

void onvif_respond_capabilities(char *response, int *respLen) {
    if (!response || !respLen) return;

    int maxLen = *respLen;
    int headerLen = strlen(onvifgood);
    memcpy(response, onvifgood, headerLen);
    *respLen = headerLen;

    *respLen += snprintf(response + headerLen, maxLen - headerLen,
        capabilitiesxml,
        netinfo.ipaddr[0], app_config.web_port,
        netinfo.ipaddr[0], app_config.web_port,
        app_config.onvif_enable_auth ? "true" : "false",
        netinfo.ipaddr[0], app_config.web_port,
        netinfo.ipaddr[0], app_config.web_port,
        netinfo.ipaddr[0], app_config.web_port,
        netinfo.ipaddr[0], app_config.web_port);
}

void onvif_respond_deviceinfo(char *response, int *respLen) {
    if (!response || !respLen) return;

    int maxLen = *respLen;
    int headerLen = strlen(onvifgood);
    memcpy(response, onvifgood, headerLen);
    *respLen = headerLen;

    char serial[32];
    onvif_hwaddr(serial, sizeof(serial));
    *respLen += snprintf(response + headerLen, maxLen - headerLen,
        deviceinfoxml,
        "OpenIPC", "IP Camera", "1.0", serial, chip);
}

void onvif_respond_mediaprofiles(char *response, int *respLen) {
    if (!response || !respLen) return;

    char profile[4096];
    onvif_fill_profiles(profile, 0);

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
        snprintf(snapshot_url, sizeof(snapshot_url), "http://%s:%d/image.jpg",
            netinfo.ipaddr[0], app_config.web_port);

    int maxLen = *respLen;
    int headerLen = strlen(onvifgood);
    memcpy(response, onvifgood, headerLen);
    *respLen = headerLen;

    *respLen += snprintf(response + headerLen, maxLen - headerLen,
        snapshotxml,
        snapshot_url);
}

void onvif_respond_stream(char *response, int *respLen, const char *soap) {
    if (!response || !respLen) return;

    char stream_url[256];
    const char *name = "stream=0";

    if (soap && strstr(soap, "stream=1") && media_sub_ch >= 0)
        name = "stream=1";

    if (app_config.rtsp_enable_auth && 
        *app_config.rtsp_auth_user && *app_config.rtsp_auth_pass) {
        char user[96], pass[96];
        escape_url(user, app_config.rtsp_auth_user, sizeof(user));
        escape_url(pass, app_config.rtsp_auth_pass, sizeof(pass));
        snprintf(stream_url, sizeof(stream_url), "rtsp://%s:%s@%s:%d/%s",
            user, pass, netinfo.ipaddr[0], app_config.rtsp_port, name);
    } else
        snprintf(stream_url, sizeof(stream_url), "rtsp://%s:%d/%s",
            netinfo.ipaddr[0], app_config.rtsp_port, name);

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

static int onvif_xml_int(const char *xml, const char *tag) {
    char key[48];
    const char *pos;
    if (!xml || !tag) return -1;
    snprintf(key, sizeof(key), "%s>", tag);
    pos = strstr(xml, key);
    if (!pos) return -1;
    pos += strlen(key);
    while (*pos == ' ' || *pos == '\t' || *pos == '\r' || *pos == '\n') pos++;
    if (*pos < '0' || *pos > '9') return -1;
    return atoi(pos);
}

static time_t onvif_timegm(int year, int month, int day, int hour, int min, int sec) {
    static const int mdays[] = {0,31,59,90,120,151,181,212,243,273,304,334};
    int leaps, leaps0;
    long days;
    if (month < 1 || month > 12 || day < 1 || day > 31) return (time_t)-1;
    leaps = ((year - 1) / 4) - ((year - 1) / 100) + ((year - 1) / 400);
    leaps0 = (1969 / 4) - (1969 / 100) + (1969 / 400);
    days = (year - 1970) * 365L + (leaps - leaps0) + mdays[month - 1] + (day - 1);
    if (month > 2 && ((year % 4 == 0 && year % 100 != 0) || year % 400 == 0))
        days++;
    return days * 86400L + hour * 3600 + min * 60 + sec;
}

static void onvif_begin(char *response, int *respLen, int *headerLen, int *maxLen);

void onvif_respond_settime(char *response, int *respLen, const char *soap) {
    int headerLen, maxLen;
    const char *when;
    int year, month, day, hour, min, sec;
    if (!response || !respLen) return;
    when = soap ? strstr(soap, "UTCDateTime") : NULL;
    if (!when) when = soap;
    year = onvif_xml_int(when, "Year");
    month = onvif_xml_int(when, "Month");
    day = onvif_xml_int(when, "Day");
    hour = onvif_xml_int(when, "Hour");
    min = onvif_xml_int(when, "Minute");
    sec = onvif_xml_int(when, "Second");
    if (year > 2000 && hour >= 0 && min >= 0 && sec >= 0) {
        time_t stamp = onvif_timegm(year, month, day, hour, min, sec);
        struct timeval tv = { .tv_sec = stamp, .tv_usec = 0 };
        if (stamp == (time_t)-1 || settimeofday(&tv, NULL) != 0)
            HAL_WARNING("onvif", "Could not set the clock to %04d-%02d-%02d %02d:%02d:%02d\n",
                year, month, day, hour, min, sec);
        else
            HAL_INFO("onvif", "Clock set to %04d-%02d-%02d %02d:%02d:%02d UTC\n",
                year, month, day, hour, min, sec);
    }
    onvif_begin(response, respLen, &headerLen, &maxLen);
    *respLen += snprintf(response + headerLen, maxLen - headerLen, "%s", settimexml);
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

static void onvif_begin(char *response, int *respLen, int *headerLen, int *maxLen) {
    *maxLen = *respLen;
    *headerLen = strlen(onvifgood);
    memcpy(response, onvifgood, *headerLen);
    *respLen = *headerLen;
}

void onvif_respond_services(char *response, int *respLen) {
    int headerLen, maxLen;
    if (!response || !respLen) return;
    onvif_begin(response, respLen, &headerLen, &maxLen);
    *respLen += snprintf(response + headerLen, maxLen - headerLen, servicesxml,
        netinfo.ipaddr[0], app_config.web_port,
        netinfo.ipaddr[0], app_config.web_port);
}

void onvif_respond_scopes(char *response, int *respLen) {
    int headerLen, maxLen;
    if (!response || !respLen) return;
    onvif_begin(response, respLen, &headerLen, &maxLen);
    *respLen += snprintf(response + headerLen, maxLen - headerLen, scopesxml, chip);
}

void onvif_respond_network(char *response, int *respLen) {
    int headerLen, maxLen;
    char mac[32];
    const char *name = netinfo.count ? netinfo.intf[0] : "eth0";
    const char *ip = netinfo.count ? netinfo.ipaddr[0] : "0.0.0.0";
    if (!response || !respLen) return;
    onvif_hwaddr(mac, sizeof(mac));
    onvif_begin(response, respLen, &headerLen, &maxLen);
    *respLen += snprintf(response + headerLen, maxLen - headerLen, networkxml,
        name, name, mac, ip, onvif_prefix(name));
}

void onvif_respond_hostname(char *response, int *respLen) {
    int headerLen, maxLen;
    if (!response || !respLen) return;
    onvif_begin(response, respLen, &headerLen, &maxLen);
    *respLen += snprintf(response + headerLen, maxLen - headerLen, hostnamexml,
        netinfo.host[0] ? netinfo.host : "divinus");
}

void onvif_respond_dns(char *response, int *respLen) {
    int headerLen, maxLen;
    if (!response || !respLen) return;
    onvif_begin(response, respLen, &headerLen, &maxLen);
    *respLen += snprintf(response + headerLen, maxLen - headerLen, "%s", dnsxml);
}

void onvif_respond_protocols(char *response, int *respLen) {
    int headerLen, maxLen;
    if (!response || !respLen) return;
    onvif_begin(response, respLen, &headerLen, &maxLen);
    *respLen += snprintf(response + headerLen, maxLen - headerLen, protocolsxml,
        app_config.web_port,
        app_config.rtsp_enable ? "true" : "false",
        app_config.rtsp_port);
}

void onvif_respond_device_caps(char *response, int *respLen) {
    int headerLen, maxLen;
    if (!response || !respLen) return;
    onvif_begin(response, respLen, &headerLen, &maxLen);
    *respLen += snprintf(response + headerLen, maxLen - headerLen, devicecapsxml,
        app_config.onvif_enable_auth ? "true" : "false");
}

void onvif_respond_media_caps(char *response, int *respLen) {
    int headerLen, maxLen;
    if (!response || !respLen) return;
    onvif_begin(response, respLen, &headerLen, &maxLen);
    *respLen += snprintf(response + headerLen, maxLen - headerLen, "%s", mediacapsxml);
}

void onvif_respond_profile(char *response, int *respLen, const char *soap) {
    int headerLen, maxLen;
    char profile[4096];
    char *close;
    int which = (soap && strstr(soap, "stream=1")) ? 2 : 1;
    if (!response || !respLen) return;
    onvif_fill_profiles(profile, which);
    if (!profile[0])
        onvif_fill_profiles(profile, 1);
    if ((close = strstr(profile, "<trt:Profiles")))
        memcpy(close, "<trt:Profile ", 13);
    if ((close = strstr(profile, "</trt:Profiles>"))) {
        memmove(close + 14, close + 15, strlen(close + 15) + 1);
        memcpy(close, "</trt:Profile>", 14);
    }
    onvif_begin(response, respLen, &headerLen, &maxLen);
    *respLen += snprintf(response + headerLen, maxLen - headerLen, profilexml, profile);
}

void onvif_respond_encoders(char *response, int *respLen) {
    int headerLen, maxLen;
    char body[4096];
    if (!response || !respLen) return;
    onvif_fill_encoders(body);
    onvif_begin(response, respLen, &headerLen, &maxLen);
    *respLen += snprintf(response + headerLen, maxLen - headerLen, encodersxml, body);
}

void onvif_respond_source_configs(char *response, int *respLen) {
    int headerLen, maxLen;
    int width = app_config.mp4_enable ? app_config.mp4_width : app_config.mjpeg_width;
    int height = app_config.mp4_enable ? app_config.mp4_height : app_config.mjpeg_height;
    if (!response || !respLen) return;
    onvif_begin(response, respLen, &headerLen, &maxLen);
    *respLen += snprintf(response + headerLen, maxLen - headerLen, sourceconfigsxml,
        height, width);
}

static void onvif_gateway_addr(char *out, size_t outlen) {
    FILE *file;
    char line[256];
    snprintf(out, outlen, "0.0.0.0");
    file = fopen("/proc/net/route", "r");
    if (!file) return;
    if (!fgets(line, sizeof(line), file)) {
        fclose(file);
        return;
    }
    while (fgets(line, sizeof(line), file)) {
        char iface[16];
        unsigned int dest = 0, gateway = 0;
        if (sscanf(line, "%15s %x %x", iface, &dest, &gateway) == 3 &&
            dest == 0 && gateway) {
            snprintf(out, outlen, "%u.%u.%u.%u",
                gateway & 255, (gateway >> 8) & 255,
                (gateway >> 16) & 255, (gateway >> 24) & 255);
            break;
        }
    }
    fclose(file);
}

void onvif_respond_gateway(char *response, int *respLen) {
    int headerLen, maxLen;
    char gateway[32];
    if (!response || !respLen) return;
    onvif_gateway_addr(gateway, sizeof(gateway));
    onvif_begin(response, respLen, &headerLen, &maxLen);
    *respLen += snprintf(response + headerLen, maxLen - headerLen, gatewayxml, gateway);
}

void onvif_respond_encoder_options(char *response, int *respLen) {
    int headerLen, maxLen;
    int width = app_config.mp4_enable ? app_config.mp4_width : app_config.mjpeg_width;
    int height = app_config.mp4_enable ? app_config.mp4_height : app_config.mjpeg_height;
    int fps = app_config.mp4_enable ? app_config.mp4_fps : app_config.mjpeg_fps;
    if (!response || !respLen) return;
    if (fps <= 0) fps = 25;
    onvif_begin(response, respLen, &headerLen, &maxLen);
    *respLen += snprintf(response + headerLen, maxLen - headerLen, encoderoptionsxml,
        width, height, MEDIA_SUB_WIDTH, MEDIA_SUB_HEIGHT, fps);
}

static int onvif_static(char *response, int *respLen, const char *body) {
    int headerLen, maxLen;
    onvif_begin(response, respLen, &headerLen, &maxLen);
    *respLen += snprintf(response + headerLen, maxLen - headerLen, "%s", body);
    return 1;
}

static const char onvif_relay[] =
    "<?xml version=\"1.0\" encoding=\"UTF-8\"?>"
    "<SOAP-ENV:Envelope xmlns:SOAP-ENV=\"http://www.w3.org/2003/05/soap-envelope\" xmlns:tds=\"http://www.onvif.org/ver10/device/wsdl\">"
    "<SOAP-ENV:Body><tds:GetRelayOutputsResponse/></SOAP-ENV:Body></SOAP-ENV:Envelope>";

static const char onvif_setnetif[] =
    "<?xml version=\"1.0\" encoding=\"UTF-8\"?>"
    "<SOAP-ENV:Envelope xmlns:SOAP-ENV=\"http://www.w3.org/2003/05/soap-envelope\" xmlns:tds=\"http://www.onvif.org/ver10/device/wsdl\">"
    "<SOAP-ENV:Body><tds:SetNetworkInterfacesResponse><tds:RebootNeeded>false</tds:RebootNeeded>"
    "</tds:SetNetworkInterfacesResponse></SOAP-ENV:Body></SOAP-ENV:Envelope>";

static const char onvif_ntp[] =
    "<?xml version=\"1.0\" encoding=\"UTF-8\"?>"
    "<SOAP-ENV:Envelope xmlns:SOAP-ENV=\"http://www.w3.org/2003/05/soap-envelope\" xmlns:tds=\"http://www.onvif.org/ver10/device/wsdl\" xmlns:tt=\"http://www.onvif.org/ver10/schema\">"
    "<SOAP-ENV:Body><tds:GetNTPResponse><tds:NTPInformation><tt:FromDHCP>false</tt:FromDHCP>"
    "</tds:NTPInformation></tds:GetNTPResponse></SOAP-ENV:Body></SOAP-ENV:Envelope>";

static const char onvif_discover[] =
    "<?xml version=\"1.0\" encoding=\"UTF-8\"?>"
    "<SOAP-ENV:Envelope xmlns:SOAP-ENV=\"http://www.w3.org/2003/05/soap-envelope\" xmlns:tds=\"http://www.onvif.org/ver10/device/wsdl\">"
    "<SOAP-ENV:Body><tds:GetDiscoveryModeResponse><tds:DiscoveryMode>Discoverable</tds:DiscoveryMode>"
    "</tds:GetDiscoveryModeResponse></SOAP-ENV:Body></SOAP-ENV:Envelope>";

static const char onvif_osds[] =
    "<?xml version=\"1.0\" encoding=\"UTF-8\"?>"
    "<SOAP-ENV:Envelope xmlns:SOAP-ENV=\"http://www.w3.org/2003/05/soap-envelope\" xmlns:trt=\"http://www.onvif.org/ver10/media/wsdl\">"
    "<SOAP-ENV:Body><trt:GetOSDsResponse/></SOAP-ENV:Body></SOAP-ENV:Envelope>";

static const char onvif_osdopt[] =
    "<?xml version=\"1.0\" encoding=\"UTF-8\"?>"
    "<SOAP-ENV:Envelope xmlns:SOAP-ENV=\"http://www.w3.org/2003/05/soap-envelope\" xmlns:trt=\"http://www.onvif.org/ver10/media/wsdl\" xmlns:tt=\"http://www.onvif.org/ver10/schema\">"
    "<SOAP-ENV:Body><trt:GetOSDOptionsResponse><trt:OSDOptions>"
    "<tt:MaximumNumberOfOSDs Total=\"0\" Image=\"0\" PlainText=\"0\" Date=\"0\" Time=\"0\" DateAndTime=\"0\"/>"
    "</trt:OSDOptions></trt:GetOSDOptionsResponse></SOAP-ENV:Body></SOAP-ENV:Envelope>";

static const char onvif_audiosrc[] =
    "<?xml version=\"1.0\" encoding=\"UTF-8\"?>"
    "<SOAP-ENV:Envelope xmlns:SOAP-ENV=\"http://www.w3.org/2003/05/soap-envelope\" xmlns:trt=\"http://www.onvif.org/ver10/media/wsdl\">"
    "<SOAP-ENV:Body><trt:GetAudioSourcesResponse/></SOAP-ENV:Body></SOAP-ENV:Envelope>";

static const char onvif_audioenc[] =
    "<?xml version=\"1.0\" encoding=\"UTF-8\"?>"
    "<SOAP-ENV:Envelope xmlns:SOAP-ENV=\"http://www.w3.org/2003/05/soap-envelope\" xmlns:trt=\"http://www.onvif.org/ver10/media/wsdl\">"
    "<SOAP-ENV:Body><trt:GetAudioEncoderConfigurationsResponse/></SOAP-ENV:Body></SOAP-ENV:Envelope>";

static const char onvif_audioopt[] =
    "<?xml version=\"1.0\" encoding=\"UTF-8\"?>"
    "<SOAP-ENV:Envelope xmlns:SOAP-ENV=\"http://www.w3.org/2003/05/soap-envelope\" xmlns:trt=\"http://www.onvif.org/ver10/media/wsdl\">"
    "<SOAP-ENV:Body><trt:GetAudioEncoderConfigurationOptionsResponse><trt:Options/>"
    "</trt:GetAudioEncoderConfigurationOptionsResponse></SOAP-ENV:Body></SOAP-ENV:Envelope>";

extern void request_idr(void);

static const char onvif_sync[] =
    "<?xml version=\"1.0\" encoding=\"UTF-8\"?>"
    "<SOAP-ENV:Envelope xmlns:SOAP-ENV=\"http://www.w3.org/2003/05/soap-envelope\" xmlns:trt=\"http://www.onvif.org/ver10/media/wsdl\">"
    "<SOAP-ENV:Body><trt:SetSynchronizationPointResponse/></SOAP-ENV:Body></SOAP-ENV:Envelope>";

static const char onvif_meta[] =
    "<?xml version=\"1.0\" encoding=\"UTF-8\"?>"
    "<SOAP-ENV:Envelope xmlns:SOAP-ENV=\"http://www.w3.org/2003/05/soap-envelope\" xmlns:trt=\"http://www.onvif.org/ver10/media/wsdl\">"
    "<SOAP-ENV:Body><trt:GetMetadataConfigurationsResponse/></SOAP-ENV:Body></SOAP-ENV:Envelope>";

static const char onvif_image[] =
    "<?xml version=\"1.0\" encoding=\"UTF-8\"?>"
    "<SOAP-ENV:Envelope xmlns:SOAP-ENV=\"http://www.w3.org/2003/05/soap-envelope\" xmlns:timg=\"http://www.onvif.org/ver20/imaging/wsdl\" xmlns:tt=\"http://www.onvif.org/ver10/schema\">"
    "<SOAP-ENV:Body><timg:GetImagingSettingsResponse><timg:ImagingSettings>"
    "<tt:Brightness>50</tt:Brightness><tt:ColorSaturation>50</tt:ColorSaturation>"
    "<tt:Contrast>50</tt:Contrast><tt:Sharpness>50</tt:Sharpness>"
    "</timg:ImagingSettings></timg:GetImagingSettingsResponse></SOAP-ENV:Body></SOAP-ENV:Envelope>";

static const char onvif_imageopt[] =
    "<?xml version=\"1.0\" encoding=\"UTF-8\"?>"
    "<SOAP-ENV:Envelope xmlns:SOAP-ENV=\"http://www.w3.org/2003/05/soap-envelope\" xmlns:timg=\"http://www.onvif.org/ver20/imaging/wsdl\" xmlns:tt=\"http://www.onvif.org/ver10/schema\">"
    "<SOAP-ENV:Body><timg:GetOptionsResponse><timg:ImagingOptions>"
    "<tt:Brightness><tt:Min>0</tt:Min><tt:Max>100</tt:Max></tt:Brightness>"
    "<tt:ColorSaturation><tt:Min>0</tt:Min><tt:Max>100</tt:Max></tt:ColorSaturation>"
    "<tt:Contrast><tt:Min>0</tt:Min><tt:Max>100</tt:Max></tt:Contrast>"
    "<tt:Sharpness><tt:Min>0</tt:Min><tt:Max>100</tt:Max></tt:Sharpness>"
    "</timg:ImagingOptions></timg:GetOptionsResponse></SOAP-ENV:Body></SOAP-ENV:Envelope>";

static const char onvif_imageset[] =
    "<?xml version=\"1.0\" encoding=\"UTF-8\"?>"
    "<SOAP-ENV:Envelope xmlns:SOAP-ENV=\"http://www.w3.org/2003/05/soap-envelope\" xmlns:timg=\"http://www.onvif.org/ver20/imaging/wsdl\">"
    "<SOAP-ENV:Body><timg:SetImagingSettingsResponse/></SOAP-ENV:Body></SOAP-ENV:Envelope>";

static const char onvif_events[] =
    "<?xml version=\"1.0\" encoding=\"UTF-8\"?>"
    "<SOAP-ENV:Envelope xmlns:SOAP-ENV=\"http://www.w3.org/2003/05/soap-envelope\" "
    "xmlns:tev=\"http://www.onvif.org/ver10/events/wsdl\" "
    "xmlns:wsnt=\"http://docs.oasis-open.org/wsn/b-2\" "
    "xmlns:wstop=\"http://docs.oasis-open.org/wsn/t-1\">"
    "<SOAP-ENV:Body><tev:GetEventPropertiesResponse>"
    "<tev:TopicNamespaceLocation>http://www.onvif.org/onvif/ver10/topics/topicns.xml</tev:TopicNamespaceLocation>"
    "<wsnt:FixedTopicSet>true</wsnt:FixedTopicSet><wstop:TopicSet/>"
    "<wsnt:TopicExpressionDialect>http://www.onvif.org/ver10/tev/topicExpression/ConcreteSet</wsnt:TopicExpressionDialect>"
    "<tev:MessageContentFilterDialect>http://www.onvif.org/ver10/tev/messageContentFilter/ItemFilter</tev:MessageContentFilterDialect>"
    "<tev:MessageContentSchemaLocation>http://www.onvif.org/onvif/ver10/schema/onvif.xsd</tev:MessageContentSchemaLocation>"
    "</tev:GetEventPropertiesResponse></SOAP-ENV:Body></SOAP-ENV:Envelope>";

static const char onvif_ptzcfg[] =
    "<?xml version=\"1.0\" encoding=\"UTF-8\"?>"
    "<SOAP-ENV:Envelope xmlns:SOAP-ENV=\"http://www.w3.org/2003/05/soap-envelope\" xmlns:tptz=\"http://www.onvif.org/ver20/ptz/wsdl\">"
    "<SOAP-ENV:Body><tptz:GetConfigurationsResponse/></SOAP-ENV:Body></SOAP-ENV:Envelope>";

static const char onvif_ptzpresets[] =
    "<?xml version=\"1.0\" encoding=\"UTF-8\"?>"
    "<SOAP-ENV:Envelope xmlns:SOAP-ENV=\"http://www.w3.org/2003/05/soap-envelope\" xmlns:tptz=\"http://www.onvif.org/ver20/ptz/wsdl\">"
    "<SOAP-ENV:Body><tptz:GetPresetsResponse/></SOAP-ENV:Body></SOAP-ENV:Envelope>";

static const char onvif_ptznodes[] =
    "<?xml version=\"1.0\" encoding=\"UTF-8\"?>"
    "<SOAP-ENV:Envelope xmlns:SOAP-ENV=\"http://www.w3.org/2003/05/soap-envelope\" xmlns:tptz=\"http://www.onvif.org/ver20/ptz/wsdl\">"
    "<SOAP-ENV:Body><tptz:GetNodesResponse/></SOAP-ENV:Body></SOAP-ENV:Envelope>";

static const char onvif_ptzmove[] =
    "<?xml version=\"1.0\" encoding=\"UTF-8\"?>"
    "<SOAP-ENV:Envelope xmlns:SOAP-ENV=\"http://www.w3.org/2003/05/soap-envelope\" xmlns:tptz=\"http://www.onvif.org/ver20/ptz/wsdl\">"
    "<SOAP-ENV:Body><tptz:ContinuousMoveResponse/></SOAP-ENV:Body></SOAP-ENV:Envelope>";

static const char onvif_ptzstop[] =
    "<?xml version=\"1.0\" encoding=\"UTF-8\"?>"
    "<SOAP-ENV:Envelope xmlns:SOAP-ENV=\"http://www.w3.org/2003/05/soap-envelope\" xmlns:tptz=\"http://www.onvif.org/ver20/ptz/wsdl\">"
    "<SOAP-ENV:Body><tptz:StopResponse/></SOAP-ENV:Body></SOAP-ENV:Envelope>";

static int onvif_source_one(char *response, int *respLen) {
    int headerLen, maxLen;
    int width = app_config.mp4_enable ? app_config.mp4_width : app_config.mjpeg_width;
    int height = app_config.mp4_enable ? app_config.mp4_height : app_config.mjpeg_height;
    onvif_begin(response, respLen, &headerLen, &maxLen);
    *respLen += snprintf(response + headerLen, maxLen - headerLen,
        "<?xml version=\"1.0\" encoding=\"UTF-8\"?>"
        "<SOAP-ENV:Envelope xmlns:SOAP-ENV=\"http://www.w3.org/2003/05/soap-envelope\" xmlns:trt=\"http://www.onvif.org/ver10/media/wsdl\" xmlns:tt=\"http://www.onvif.org/ver10/schema\">"
        "<SOAP-ENV:Body><trt:GetVideoSourceConfigurationResponse>"
        "<trt:Configuration token=\"VSC_1\"><tt:Name>VideoSourceConfig_1</tt:Name><tt:UseCount>1</tt:UseCount>"
        "<tt:SourceToken>VideoSource_1</tt:SourceToken><tt:Bounds height=\"%d\" width=\"%d\" y=\"0\" x=\"0\"/>"
        "</trt:Configuration></trt:GetVideoSourceConfigurationResponse></SOAP-ENV:Body></SOAP-ENV:Envelope>",
        height, width);
    return 1;
}

static int onvif_source_options(char *response, int *respLen) {
    int headerLen, maxLen;
    int width = app_config.mp4_enable ? app_config.mp4_width : app_config.mjpeg_width;
    int height = app_config.mp4_enable ? app_config.mp4_height : app_config.mjpeg_height;
    onvif_begin(response, respLen, &headerLen, &maxLen);
    *respLen += snprintf(response + headerLen, maxLen - headerLen,
        "<?xml version=\"1.0\" encoding=\"UTF-8\"?>"
        "<SOAP-ENV:Envelope xmlns:SOAP-ENV=\"http://www.w3.org/2003/05/soap-envelope\" xmlns:trt=\"http://www.onvif.org/ver10/media/wsdl\" xmlns:tt=\"http://www.onvif.org/ver10/schema\">"
        "<SOAP-ENV:Body><trt:GetVideoSourceConfigurationOptionsResponse><trt:Options>"
        "<tt:BoundsRange><tt:XRange><tt:Min>0</tt:Min><tt:Max>0</tt:Max></tt:XRange>"
        "<tt:YRange><tt:Min>0</tt:Min><tt:Max>0</tt:Max></tt:YRange>"
        "<tt:WidthRange><tt:Min>%d</tt:Min><tt:Max>%d</tt:Max></tt:WidthRange>"
        "<tt:HeightRange><tt:Min>%d</tt:Min><tt:Max>%d</tt:Max></tt:HeightRange></tt:BoundsRange>"
        "<tt:VideoSourceTokensAvailable>VideoSource_1</tt:VideoSourceTokensAvailable>"
        "</trt:Options></trt:GetVideoSourceConfigurationOptionsResponse></SOAP-ENV:Body></SOAP-ENV:Envelope>",
        width, width, height, height);
    return 1;
}

static void onvif_keep_encoder(char *body, const char *soap) {
    char *first = strstr(body, "<trt:Configurations");
    char *second = first ? strstr(first + 1, "<trt:Configurations") : NULL;
    if (!second) return;
    if (soap && strstr(soap, "VEC_2"))
        memmove(body, second, strlen(second) + 1);
    else
        *second = '\0';
}

static int onvif_encoder_one(char *response, int *respLen, const char *wrapper,
    int singular, const char *soap) {
    int headerLen, maxLen;
    char body[4096];
    char *close;
    onvif_fill_encoders(body);
    if (singular) onvif_keep_encoder(body, soap);
    if (singular && (close = strstr(body, "<trt:Configurations")))
        memcpy(close, "<trt:Configuration ", 19);
    if (singular && (close = strstr(body, "</trt:Configurations>"))) {
        memmove(close + 20, close + 21, strlen(close + 21) + 1);
        memcpy(close, "</trt:Configuration>", 20);
    }
    onvif_begin(response, respLen, &headerLen, &maxLen);
    *respLen += snprintf(response + headerLen, maxLen - headerLen,
        "<?xml version=\"1.0\" encoding=\"UTF-8\"?>"
        "<SOAP-ENV:Envelope xmlns:SOAP-ENV=\"http://www.w3.org/2003/05/soap-envelope\" xmlns:trt=\"http://www.onvif.org/ver10/media/wsdl\" xmlns:tt=\"http://www.onvif.org/ver10/schema\">"
        "<SOAP-ENV:Body><trt:%sResponse>%s</trt:%sResponse></SOAP-ENV:Body></SOAP-ENV:Envelope>",
        wrapper, body, wrapper);
    return 1;
}

int onvif_dispatch_extra(const char *path, const char *action,
    char *response, int *respLen, const char *soap) {
    if (!path || !action || !response || !respLen) return 0;
    if (EQUALS(path, "device_service")) {
        if (EQUALS(action, "GetRelayOutputs")) return onvif_static(response, respLen, onvif_relay);
        if (EQUALS(action, "SetNetworkInterfaces")) return onvif_static(response, respLen, onvif_setnetif);
        if (EQUALS(action, "GetNTP")) return onvif_static(response, respLen, onvif_ntp);
        if (EQUALS(action, "GetDiscoveryMode")) return onvif_static(response, respLen, onvif_discover);
    } else if (EQUALS(path, "media_service")) {
        if (EQUALS(action, "GetVideoSourceConfiguration")) return onvif_source_one(response, respLen);
        if (EQUALS(action, "GetVideoSourceConfigurationOptions")) return onvif_source_options(response, respLen);
        if (EQUALS(action, "GetVideoEncoderConfiguration"))
            return onvif_encoder_one(response, respLen, "GetVideoEncoderConfiguration", 1, soap);
        if (EQUALS(action, "GetCompatibleVideoEncoderConfigurations"))
            return onvif_encoder_one(response, respLen, "GetCompatibleVideoEncoderConfigurations", 0, soap);
        if (EQUALS(action, "GetOSDs")) return onvif_static(response, respLen, onvif_osds);
        if (EQUALS(action, "GetOSDOptions")) return onvif_static(response, respLen, onvif_osdopt);
        if (EQUALS(action, "GetAudioSources")) return onvif_static(response, respLen, onvif_audiosrc);
        if (EQUALS(action, "GetAudioEncoderConfigurations") ||
            EQUALS(action, "GetAudioEncoderConfiguration"))
            return onvif_static(response, respLen, onvif_audioenc);
        if (EQUALS(action, "GetAudioEncoderConfigurationOptions"))
            return onvif_static(response, respLen, onvif_audioopt);
        if (EQUALS(action, "GetMetadataConfigurations")) return onvif_static(response, respLen, onvif_meta);
        if (EQUALS(action, "SetSynchronizationPoint")) {
            request_idr();
            return onvif_static(response, respLen, onvif_sync);
        }
    } else if (EQUALS(path, "imaging_service")) {
        if (EQUALS(action, "GetImagingSettings")) return onvif_static(response, respLen, onvif_image);
        if (EQUALS(action, "GetOptions")) return onvif_static(response, respLen, onvif_imageopt);
        if (EQUALS(action, "SetImagingSettings")) return onvif_static(response, respLen, onvif_imageset);
    } else if (EQUALS(path, "event_service")) {
        if (EQUALS(action, "GetEventProperties")) return onvif_static(response, respLen, onvif_events);
    } else if (EQUALS(path, "ptz_service")) {
        if (EQUALS(action, "GetConfigurations") || EQUALS(action, "GetConfiguration"))
            return onvif_static(response, respLen, onvif_ptzcfg);
        if (EQUALS(action, "GetPresets")) return onvif_static(response, respLen, onvif_ptzpresets);
        if (EQUALS(action, "GetNodes") || EQUALS(action, "GetNode"))
            return onvif_static(response, respLen, onvif_ptznodes);
        if (EQUALS(action, "ContinuousMove")) return onvif_static(response, respLen, onvif_ptzmove);
        if (EQUALS(action, "Stop")) return onvif_static(response, respLen, onvif_ptzstop);
    }
    return 0;
}