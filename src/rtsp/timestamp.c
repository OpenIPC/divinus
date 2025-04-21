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
#include <stdatomic.h>

#include "../hal/macros.h"

#define PROC_FILENAME "/proc/sstarts"

#define PACKET_MAGIC 0xA1B2C3D4
#define TIMEOUT_US 5000          // Timeout in microseconds


typedef struct {
    unsigned long      frameNb;
    unsigned long long vsync_timestamp;
    unsigned long long framestart_timestamp;
    unsigned long long frameend_timestamp;
    unsigned long long ispframedone_timestamp;
    unsigned long long vencdone_timestamp;
    unsigned long long one_way_delay_ns;
    unsigned long long air_time_ns;
} air_timestamp_buffer_t;
static air_timestamp_buffer_t timestamps;

typedef enum {
    PACKET_TYPE_AIR_TIME,
    PACKET_TYPE_AIR_TIMESTAMPS
} packet_type_t;

typedef struct {
    uint32_t magic; // Magic number for validation
    packet_type_t type;
    union {
        uint64_t air_time_ns;
        air_timestamp_buffer_t air_timestamps;
    } data;
} air_packet_t;

static air_packet_t air_packet;
static void *ts_thread_func(void *arg);
static int rcv_sockfd = -1;
static struct sockaddr_in ground_addr;
static bool timestamp_enabled = false;
static pthread_t ts_thread;
static atomic_bool ts_thread_ready = false;
static unsigned long current_frameNb;
static int proc_fd = -1;

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


void timestamp_venc_finished(void) {
    if (timestamp_enabled)
    {
        unsigned long ispframedone_nb;
        unsigned long long current_time_ns;
        struct timespec ts;
        ssize_t bytes_read;
        char buffer[256];

        // Obtenir le temps courant
        clock_gettime(CLOCK_MONOTONIC, &ts);
        current_time_ns = ts.tv_sec * 1000000000ULL + ts.tv_nsec;
        timestamps.vencdone_timestamp = current_time_ns;

        // get timestamp from the kernel module
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
    }
}

// Helper function to send air_packet_t
static int send_air_packet(air_packet_t *packet, size_t packet_size) {
    int sockfd;
    struct sockaddr_in dest_addr;
    const char *message = "Votre message";

    // Créer un socket UDP
    if ((sockfd = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
        perror("Failed to create socket");
        return 1;
    }

    // Configurer l'adresse de destination
    memset(&dest_addr, 0, sizeof(dest_addr));
    dest_addr.sin_family = AF_INET;
    dest_addr.sin_port = htons(12345);
    inet_pton(AF_INET, "127.0.0.1", &dest_addr.sin_addr);

    // Envoyer le message
    if (sendto(sockfd, packet, packet_size, 0, (struct sockaddr *)&dest_addr, sizeof(dest_addr)) < 0) {
        perror("Failed to send message");
        close(sockfd);
        return 1;
    }

    close(sockfd);
    return 0;

}


void timestamp_send_finished(unsigned long frameNb)
{
    if (timestamp_enabled)
    {
        current_frameNb = frameNb;
        // send is finished, we can tell the thread to send the packet
        atomic_store_explicit(&ts_thread_ready, true, memory_order_release);
    }
}


void *ts_thread_func(void *arg) {

    unsigned long long acquisition_time_ns;
    unsigned long long isp_processing_time_ns;
    unsigned long long vpe_venc_time_ns;
    struct timespec ts;
    unsigned long long air_time_ns, rtt_ns, ground_time_ns;
    socklen_t addr_len = sizeof(ground_addr);


    while (timestamp_enabled)
    {
        // wait until the thread is ready
        if (!atomic_load_explicit(&ts_thread_ready, memory_order_acquire)) {
            usleep(100);
        }
        else
        {
            // Reinitialize the thread ready flag
            atomic_store_explicit(&ts_thread_ready, false, memory_order_release);

            // Compute latency
            acquisition_time_ns = timestamps.frameend_timestamp - timestamps.vsync_timestamp;
            isp_processing_time_ns = timestamps.ispframedone_timestamp - timestamps.frameend_timestamp;
            vpe_venc_time_ns = timestamps.vencdone_timestamp - timestamps.ispframedone_timestamp;

            // print it
            HAL_DEBUG("TS", "Acquisition Time: %llu ns\n", acquisition_time_ns);
            HAL_DEBUG("TS", "ISP Processing Time: %llu ns\n", isp_processing_time_ns);
            HAL_DEBUG("TS", "VPE + VENC Time: %llu ns\n", vpe_venc_time_ns);

            // Synchronization mechanism with ground system
            // - Capture air time
            clock_gettime(CLOCK_MONOTONIC, &ts);
            air_time_ns = ts.tv_sec * 1000000000ULL + ts.tv_nsec;

            // - Prepare and send air_time_ns packet
            air_packet_t air_time_packet;
            air_time_packet.magic = htonl(PACKET_MAGIC);
            air_time_packet.type = htonl(PACKET_TYPE_AIR_TIME);
            air_time_packet.data.air_time_ns = htonll(air_time_ns);

            if (send_air_packet(&air_time_packet, sizeof(air_time_packet)) < 0) {
                continue;
            }

            // - Receive response from ground system with timeout
            struct timeval timeout = { .tv_sec = 0, .tv_usec = TIMEOUT_US };
            setsockopt(rcv_sockfd, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));

            if (recvfrom(rcv_sockfd, &ground_time_ns, sizeof(ground_time_ns), 0, (struct sockaddr *)&ground_addr, &addr_len) < 0) {
                if (errno == EWOULDBLOCK || errno == EAGAIN) {
                    //printf("Receive timeout, setting ground_time_ns to 0\n");
                    ground_time_ns = 0;
                } else {
                    perror("Failed to receive data");
                    ground_time_ns = 0;
                }
            }

            if (ground_time_ns) {
                // Capture response time and calculate RTT
                clock_gettime(CLOCK_MONOTONIC, &ts);
                rtt_ns = ts.tv_sec * 1000000000ULL + ts.tv_nsec - air_time_ns;

                // Convert ground time to host byte order
                ground_time_ns = ntohll(ground_time_ns);

                // Calculate one-way delay
                timestamps.one_way_delay_ns = rtt_ns / 2;
            } else {
                timestamps.one_way_delay_ns = 0;
            }

            HAL_DEBUG("TS", "Packet sent %lu =>\n", current_frameNb);
            HAL_DEBUG("TS", "AirTime:                %llu us\n", air_time_ns / 1000);
            HAL_DEBUG("TS", "GroundTime:             %llu us\n", ground_time_ns / 1000);
            HAL_DEBUG("TS", "Vsync:                  %llu us\n", timestamps.vsync_timestamp / 1000);
            HAL_DEBUG("TS", "ISP Done:               %llu us\n", timestamps.ispframedone_timestamp / 1000);
            HAL_DEBUG("TS", "Venc Done:              %llu us\n", timestamps.vencdone_timestamp / 1000);
            HAL_DEBUG("TS", "Air One Way Delay:      %llu us\n", timestamps.one_way_delay_ns / 1000);

            // - Prepare and send timestamps packet
            air_packet_t timestamp_packet;
            timestamp_packet.magic = htonl(PACKET_MAGIC);
            timestamp_packet.type = htonl(PACKET_TYPE_AIR_TIMESTAMPS);

            timestamp_packet.data.air_timestamps.frameNb = htonl(current_frameNb);
            timestamp_packet.data.air_timestamps.vsync_timestamp = htonll(timestamps.vsync_timestamp);
            timestamp_packet.data.air_timestamps.framestart_timestamp = htonll(timestamps.framestart_timestamp);
            timestamp_packet.data.air_timestamps.frameend_timestamp = htonll(timestamps.frameend_timestamp);
            timestamp_packet.data.air_timestamps.ispframedone_timestamp = htonll(timestamps.ispframedone_timestamp);
            timestamp_packet.data.air_timestamps.vencdone_timestamp = htonll(timestamps.vencdone_timestamp);
            timestamp_packet.data.air_timestamps.one_way_delay_ns = htonll(timestamps.one_way_delay_ns);

            clock_gettime(CLOCK_MONOTONIC, &ts);
            air_time_ns = ts.tv_sec * 1000000000ULL + ts.tv_nsec;
            timestamp_packet.data.air_timestamps.air_time_ns = htonll(air_time_ns);

            send_air_packet(&timestamp_packet, sizeof(timestamp_packet));
        }
    }
}


int timestamp_init(char* ip, unsigned int port_tx, unsigned int port_rx)
{
    HAL_INFO("TS", "Initialize timestamp, ip %s , port_rx %i, port_tx %i\n", ip, port_rx, port_tx);
    
    load_sstarts();

    // Créer un socket UDP
    if ((rcv_sockfd = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
        perror("Failed to create socket");
        return 1;
    }

    struct sockaddr_in air_addr;
    memset(&air_addr, 0, sizeof(air_addr));
    air_addr.sin_family = AF_INET;
    air_addr.sin_addr.s_addr = INADDR_ANY;
    air_addr.sin_port = htons(port_rx);

    if (bind(rcv_sockfd, (struct sockaddr *)&air_addr, sizeof(air_addr)) < 0) {
        perror("Failed to bind socket");
        close(rcv_sockfd);
        return 1;
    }

    timestamp_enabled = true;

	// Créer et démarrer le thread pour le système "ground"
	if (pthread_create(&ts_thread, NULL, ts_thread_func, NULL) != 0) {
		perror("Failed to create ts thread");
        close(rcv_sockfd);
		return 1;
	}
}

void timestamp_deinit(void)
{
    timestamp_enabled = false;

    pthread_join(ts_thread, NULL);

    // Fermer le fichier à la fin du programme
    if (proc_fd >= 0) {
        close(proc_fd);
    }
    if (rcv_sockfd >= 0) {
        close(rcv_sockfd);
    }
}