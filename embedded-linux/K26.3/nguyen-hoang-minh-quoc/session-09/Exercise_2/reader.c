/**
 * @file reader.c
 * @brief Device Config Reader using mmap
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <signal.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include "device_cfg.h"

static volatile sig_atomic_t g_running = 1;

static void sigint_handler(int sig)
{
    (void)sig;
    g_running = 0;
}

static const char *log_level_to_string(int level)
{
    switch (level) {
    case 0: return "OFF";
    case 1: return "ERROR";
    case 2: return "INFO";
    case 3: return "DEBUG";
    default: return "UNKNOWN";
    }
}

int main(void)
{
    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = sigint_handler;
    sigemptyset(&sa.sa_mask);
    if (sigaction(SIGINT, &sa, NULL) < 0) {
        perror("sigaction");
        return EXIT_FAILURE;
    }

    int fd = open(CONFIG_FILE_PATH, O_RDONLY);
    if (fd < 0) {
        perror("open");
        fprintf(stderr, "Please make sure %s exists and is created by writer.\n", CONFIG_FILE_PATH);
        return EXIT_FAILURE;
    }

    struct stat st;
    if (fstat(fd, &st) < 0 || st.st_size < (off_t)sizeof(device_cfg_t)) {
        perror("fstat / invalid size");
        close(fd);
        return EXIT_FAILURE;
    }

    device_cfg_t *cfg = mmap(NULL, sizeof(device_cfg_t), PROT_READ, MAP_SHARED, fd, 0);
    close(fd);

    if (cfg == MAP_FAILED) {
        perror("mmap");
        return EXIT_FAILURE;
    }

    printf("[Config Reader] Polling %s every 2s...\n", CONFIG_FILE_PATH);
    fflush(stdout);

    while (g_running) {
        printf("baud_rate=%d  sampling_rate=%d Hz  log_level=%s\n",
               cfg->baud_rate,
               cfg->sampling_rate_hz,
               log_level_to_string(cfg->log_level));
        fflush(stdout);

        sleep(2);
    }

    printf("\n[Config Reader] Stopping...\n");
    if (munmap(cfg, sizeof(device_cfg_t)) < 0) {
        perror("munmap");
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}
