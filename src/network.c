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

    struct ip_mreq group;
    group.imr_multiaddr.s_addr = inet_addr("239.255.255.250");
    group.imr_interface.s_addr = inet_addr(ipaddr);
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

        printf("Received message: %s\n", msgbuf);

        if (!strstr(msgbuf, "http://schemas.xmlsoap.org/ws/2005/04/discovery/Probe"))
            continue;

        char uuid[37], response[2048] = {0};
        uuid_generate(uuid);
        sendto(servfd, response, strlen(response), 0, (struct sockaddr *)&clntaddr, clntsz);
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
    if (pthread_attr_setstacksize(&thread_attr, new_stacksize)) {
        HAL_DANGER("onvif", "Error:  Can't set stack size %zu\n", new_stacksize);
    }
    pthread_create(&onvifPid, &thread_attr, (void *(*)(void *))onvif_thread, NULL);
    if (pthread_attr_setstacksize(&thread_attr, stacksize)) {
        HAL_DANGER("onvif", "Error:  Can't set stack size %zu\n", stacksize);
    }
    pthread_attr_destroy(&thread_attr);
}

void stop_onvif_server() {
    pthread_join(onvifPid, NULL);
}