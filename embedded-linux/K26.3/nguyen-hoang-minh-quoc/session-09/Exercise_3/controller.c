/**
 * @file controller.c
 * @brief Device Controller with POSIX Shared Memory + pthread_mutex (process-shared)
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <pthread.h>

#define SHM_NAME "/device_shm"

typedef struct {
    pthread_mutex_t mutex;
    int status; /* 0 = OFF, 1 = ON */
} device_state_t;

int main(void)
{
    /* Remove stale shared memory if exists */
    shm_unlink(SHM_NAME);

    int shm_fd = shm_open(SHM_NAME, O_CREAT | O_RDWR, 0666);
    if (shm_fd < 0) {
        perror("shm_open");
        return EXIT_FAILURE;
    }

    if (ftruncate(shm_fd, sizeof(device_state_t)) < 0) {
        perror("ftruncate");
        close(shm_fd);
        shm_unlink(SHM_NAME);
        return EXIT_FAILURE;
    }

    device_state_t *state = mmap(NULL, sizeof(device_state_t),
                                 PROT_READ | PROT_WRITE, MAP_SHARED, shm_fd, 0);
    close(shm_fd);

    if (state == MAP_FAILED) {
        perror("mmap");
        shm_unlink(SHM_NAME);
        return EXIT_FAILURE;
    }

    /* Initialize process-shared mutex */
    pthread_mutexattr_t attr;
    if (pthread_mutexattr_init(&attr) != 0) {
        perror("pthread_mutexattr_init");
        munmap(state, sizeof(device_state_t));
        shm_unlink(SHM_NAME);
        return EXIT_FAILURE;
    }

    if (pthread_mutexattr_setpshared(&attr, PTHREAD_PROCESS_SHARED) != 0) {
        perror("pthread_mutexattr_setpshared");
        pthread_mutexattr_destroy(&attr);
        munmap(state, sizeof(device_state_t));
        shm_unlink(SHM_NAME);
        return EXIT_FAILURE;
    }

    if (pthread_mutex_init(&state->mutex, &attr) != 0) {
        perror("pthread_mutex_init");
        pthread_mutexattr_destroy(&attr);
        munmap(state, sizeof(device_state_t));
        shm_unlink(SHM_NAME);
        return EXIT_FAILURE;
    }
    pthread_mutexattr_destroy(&attr);

    /* Initial state: OFF */
    pthread_mutex_lock(&state->mutex);
    state->status = 0;
    pthread_mutex_unlock(&state->mutex);

    printf("[Controller] Shared memory ready. Commands: on / off / quit\n");
    fflush(stdout);

    char line[128];
    while (1) {
        printf("> ");
        fflush(stdout);

        if (!fgets(line, sizeof(line), stdin)) {
            break;
        }

        char cmd[64] = {0};
        if (sscanf(line, "%63s", cmd) != 1) {
            continue;
        }

        if (strcmp(cmd, "on") == 0) {
            pthread_mutex_lock(&state->mutex);
            state->status = 1;
            pthread_mutex_unlock(&state->mutex);
            printf("[Controller] Command sent: ON\n");
        } else if (strcmp(cmd, "off") == 0) {
            pthread_mutex_lock(&state->mutex);
            state->status = 0;
            pthread_mutex_unlock(&state->mutex);
            printf("[Controller] Command sent: OFF\n");
        } else if (strcmp(cmd, "quit") == 0) {
            break;
        } else {
            printf("Unknown command. Valid: on, off, quit\n");
        }
        fflush(stdout);
    }

    printf("[Controller] Cleaning up. Goodbye.\n");
    pthread_mutex_destroy(&state->mutex);
    if (munmap(state, sizeof(device_state_t)) < 0) {
        perror("munmap");
    }
    if (shm_unlink(SHM_NAME) < 0) {
        perror("shm_unlink");
    }

    return EXIT_SUCCESS;
}
