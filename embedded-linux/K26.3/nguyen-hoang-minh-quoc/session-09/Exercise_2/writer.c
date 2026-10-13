/**
 * @file writer.c
 * @brief Device Config Writer using mmap
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include "device_cfg.h"

int main(void)
{
    int fd = open(CONFIG_FILE_PATH, O_RDWR | O_CREAT, 0666);
    if (fd < 0) {
        perror("open");
        return EXIT_FAILURE;
    }

    struct stat st;
    if (fstat(fd, &st) < 0) {
        perror("fstat");
        close(fd);
        return EXIT_FAILURE;
    }

    int need_init = (st.st_size < (off_t)sizeof(device_cfg_t));
    if (ftruncate(fd, sizeof(device_cfg_t)) < 0) {
        perror("ftruncate");
        close(fd);
        return EXIT_FAILURE;
    }

    device_cfg_t *cfg = mmap(NULL, sizeof(device_cfg_t),
                             PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    close(fd); /* fd can be closed after mmap */

    if (cfg == MAP_FAILED) {
        perror("mmap");
        return EXIT_FAILURE;
    }

    /* Initialize default values if file was brand new */
    if (need_init) {
        cfg->baud_rate = 9600;
        cfg->sampling_rate_hz = 100;
        cfg->log_level = 2; /* INFO */
        if (msync(cfg, sizeof(device_cfg_t), MS_SYNC) < 0) {
            perror("msync");
        }
    }

    printf("[Config Writer] Loaded %s\n", CONFIG_FILE_PATH);
    printf("Current: baud_rate=%d sampling_rate=%d log_level=%d\n\n",
           cfg->baud_rate, cfg->sampling_rate_hz, cfg->log_level);
    fflush(stdout);

    char line[128];
    while (1) {
        printf("Select field to update [baud/rate/log/quit]: ");
        fflush(stdout);

        if (!fgets(line, sizeof(line), stdin)) {
            break;
        }

        char choice[64] = {0};
        if (sscanf(line, "%63s", choice) != 1) {
            continue;
        }

        if (strcmp(choice, "quit") == 0) {
            break;
        } else if (strcmp(choice, "baud") == 0) {
            printf("Select baud rate [9600/115200/460800]: ");
            fflush(stdout);
            if (fgets(line, sizeof(line), stdin)) {
                int val = 0;
                if (sscanf(line, "%d", &val) == 1 &&
                    (val == 9600 || val == 115200 || val == 460800)) {
                    cfg->baud_rate = val;
                    if (msync(cfg, sizeof(device_cfg_t), MS_SYNC) < 0) {
                        perror("msync");
                    }
                    printf("[Updated] baud_rate = %d\n", val);
                } else {
                    printf("Invalid baud rate!\n");
                }
            }
        } else if (strcmp(choice, "rate") == 0) {
            printf("Enter sampling rate (1-1000 Hz): ");
            fflush(stdout);
            if (fgets(line, sizeof(line), stdin)) {
                int val = 0;
                if (sscanf(line, "%d", &val) == 1 && val >= 1 && val <= 1000) {
                    cfg->sampling_rate_hz = val;
                    if (msync(cfg, sizeof(device_cfg_t), MS_SYNC) < 0) {
                        perror("msync");
                    }
                    printf("[Updated] sampling_rate_hz = %d\n", val);
                } else {
                    printf("Invalid sampling rate! Must be 1-1000.\n");
                }
            }
        } else if (strcmp(choice, "log") == 0) {
            printf("Select log level [0=OFF/1=ERROR/2=INFO/3=DEBUG]: ");
            fflush(stdout);
            if (fgets(line, sizeof(line), stdin)) {
                int val = 0;
                if (sscanf(line, "%d", &val) == 1 && val >= 0 && val <= 3) {
                    cfg->log_level = val;
                    if (msync(cfg, sizeof(device_cfg_t), MS_SYNC) < 0) {
                        perror("msync");
                    }
                    printf("[Updated] log_level = %d\n", val);
                } else {
                    printf("Invalid log level! Must be 0-3.\n");
                }
            }
        } else {
            printf("Unknown field. Options: baud, rate, log, quit\n");
        }
        fflush(stdout);
    }

    if (munmap(cfg, sizeof(device_cfg_t)) < 0) {
        perror("munmap");
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}
