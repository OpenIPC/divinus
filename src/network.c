#include "network.h"

char configured = 0;
struct mdnsd *mdns = {0};
NetInfo netinfo;

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