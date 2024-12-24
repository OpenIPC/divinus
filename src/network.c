#include "network.h"

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