#include "network.h"

IMPORT_STR(.rodata, "../res/onvif/discovery.xml", discoveryxml);
extern const char discoveryxml[];

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

    strcat(hostname, netinfo.host);
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
        clntsz = sizeof(clntaddr);
        if ((reqLen = recvfrom(servfd, request, sizeof(request), 0, (struct sockaddr *)&clntaddr, &clntsz)) < 0)
            continue;

        request[reqLen] = '\0';
#ifdef DEBUG_ONVIF
        HAL_INFO("onvif", "Received message: %s\n", msgbuf);
#endif

        if (!strstr(request, "http://schemas.xmlsoap.org/ws/2005/04/discovery/Probe"))
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