/**
 * @file device.c
 * @brief Device process reading status with pthread_mutex synchronization
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <signal.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <pthread.h>

#define SHM_NAME "/device_shm"

typedef struct {
    pthread_mutex_t mutex;
    int status; /* 0 = OFF, 1 = ON */
} device_state_t;

static volatile sig_atomic_t g_running = 1;

static void sigint_handler(int sig)
{
    (void)sig;
    g_running = 0;
}

int main(void)
{
    struct sigaction sa;
    sa.sa_handler = sigint_handler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;
    if (sigaction(SIGINT, &sa, NULL) < 0) {
        perror("sigaction");
        return EXIT_FAILURE;
    }

    int shm_fd = shm_open(SHM_NAME, O_RDWR, 0666);
    if (shm_fd < 0) {
        perror("shm_open");
        fprintf(stderr, "Please run controller first to create %s\n", SHM_NAME);
        return EXIT_FAILURE;
    }

    device_state_t *state = mmap(NULL, sizeof(device_state_t),
                                 PROT_READ | PROT_WRITE, MAP_SHARED, shm_fd, 0);
    close(shm_fd);

    if (state == MAP_FAILED) {
        perror("mmap");
        return EXIT_FAILURE;
    }

    printf("[Device] Attached to %s\n", SHM_NAME);
    fflush(stdout);

    while (g_running) {
        pthread_mutex_lock(&state->mutex);
        int current_status = state->status;
        pthread_mutex_unlock(&state->mutex);

        if (current_status == 1) {
            printf("[Device] Status: ON  — Running...\n");
        } else {
            printf("[Device] Status: OFF — Idle.\n");
        }
        fflush(stdout);

        sleep(1);
    }

    printf("\n[Device] Exiting...\n");
    if (munmap(state, sizeof(device_state_t)) < 0) {
        perror("munmap");
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}
