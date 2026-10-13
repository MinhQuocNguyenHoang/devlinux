/**
 * @file server.c
 * @brief System Monitor Daemon via Unix Domain Socket (Stream)
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <errno.h>
#include <sys/socket.h>
#include <sys/un.h>

#define SOCKET_PATH "/tmp/monitor.sock"
#define BUFFER_SIZE 256

static volatile sig_atomic_t g_running = 1;

static void sigint_handler(int sig)
{
    (void)sig;
    g_running = 0;
}

static void handle_cpu_command(int client_fd)
{
    char resp[BUFFER_SIZE];
    FILE *f = fopen("/proc/loadavg", "r");
    if (!f) {
        snprintf(resp, sizeof(resp), "ERROR: failed to read /proc/loadavg\n");
    } else {
        char load1[32] = {0};
        if (fscanf(f, "%31s", load1) == 1) {
            snprintf(resp, sizeof(resp), "load_avg=%s\n", load1);
        } else {
            snprintf(resp, sizeof(resp), "ERROR: failed to parse /proc/loadavg\n");
        }
        fclose(f);
    }
    ssize_t bytes_sent = send(client_fd, resp, strlen(resp), 0);
    if (bytes_sent < 0) {
        perror("send");
    }
}

static void handle_mem_command(int client_fd)
{
    char resp[BUFFER_SIZE];
    FILE *f = fopen("/proc/meminfo", "r");
    if (!f) {
        snprintf(resp, sizeof(resp), "ERROR: failed to read /proc/meminfo\n");
    } else {
        char line[128];
        long mem_total = -1;
        long mem_free = -1;

        while (fgets(line, sizeof(line), f)) {
            if (sscanf(line, "MemTotal: %ld kB", &mem_total) == 1) {
                continue;
            }
            if (sscanf(line, "MemFree: %ld kB", &mem_free) == 1) {
                continue;
            }
        }
        fclose(f);

        if (mem_total >= 0 && mem_free >= 0) {
            snprintf(resp, sizeof(resp), "mem_total=%ld kB mem_free=%ld kB\n", mem_total, mem_free);
        } else {
            snprintf(resp, sizeof(resp), "ERROR: failed to parse /proc/meminfo\n");
        }
    }
    ssize_t bytes_sent = send(client_fd, resp, strlen(resp), 0);
    if (bytes_sent < 0) {
        perror("send");
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

    /* Unlink stale socket */
    unlink(SOCKET_PATH);

    int server_fd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (server_fd < 0) {
        perror("socket");
        return EXIT_FAILURE;
    }

    struct sockaddr_un addr;
    memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;
    strncpy(addr.sun_path, SOCKET_PATH, sizeof(addr.sun_path) - 1);

    if (bind(server_fd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        perror("bind");
        close(server_fd);
        unlink(SOCKET_PATH);
        return EXIT_FAILURE;
    }

    if (listen(server_fd, 5) < 0) {
        perror("listen");
        close(server_fd);
        unlink(SOCKET_PATH);
        return EXIT_FAILURE;
    }

    printf("[Daemon] Listening on %s...\n", SOCKET_PATH);
    fflush(stdout);

    while (g_running) {
        int client_fd = accept(server_fd, NULL, NULL);
        if (client_fd < 0) {
            if (errno == EINTR) {
                break;
            }
            perror("accept");
            continue;
        }

        printf("[Daemon] Client connected.\n");
        fflush(stdout);

        char buf[BUFFER_SIZE];
        while (g_running) {
            memset(buf, 0, sizeof(buf));
            ssize_t n = recv(client_fd, buf, sizeof(buf) - 1, 0);
            if (n < 0) {
                if (errno == EINTR) {
                    break;
                }
                perror("recv");
                break;
            }
            if (n == 0) {
                /* Client disconnected */
                break;
            }

            /* Strip trailing newline / carriage return */
            buf[n] = '\0';
            char *newline = strpbrk(buf, "\r\n");
            if (newline) {
                *newline = '\0';
            }

            printf("[Daemon] CMD: %s\n", buf);
            fflush(stdout);

            if (strcmp(buf, "cpu") == 0) {
                handle_cpu_command(client_fd);
            } else if (strcmp(buf, "mem") == 0) {
                handle_mem_command(client_fd);
            } else if (strcmp(buf, "quit") == 0) {
                break;
            } else {
                const char *err_msg = "ERROR: unknown command\n";
                if (send(client_fd, err_msg, strlen(err_msg), 0) < 0) {
                    perror("send");
                    break;
                }
            }
        }

        close(client_fd);
        printf("[Daemon] Client disconnected. Waiting for next client...\n");
        fflush(stdout);
    }

    close(server_fd);
    unlink(SOCKET_PATH);
    printf("[Daemon] Terminated cleanly.\n");
    return EXIT_SUCCESS;
}
