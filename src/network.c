#include "network.h"

pthread_t onvifPid = 0;

struct mdnsd *mdns = {0};

int start_mdns() {
    char configured = 0, *hostname, ipaddr[INET_ADDRSTRLEN];
    struct utsname uts;
    struct ifaddrs *ifaddr, *ifa;
    
    if (getifaddrs(&ifaddr) == -1)
        HAL_ERROR("mdns", "Failed to get network interfaces!\n");

    if (uname(&uts) == -1)
        HAL_ERROR("mdns", "Failed to get system information!\n");

    if (!asprintf(&hostname, "%s.local", uts.nodename))
        HAL_ERROR("mdns", "Failed to allocate memory for hostname!\n");

    if (!(mdns = mdnsd_start()))
        HAL_ERROR("mdns", "Failed to start mDNS server!\n");

    for (ifa = ifaddr; ifa != NULL; ifa = ifa->ifa_next) {
        if (!ifa->ifa_addr || ifa->ifa_addr->sa_family != AF_INET) continue;
        if (ifa->ifa_flags & IFF_LOOPBACK) continue;
        struct sockaddr_in *addr = (struct sockaddr_in *)ifa->ifa_addr;
        
        if (!configured) {
            HAL_INFO("mdns", "Setting hostname %s...\n", hostname);
            mdnsd_set_hostname(mdns, uts.nodename, addr->sin_addr.s_addr);
            configured = 1;
        }

        struct rr_entry *entry = NULL;
        inet_ntop(AF_INET, &addr->sin_addr.s_addr, ipaddr, sizeof(ipaddr));
        HAL_INFO("mdns", "Adding an A entry for IP %s...\n", ipaddr);
        entry = rr_create_a(create_nlabel(hostname), addr->sin_addr.s_addr);
        mdnsd_add_rr(mdns, entry);
    }

    free(hostname);
    
    freeifaddrs(ifaddr);
}

void stop_mdns() {
    if (mdns)
        mdnsd_stop(mdns);
    mdns = NULL;
}

void *onvif_thread(void) {
    struct ifaddrs *ifaddr, *ifa;
    char ipaddr[INET_ADDRSTRLEN];
    char msgbuf[4096];
    int servfd, msglen;
    struct sockaddr_in servaddr, clntaddr;
    socklen_t clntsz;

    if (getifaddrs(&ifaddr) == -1) {
        HAL_DANGER("onvif", "Failed to get network interfaces!\n");
        return (void*)EXIT_FAILURE;
    }

    for (ifa = ifaddr; ifa != NULL; ifa = ifa->ifa_next) {
        if (!ifa->ifa_addr || ifa->ifa_addr->sa_family != AF_INET) continue;
        if (ifa->ifa_flags & IFF_LOOPBACK) continue;
        struct sockaddr_in *addr = (struct sockaddr_in *)ifa->ifa_addr;
        inet_ntop(AF_INET, &addr->sin_addr.s_addr, ipaddr, sizeof(ipaddr));
        break;
    }

    if ((servfd = socket(AF_INET, SOCK_DGRAM, 0)) == -1) {
        HAL_DANGER("onvif", "Failed to create socket!\n");
        return (void*)EXIT_FAILURE;
    }

    memset(&servaddr, 0, sizeof(servaddr));
    servaddr.sin_family = AF_INET;
    servaddr.sin_addr.s_addr = INADDR_ANY;
    servaddr.sin_port = htons(3702);

    if (bind(servfd, (struct sockaddr *)&servaddr, sizeof(servaddr)) == -1) {
        HAL_DANGER("onvif", "Failed to bind socket!\n");
        return (void*)EXIT_FAILURE;
    }

    struct ip_mreq group = {
        .imr_multiaddr.s_addr = inet_addr("239.255.255.250"),
        .imr_interface.s_addr = inet_addr(ipaddr)
    };
    if (setsockopt(servfd, IPPROTO_IP, IP_ADD_MEMBERSHIP, (char *)&group, sizeof(group)) < 0){
        close(servfd);
        return (void*)EXIT_FAILURE;
    }

    while (keepRunning) {
        clntsz = sizeof(clntaddr);
        if ((msglen = recvfrom(servfd, msgbuf, sizeof(msgbuf), 0, (struct sockaddr *)&clntaddr, &clntsz)) < 0)
            continue;

        msgbuf[msglen] = '\0';
#ifdef DEBUG_ONVIF
        HAL_INFO("onvif", "Received message: %s\n", msgbuf);
#endif

        if (!strstr(msgbuf, "http://schemas.xmlsoap.org/ws/2005/04/discovery/Probe"))
            continue;

        char uuid[37], msgid[100] = {0};
        uuid_generate(uuid);
    
        char *msgid_init = strstr(msgbuf, "MessageID>");
        if (msgid_init) {
            msgid_init += 10;
            char *msgid_end = strstr(msgid_init, "<");
            if (msgid_end && (msgid_end - msgid_init) < sizeof(msgid)) {
                strncpy(msgid, msgid_init, msgid_end - msgid_init);
                msgid[msgid_end - msgid_init] = '\0';
            }
        }

        char response[4096] = {0};
        char device_uuid[64];
        snprintf(device_uuid, sizeof(device_uuid), "urn:uuid:%s", uuid);
        
        char device_name[64];
        snprintf(device_name, sizeof(device_name), "Divinus");
        
        char device_url[128];
        snprintf(device_url, sizeof(device_url), "http://%s:%d/onvif/device_service", ipaddr, app_config.web_port);
        
        int respLen = snprintf(response, sizeof(response),
            "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
            "<SOAP-ENV:Envelope "
                "xmlns:SOAP-ENV=\"http://www.w3.org/2003/05/soap-envelope\" "
                "xmlns:wsa=\"http://schemas.xmlsoap.org/ws/2004/08/addressing\" "
                "xmlns:d=\"http://schemas.xmlsoap.org/ws/2005/04/discovery\" "
                "xmlns:dn=\"http://www.onvif.org/ver10/network/wsdl\">\n"
            "  <SOAP-ENV:Header>\n"
            "    <wsa:MessageID>%s</wsa:MessageID>\n"
            "    <wsa:RelatesTo>%s</wsa:RelatesTo>\n"
            "    <wsa:To>http://schemas.xmlsoap.org/ws/2004/08/addressing/role/anonymous</wsa:To>\n"
            "    <wsa:Action>http://schemas.xmlsoap.org/ws/2005/04/discovery/ProbeMatches</wsa:Action>\n"
            "  </SOAP-ENV:Header>\n"
            "  <SOAP-ENV:Body>\n"
            "    <d:ProbeMatches>\n"
            "      <d:ProbeMatch>\n"
            "        <wsa:EndpointReference>\n"
            "          <wsa:Address>%s</wsa:Address>\n"
            "        </wsa:EndpointReference>\n"
            "        <d:Types>dn:NetworkVideoTransmitter</d:Types>\n"
            "        <d:Scopes>onvif://www.onvif.org/type/video_encoder onvif://www.onvif.org/Profile/Streaming onvif://www.onvif.org/name/%s onvif://www.onvif.org/hardware/Divinus</d:Scopes>\n"
            "        <d:XAddrs>%s</d:XAddrs>\n"
            "        <d:MetadataVersion>1</d:MetadataVersion>\n"
            "      </d:ProbeMatch>\n"
            "    </d:ProbeMatches>\n"
            "  </SOAP-ENV:Body>\n"
            "</SOAP-ENV:Envelope>",
            device_uuid, msgid, device_uuid, device_name, device_url);
    
        HAL_INFO("onvif", "Sending discovery response to %s:%d\n", 
                 inet_ntoa(clntaddr.sin_addr), ntohs(clntaddr.sin_port));
    
        if (sendto(servfd, response, strlen(response), 0, (struct sockaddr *)&clntaddr, clntsz) < 0)
            HAL_WARNING("onvif", "Failed to send discovery response: %s\n", strerror(errno));
    }

    close(servfd);
    return (void*)EXIT_SUCCESS;
}

int start_onvif_server() {
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

void stop_onvif_server() {
    pthread_join(onvifPid, NULL);
}