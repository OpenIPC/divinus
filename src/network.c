#include "network.h"

char configured = 0;
struct mdnsd *mdns = {0};
NetInfo netinfo;

static pthread_t watchPid;
static volatile char watching = 0;

bool network_collect(struct ifaddrs *list, NetInfo *info) {
    NetInfo found = {0};
    int count = 0;

    for (struct ifaddrs *ifa = list; ifa; ifa = ifa->ifa_next) {
        if (count >= 3) break;
        if (!ifa->ifa_addr || ifa->ifa_addr->sa_family != AF_INET) continue;
        if (ifa->ifa_flags & IFF_LOOPBACK) continue;
        struct sockaddr_in *addr = (struct sockaddr_in *)ifa->ifa_addr;
        strncpy(found.intf[count], ifa->ifa_name, sizeof(*found.intf) - 1);
        inet_ntop(AF_INET, &addr->sin_addr.s_addr,
            found.ipaddr[count], sizeof(*found.ipaddr));
        count++;
    }
    found.count = count;

    bool changed = found.count != info->count ||
        memcmp(found.intf, info->intf, sizeof(found.intf)) ||
        memcmp(found.ipaddr, info->ipaddr, sizeof(found.ipaddr));
    if (changed) {
        memcpy(found.host, info->host, sizeof(found.host));
        memcpy(info, &found, sizeof(found));
    }
    return changed;
}

void network_refresh(void) {
    struct ifaddrs *ifaddr;
    if (getifaddrs(&ifaddr) == -1) {
        HAL_WARNING("network", "Failed to get network interfaces!\n");
        return;
    }
    NetInfo updated = netinfo;
    bool changed = network_collect(ifaddr, &updated);
    freeifaddrs(ifaddr);
    if (!changed) return;

    memcpy(&netinfo, &updated, sizeof(netinfo));
    HAL_INFO("network", "Address changed to %s\n",
        netinfo.count ? netinfo.ipaddr[0] : "none");

    if (app_config.mdns_enable) {
        mdns_stop();
        mdns_start();
    }
}

static void *network_watch(void) {
    while (watching && keepRunning) {
        for (int i = 0; i < 5 && watching && keepRunning; i++)
            sleep(1);
        if (watching && keepRunning)
            network_refresh();
    }
    return NULL;
}

void network_init(void) {
    if (configured) return;

    struct utsname uts;
    if (uname(&uts) == -1) {
        HAL_DANGER("network", "Failed to get system information!\n");
        return;
    }
    strcpy(netinfo.host, uts.nodename);

    struct ifaddrs *ifaddr;
    if (getifaddrs(&ifaddr) == -1) {
        HAL_DANGER("network", "Failed to get network interfaces!\n");
        return;
    }
    network_collect(ifaddr, &netinfo);
    freeifaddrs(ifaddr);

    for (int i = 0; i < netinfo.count; i++)
        HAL_INFO("network", "Interface %s has address %s\n",
            netinfo.intf[i], netinfo.ipaddr[i]);

    configured = 1;
}

int network_start(void) {
    network_init();
    if (!configured)
        return EXIT_FAILURE;

    if (app_config.mdns_enable)
        mdns_start();

    if (app_config.onvif_enable)
        start_onvif();

    pthread_attr_t thread_attr;
    pthread_attr_init(&thread_attr);
    size_t stacksize;
    pthread_attr_getstacksize(&thread_attr, &stacksize);
    size_t new_stacksize = 64 * 1024;
    if (pthread_attr_setstacksize(&thread_attr, new_stacksize))
        HAL_DANGER("network", "Can't set stack size %zu\n", new_stacksize);
    watching = 1;
    if (pthread_create(&watchPid, &thread_attr, (void *(*)(void *))network_watch, NULL)) {
        watching = 0;
        HAL_DANGER("network", "Failed to start the address watcher!\n");
    }
    if (pthread_attr_setstacksize(&thread_attr, stacksize))
        HAL_DANGER("network", "Can't set stack size %zu\n", stacksize);
    pthread_attr_destroy(&thread_attr);

    return EXIT_SUCCESS;
}

void network_stop(void) {
    if (watching) {
        watching = 0;
        pthread_join(watchPid, NULL);
    }

    if (app_config.mdns_enable)
        mdns_stop();

    if (app_config.onvif_enable)
        stop_onvif();
}

int mdns_start(void) {
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

    return EXIT_SUCCESS;
}

void mdns_stop(void) {
    if (mdns)
        mdnsd_stop(mdns);

    mdns = NULL;
}
