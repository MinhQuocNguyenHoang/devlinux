/**
 * @file cli.c
 * @brief Sensor CLI reading from SystemV Shared Memory
 */

#include <stdio.h>
#include <stdlib.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include "sensor_shm.h"

int main(void)
{
    int shmid = shmget(SHM_KEY, sizeof(sensor_data_t), 0666);
    if (shmid < 0) {
        fprintf(stderr, "Daemon is not running.\n");
        return EXIT_FAILURE;
    }

    sensor_data_t *shm_ptr = (sensor_data_t *)shmat(shmid, NULL, 0);
    if (shm_ptr == (void *)-1) {
        perror("shmat");
        return EXIT_FAILURE;
    }

    printf("[Sensor Report]\n");
    printf("Timestamp : %ld\n", (long)shm_ptr->timestamp);
    printf("CPU Temp  : %.2f C\n", shm_ptr->cpu_temp);
    printf("RAM Used  : %.2f %%\n", shm_ptr->ram_used_pct);

    if (shmdt(shm_ptr) < 0) {
        perror("shmdt");
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}
