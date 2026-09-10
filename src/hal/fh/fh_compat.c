#if defined(__arm__) && !defined(__ARM_PCS_VFP) && __ARM_ARCH == 6

/*
 * The Fullhan 3.0.8 kernels oops in rtnl_fill_ifinfo() on any netlink
 * interface dump, which kills the calling process. musl implements
 * getifaddrs() over netlink, so replace it with the SIOCGIFCONF ioctl
 * interface for this platform.
 */
#include <errno.h>
#include <ifaddrs.h>
#include <net/if.h>
#include <netinet/in.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <unistd.h>

struct fh_ifaddr {
    struct ifaddrs ifa;
    char name[IFNAMSIZ];
    struct sockaddr_in addr, netmask, broadcast;
};

int getifaddrs(struct ifaddrs **ifap)
{
    struct ifconf ifc;
    struct ifreq *reqs;
    int fd, count, len = 32 * sizeof(struct ifreq);
    struct fh_ifaddr *list = NULL, *last = NULL;

    *ifap = NULL;
    if ((fd = socket(AF_INET, SOCK_DGRAM, 0)) < 0)
        return -1;
    if (!(reqs = malloc(len))) {
        close(fd);
        return -1;
    }
    ifc.ifc_len = len;
    ifc.ifc_req = reqs;
    if (ioctl(fd, SIOCGIFCONF, &ifc) < 0) {
        free(reqs);
        close(fd);
        return -1;
    }
    count = ifc.ifc_len / sizeof(struct ifreq);

    for (int i = 0; i < count; i++) {
        struct ifreq req = reqs[i];
        struct fh_ifaddr *entry = calloc(1, sizeof(*entry));
        if (!entry) break;
        strncpy(entry->name, req.ifr_name, IFNAMSIZ - 1);
        entry->ifa.ifa_name = entry->name;
        memcpy(&entry->addr, &req.ifr_addr, sizeof(entry->addr));
        entry->ifa.ifa_addr = (struct sockaddr*)&entry->addr;
        if (!ioctl(fd, SIOCGIFFLAGS, &req))
            entry->ifa.ifa_flags = req.ifr_flags;
        if (!ioctl(fd, SIOCGIFNETMASK, &req)) {
            memcpy(&entry->netmask, &req.ifr_netmask, sizeof(entry->netmask));
            entry->ifa.ifa_netmask = (struct sockaddr*)&entry->netmask;
        }
        if (!ioctl(fd, SIOCGIFBRDADDR, &req)) {
            memcpy(&entry->broadcast, &req.ifr_broadaddr, sizeof(entry->broadcast));
            entry->ifa.ifa_broadaddr = (struct sockaddr*)&entry->broadcast;
        }
        if (last) last->ifa.ifa_next = &entry->ifa;
        else list = entry;
        last = entry;
    }

    free(reqs);
    close(fd);
    *ifap = list ? &list->ifa : NULL;
    return 0;
}

void freeifaddrs(struct ifaddrs *ifa)
{
    while (ifa) {
        struct ifaddrs *next = ifa->ifa_next;
        free(ifa);
        ifa = next;
    }
}

#endif
