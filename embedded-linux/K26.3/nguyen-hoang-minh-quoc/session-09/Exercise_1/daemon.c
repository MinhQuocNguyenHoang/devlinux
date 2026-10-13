/**
 * @file daemon.c
 * @brief Sensor Monitor Daemon using SystemV Shared Memory
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <errno.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include "sensor_shm.h"

static volatile sig_atomic_t g_running = 1;
static int g_shmid = -1;
static sensor_data_t *g_shm_ptr = (void *)-1;

static void sigint_handler(int sig)
{
    (void)sig;
    g_running = 0;
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

    /* Create shared memory */
    g_shmid = shmget(SHM_KEY, sizeof(sensor_data_t), IPC_CREAT | 0666);
    if (g_shmid < 0) {
        perror("shmget");
        return EXIT_FAILURE;
    }

    /* Attach shared memory */
    g_shm_ptr = (sensor_data_t *)shmat(g_shmid, NULL, 0);
    if (g_shm_ptr == (void *)-1) {
        perror("shmat");
        shmctl(g_shmid, IPC_RMID, NULL);
        return EXIT_FAILURE;
    }

    printf("[Daemon] Shared memory created. Key=0x%x\n", SHM_KEY);
    fflush(stdout);

    while (g_running) {
        /* Read CPU load */
        double load1 = 0.5;
        FILE *f_load = fopen("/proc/loadavg", "r");
        if (f_load) {
            if (fscanf(f_load, "%lf", &load1) != 1) {
                load1 = 0.5;
            }
            fclose(f_load);
        }
        double temp = 40.0 + load1 * 10.0;

        /* Read Mem info */
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

        double ram_used_pct = 0.0;
        if (mem_total > 0) {
            ram_used_pct = (double)(mem_total - mem_free) / (double)mem_total * 100.0;
        }

        time_t now = time(NULL);

        /* Write to shared memory */
        g_shm_ptr->timestamp = now;
        g_shm_ptr->cpu_temp = temp;
        g_shm_ptr->ram_used_pct = ram_used_pct;

        printf("[Daemon] Written: temp=%.2f ram=%.2f%%\n", temp, ram_used_pct);
        fflush(stdout);

        sleep(2);
    }

    printf("\n[Daemon] Cleaning up shared memory. Goodbye.\n");
    if (g_shm_ptr != (void *)-1) {
        if (shmdt(g_shm_ptr) < 0) {
            perror("shmdt");
        }
    }
    if (g_shmid >= 0) {
        if (shmctl(g_shmid, IPC_RMID, NULL) < 0) {
            perror("shmctl");
        }
    }

    return EXIT_SUCCESS;
}
