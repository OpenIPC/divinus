#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <time.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <pthread.h>
#include <time.h>
#include <stdbool.h>

#include "../hal/macros.h"

#define PROC_FILENAME "/proc/sstarts"

typedef struct {
    unsigned long      frameNb;
    unsigned long long vsync_timestamp;
    unsigned long long framestart_timestamp;
    unsigned long long frameend_timestamp;
    unsigned long long ispframedone_timestamp;
    unsigned long long vencdone_timestamp;
    unsigned long long one_way_delay_ns;
} air_timestamp_buffer_t;
static air_timestamp_buffer_t timestamps;



static int is_module_loaded(const char *module_name) {
    FILE *fp = fopen("/proc/modules", "r");
    if (!fp) {
        perror("Failed to open /proc/modules");
        return -1;
    }

    char line[256];
    while (fgets(line, sizeof(line), fp)) {
        if (strncmp(line, module_name, strlen(module_name)) == 0) {
            fclose(fp);
            return 1; // Module is loaded
        }
    }

    fclose(fp);
    return 0; // Module is not loaded
}

static int load_sstarts(void) {
    const char *module_name = "sstarts";
    const char *path1 = "/root/sstarts.ko";
    const char *path2 = "/lib/modules/4.9.84/extra/sstarts.ko";
    const char *module_path = NULL;

    // Check if the module is already loaded
    int loaded = is_module_loaded(module_name);
    if (loaded < 0) {
        return 1; // Error occurred while checking
    } else if (loaded == 1) {
        HAL_INFO("TS", "Module '%s' is already loaded. Attempting to remove it...\n", module_name);
        if (system("rmmod sstarts") != 0) {
            perror("Failed to remove kernel module");
            return 1;
        }
        HAL_INFO("TS", "Module '%s' removed successfully.\n", module_name);
    }

    // Check if the module exists at the first path
    if (access(path1, F_OK) == 0) {
        module_path = path1;
    }
    // Check if the module exists at the second path
    else if (access(path2, F_OK) == 0) {
        module_path = path2;
    } else {
        HAL_ERROR("TS", "%s kernel module not found at either path %s or %s\n", module_name, path1, path2);
        return 1;
    }

    // Construct the insmod command
    char command[256];
    snprintf(command, sizeof(command), "insmod %s", module_path);

    // Execute the insmod command
    int ret = system(command);

    // Check the result
    if (ret == 0) {
        HAL_INFO("TS", "Kernel module inserted successfully from %s.\n", module_path);
    } else {
        perror("Failed to insert kernel module sstarts");
    }

    return ret;
}


#define TIMEOUT_US 500          // Timeout en microsecondes

static int sockfd = -1;
static struct sockaddr_in ground_addr;
static bool timestamp_enabled = false;

int timestamp_init(char* ip, unsigned int port_rx, unsigned int port_tx)
{
    HAL_INFO("TS", "Initialize timestamp, ip %s , port_rx %i, port_tx %i\n", ip, port_rx, port_tx);

    load_sstarts();

    // Créer un socket UDP
    if ((sockfd = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
        perror("Failed to create socket");
        return 1;
    }

    struct sockaddr_in air_addr;
    memset(&air_addr, 0, sizeof(air_addr));
    air_addr.sin_family = AF_INET;
    air_addr.sin_addr.s_addr = INADDR_ANY;
    air_addr.sin_port = htons(port_tx);

    if (bind(sockfd, (struct sockaddr *)&air_addr, sizeof(air_addr)) < 0) {
        perror("Failed to bind socket");
        close(sockfd);
        return 1;
    }

    memset(&ground_addr, 0, sizeof(ground_addr));
    ground_addr.sin_family = AF_INET;
    ground_addr.sin_port = htons(port_rx);
    if (inet_pton(AF_INET, ip, &ground_addr.sin_addr) <= 0) {
        perror("Invalid address/Address not supported");
        close(sockfd);
        return 1;
    }

    timestamp_enabled = true;
}

// Define htonll and ntohll functions
uint64_t htonll(uint64_t value) {
    if (htonl(1) != 1) {
        return ((uint64_t)htonl(value & 0xFFFFFFFF) << 32) | htonl(value >> 32);
    } else {
        return value;
    }
}

uint64_t ntohll(uint64_t value) {
    if (ntohl(1) != 1) {
        return ((uint64_t)ntohl(value & 0xFFFFFFFF) << 32) | ntohl(value >> 32);
    } else {
        return value;
    }
}


static int proc_fd = -1;

void timestamp_venc_finished(void) {
    if (timestamp_enabled)
    {
        char buffer[256];
        ssize_t bytes_read;
        unsigned long long current_time_ns;
        unsigned long long acquisition_time_ns;
        unsigned long long isp_processing_time_ns;
        unsigned long long vpe_venc_time_ns;
        unsigned long ispframedone_nb;

        struct timespec ts;
        loff_t pos = 0;

        // Obtenir le temps courant
        clock_gettime(CLOCK_MONOTONIC, &ts);
        current_time_ns = ts.tv_sec * 1000000000ULL + ts.tv_nsec;
        timestamps.vencdone_timestamp = current_time_ns;

        // Ouvrir le fichier /proc/sstarts si nécessaire
        if (proc_fd < 0) {
            proc_fd = open(PROC_FILENAME, O_RDONLY);
            if (proc_fd < 0) {
                perror("Failed to open /proc/sstarts");
                return;
            }
        }

        lseek(proc_fd, 0, SEEK_SET);
        bytes_read = read(proc_fd, buffer, sizeof(buffer) - 1);
        if (bytes_read < 0) {
            perror("Failed to read /proc/sstarts");
            close(proc_fd);
            proc_fd = -1;
            return;
        }
        buffer[bytes_read] = '\0';

        // Parse the timestamps
        sscanf(buffer, "Frame: %u, VSync: %llu ns, FrameStart: %llu ns, FrameEnd: %llu ns, ISPFrameDone: %llu ns\n",
            &ispframedone_nb,
            &timestamps.vsync_timestamp,
            &timestamps.framestart_timestamp,
            &timestamps.frameend_timestamp,
            &timestamps.ispframedone_timestamp);


        // Calculer les temps
        acquisition_time_ns = timestamps.frameend_timestamp - timestamps.vsync_timestamp;
        isp_processing_time_ns = timestamps.ispframedone_timestamp - timestamps.frameend_timestamp;
        vpe_venc_time_ns = timestamps.vencdone_timestamp - timestamps.ispframedone_timestamp;

        // Publier les résultats
        HAL_DEBUG("TS", "Acquisition Time: %llu ns\n", acquisition_time_ns);
        HAL_DEBUG("TS", "ISP Processing Time: %llu ns\n", isp_processing_time_ns);
        HAL_DEBUG("TS", "VPE + VENC Time: %llu ns\n", vpe_venc_time_ns);
    }
}


void timestamp_send_finished(unsigned long frameNb)
{
    if (timestamp_enabled)
    {
        struct timespec ts;
        unsigned long long air_time_ns, air_time_ns_network, rtt_ns, ground_time_ns;
        socklen_t addr_len = sizeof(ground_addr);
        char buffer[256];

        // Capturer le temps "air"
        clock_gettime(CLOCK_MONOTONIC, &ts);
        air_time_ns = ts.tv_sec * 1000000000ULL + ts.tv_nsec;

        // Envoyer le temps "air" au système "ground"
        air_time_ns_network = htonll(air_time_ns);
        if (sendto(sockfd, &air_time_ns_network, sizeof(air_time_ns_network), 0, (struct sockaddr *)&ground_addr, sizeof(ground_addr)) < 0) {
            perror("Failed to send data");
            return;
        }

        // Recevoir la réponse du système "ground" avec timeout
        struct timeval timeout;
        timeout.tv_sec = 0;
        timeout.tv_usec = TIMEOUT_US;
        setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));

        if (recvfrom(sockfd, &ground_time_ns, sizeof(ground_time_ns), 0, (struct sockaddr *)&ground_addr, &addr_len) < 0) {
            if (errno == EWOULDBLOCK || errno == EAGAIN) {
                printf("Receive timeout\n");
            } else {
                perror("Failed to receive data");
            }
            return;
        }

        // Capturer le temps de réception de la réponse
        clock_gettime(CLOCK_MONOTONIC, &ts);
        rtt_ns = ts.tv_sec * 1000000000ULL + ts.tv_nsec - air_time_ns;

        // convertir le temps de réception du système "ground" en réseau en local
        ground_time_ns = ntohll(ground_time_ns);

        // Calculer le temps de traversée unidirectionnel
        timestamps.one_way_delay_ns = rtt_ns / 2;

        
        HAL_DEBUG("TS",  "Packet sent %lu =>\n", frameNb);
        HAL_DEBUG("TS",  "AirTime:                %llu us\n", air_time_ns / 1000);
        HAL_DEBUG("TS",  "GroundTime:             %llu us\n", ground_time_ns / 1000);
        HAL_DEBUG("TS",  "Vsync:                  %llu us\n", timestamps.vsync_timestamp / 1000);
        HAL_DEBUG("TS",  "ISP Done:               %llu us\n", timestamps.ispframedone_timestamp / 1000);
        HAL_DEBUG("TS",  "Venc Done:              %llu us\n", timestamps.vencdone_timestamp / 1000);
        HAL_DEBUG("TS",  "Air One Way Delay:      %llu us\n", timestamps.one_way_delay_ns / 1000);

        timestamps.frameNb = htonl(frameNb);
        timestamps.vsync_timestamp = htonll(timestamps.vsync_timestamp);
        timestamps.framestart_timestamp = htonll(timestamps.framestart_timestamp);
        timestamps.frameend_timestamp = htonll(timestamps.frameend_timestamp);
        timestamps.ispframedone_timestamp = htonll(timestamps.ispframedone_timestamp);
        timestamps.vencdone_timestamp = htonll(timestamps.vencdone_timestamp);
        timestamps.one_way_delay_ns = htonll(timestamps.one_way_delay_ns);

        if (sendto(sockfd, &timestamps, sizeof(timestamps), 0, (struct sockaddr *)&ground_addr, sizeof(ground_addr)) < 0) {
            perror("Failed to send data");
            return;
        }
    }
}


void timestamp_deinit(void)
{
    timestamp_enabled = false;
    // Fermer le fichier à la fin du programme
    if (proc_fd >= 0) {
        close(proc_fd);
    }
    if (sockfd >= 0) {
        close(sockfd);
    }
}