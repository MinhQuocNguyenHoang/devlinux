/**
 * @file sender.c
 * @brief Simulated Sensor Telemetry via UDP Internet Socket
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#define COLLECTOR_IP   "127.0.0.1"
#define COLLECTOR_PORT 9000
#define BUFFER_SIZE    128
#define TOTAL_DATAGRAMS 5

int main(void)
{
    int sock_fd = socket(AF_INET, SOCK_DGRAM, 0);
    if (sock_fd < 0) {
        perror("socket");
        return EXIT_FAILURE;
    }

    struct sockaddr_in dest_addr;
    memset(&dest_addr, 0, sizeof(dest_addr));
    dest_addr.sin_family = AF_INET;
    dest_addr.sin_port = htons(COLLECTOR_PORT);
    if (inet_pton(AF_INET, COLLECTOR_IP, &dest_addr.sin_addr) <= 0) {
        perror("inet_pton");
        close(sock_fd);
        return EXIT_FAILURE;
    }

    printf("[Sensor] Target collector: %s:%d\n", COLLECTOR_IP, COLLECTOR_PORT);
    fflush(stdout);

    for (int count = 1; count <= TOTAL_DATAGRAMS; count++) {
        /* Read CPU load average to derive temperature */
        double load1 = 0.5;
        FILE *f_load = fopen("/proc/loadavg", "r");
        if (f_load) {
            if (fscanf(f_load, "%lf", &load1) != 1) {
                load1 = 0.5;
            }
            fclose(f_load);
        }
        double temp = 40.0 + load1 * 10.0;

        /* Read MemTotal and MemFree to calculate memory usage percent */
        long mem_total = 0;
        long mem_free = 0;
        FILE *f_mem = fopen("/proc/meminfo", "r");
        if (f_mem) {
            char line[128];
            while (fgets(line, sizeof(line), f_mem)) {
                if (sscanf(line, "MemTotal: %ld kB", &mem_total) == 1) {
                    continue;
                }
                if (sscanf(line, "MemFree: %ld kB", &mem_free) == 1) {
                    continue;
                }
            }
            fclose(f_mem);
        }

        double mem_used_pct = 0.0;
        if (mem_total > 0) {
            mem_used_pct = (double)(mem_total - mem_free) / (double)mem_total * 100.0;
        }

        char msg[BUFFER_SIZE];
        snprintf(msg, sizeof(msg), "id=sensor-01 temp=%.1f mem_used=%.1f%%", temp, mem_used_pct);

        ssize_t sent_len = sendto(sock_fd, msg, strlen(msg), 0,
                                  (struct sockaddr *)&dest_addr, sizeof(dest_addr));
        if (sent_len < 0) {
            perror("sendto");
            close(sock_fd);
            return EXIT_FAILURE;
        }

        printf("[Sent %d/%d] %s\n", count, TOTAL_DATAGRAMS, msg);
        fflush(stdout);

        if (count < TOTAL_DATAGRAMS) {
            sleep(2);
        }
    }

    printf("[Sensor] Done.\n");
    close(sock_fd);
    return EXIT_SUCCESS;
}
