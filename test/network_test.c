#include "check.h"
#include "network.h"

volatile char graceful = 0, keepRunning = 1;
struct AppConfig app_config;

struct mdnsd *mdnsd_start(void) { return NULL; }
void mdnsd_stop(struct mdnsd *svr) {}
void mdnsd_set_hostname(struct mdnsd *svr, const char *hostname, uint32_t ip) {}
void mdnsd_add_rr(struct mdnsd *svr, struct rr_entry *rr) {}
struct rr_entry *rr_create_a(uint8_t *name, uint32_t addr) { return NULL; }
uint8_t *create_nlabel(const char *txt) { return NULL; }
int start_onvif(void) { return 0; }
void stop_onvif(void) {}

static struct sockaddr_in loAddr, wlanAddr;
static struct ifaddrs lo, wlan;

static void build_list(const char *wlanIp) {
    loAddr.sin_family = AF_INET;
    inet_pton(AF_INET, "127.0.0.1", &loAddr.sin_addr);
    lo = (struct ifaddrs){ .ifa_next = &wlan, .ifa_name = "lo",
        .ifa_flags = IFF_UP | IFF_LOOPBACK, .ifa_addr = (struct sockaddr *)&loAddr };
    wlanAddr.sin_family = AF_INET;
    inet_pton(AF_INET, wlanIp, &wlanAddr.sin_addr);
    wlan = (struct ifaddrs){ .ifa_name = "wlan0",
        .ifa_flags = IFF_UP, .ifa_addr = (struct sockaddr *)&wlanAddr };
}

int main(void) {
    NetInfo info = {0};

    build_list("192.168.1.10");
    CHECK(network_collect(&lo, &info));
    CHECK(info.count == 1);
    CHECK(!strcmp(info.intf[0], "wlan0"));
    CHECK(!strcmp(info.ipaddr[0], "192.168.1.10"));

    CHECK(!network_collect(&lo, &info));

    build_list("192.168.1.100");
    CHECK(network_collect(&lo, &info));
    CHECK(info.count == 1);
    CHECK(!strcmp(info.ipaddr[0], "192.168.1.100"));

    CHECK(network_collect(NULL, &info));
    CHECK(info.count == 0);

    CHECK_DONE();
}
