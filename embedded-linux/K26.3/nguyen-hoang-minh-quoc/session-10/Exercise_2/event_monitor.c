/**
 * @file event_monitor.c
 * @brief Multi-Source System Event Monitor using poll()
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <signal.h>
#include <errno.h>
#include <time.h>
#include <poll.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <sys/un.h>
#include "event_monitor.h"

static volatile sig_atomic_t g_running = 1;

static void sigterm_handler(int sig)
{
    (void)sig;
    g_running = 0;
}

int main(void)
{
    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = sigterm_handler;
    sigemptyset(&sa.sa_mask);
    if (sigaction(SIGTERM, &sa, NULL) < 0) {
        perror("sigaction SIGTERM");
        return EXIT_FAILURE;
    }
    if (sigaction(SIGINT, &sa, NULL) < 0) {
        perror("sigaction SIGINT");
        return EXIT_FAILURE;
    }

    /* 1. Named pipe (FIFO) */
    if (mkfifo(FIFO_PATH, 0666) < 0 && errno != EEXIST) {
        perror("mkfifo");
        return EXIT_FAILURE;
    }

    /* Open FIFO with O_RDWR | O_NONBLOCK to prevent blocking on open/read and avoiding EOF busy loop */
    int fifo_fd = open(FIFO_PATH, O_RDWR | O_NONBLOCK);
    if (fifo_fd < 0) {
        perror("open FIFO");
        return EXIT_FAILURE;
    }

    /* 2. Unix Domain Socket */
    unlink(SOCKET_PATH);
    int sock_fd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (sock_fd < 0) {
        perror("socket");
        close(fifo_fd);
        return EXIT_FAILURE;
    }

    struct sockaddr_un sock_addr;
    memset(&sock_addr, 0, sizeof(sock_addr));
    sock_addr.sun_family = AF_UNIX;
    strncpy(sock_addr.sun_path, SOCKET_PATH, sizeof(sock_addr.sun_path) - 1);

    if (bind(sock_fd, (struct sockaddr *)&sock_addr, sizeof(sock_addr)) < 0) {
        perror("bind socket");
        close(fifo_fd);
        close(sock_fd);
        unlink(SOCKET_PATH);
        return EXIT_FAILURE;
    }

    if (listen(sock_fd, 5) < 0) {
        perror("listen socket");
        close(fifo_fd);
        close(sock_fd);
        unlink(SOCKET_PATH);
        return EXIT_FAILURE;
    }

    /* 3. Regular file */
    int file_fd = open(FILE_PATH, O_CREAT | O_RDWR, 0666);
    if (file_fd < 0) {
        perror("open FILE");
        close(fifo_fd);
        close(sock_fd);
        unlink(SOCKET_PATH);
        return EXIT_FAILURE;
    }

    struct stat st;
    off_t last_file_size = 0;
    if (fstat(file_fd, &st) == 0) {
        last_file_size = st.st_size;
    }

    printf("[Monitor] Listening on %s\n", SOCKET_PATH);
    printf("[Monitor] Monitoring %s (FIFO) and %s\n", FIFO_PATH, FILE_PATH);
    fflush(stdout);

    struct pollfd fds[NUM_POLL_FDS];
    fds[FD_FIFO].fd = fifo_fd;
    fds[FD_FIFO].events = POLLIN;

    fds[FD_SOCKET_LISTENER].fd = sock_fd;
    fds[FD_SOCKET_LISTENER].events = POLLIN;

    fds[FD_FILE].fd = file_fd;
    fds[FD_FILE].events = 0;

    int total_events = 0;
    int is_active = 1;
    time_t start_time = time(NULL);

    while (g_running) {
        int ret = poll(fds, NUM_POLL_FDS, POLL_TIMEOUT_MS);
        if (ret < 0) {
            if (errno == EINTR) {
                break;
            }
            perror("poll");
            break;
        }

        if (ret == 0) {
            /* Heartbeat on timeout */
            printf("[HEARTBEAT] Monitor alive, events_seen=%d\n", total_events);
            fflush(stdout);
        }

        /* Check FIFO */
        if (fds[FD_FIFO].revents & POLLIN) {
            char buf[BUFFER_SIZE];
            ssize_t n = read(fifo_fd, buf, sizeof(buf) - 1);
            if (n > 0) {
                buf[n] = '\0';
                /* Might contain multiple lines */
                char *line = strtok(buf, "\r\n");
                while (line) {
                    if (is_active) {
                        printf("[FIFO_EVENT] %s\n", line);
                        fflush(stdout);
                        total_events++;
                    }
                    line = strtok(NULL, "\r\n");
                }
            }
        }

        /* Check Socket */
        if (fds[FD_SOCKET_LISTENER].revents & POLLIN) {
            int client_fd = accept(sock_fd, NULL, NULL);
            if (client_fd >= 0) {
                char cmd_buf[BUFFER_SIZE];
                memset(cmd_buf, 0, sizeof(cmd_buf));
                ssize_t n = recv(client_fd, cmd_buf, sizeof(cmd_buf) - 1, 0);
                if (n > 0) {
                    cmd_buf[n] = '\0';
                    char *newline = strpbrk(cmd_buf, "\r\n");
                    if (newline) {
                        *newline = '\0';
                    }

                    char resp[BUFFER_SIZE];
                    if (strcmp(cmd_buf, "STATUS") == 0) {
                        time_t now = time(NULL);
                        long uptime = (long)(now - start_time);
                        snprintf(resp, sizeof(resp), "Total events: %d, Uptime: %ld seconds\n",
                                 total_events, uptime);
                        send(client_fd, resp, strlen(resp), 0);
                    } else if (strcmp(cmd_buf, "START") == 0) {
                        is_active = 1;
                        snprintf(resp, sizeof(resp), "OK\n");
                        send(client_fd, resp, strlen(resp), 0);
                    } else if (strcmp(cmd_buf, "STOP") == 0) {
                        is_active = 0;
                        snprintf(resp, sizeof(resp), "OK\n");
                        send(client_fd, resp, strlen(resp), 0);
                    } else if (strcmp(cmd_buf, "EXIT") == 0) {
                        snprintf(resp, sizeof(resp), "OK\n");
                        send(client_fd, resp, strlen(resp), 0);
                    } else {
                        snprintf(resp, sizeof(resp), "ERROR: Unknown command\n");
                        send(client_fd, resp, strlen(resp), 0);
                    }
                }
                close(client_fd);
            }
        }

        /* Check File size change */
        if (stat(FILE_PATH, &st) == 0) {
            if (st.st_size != last_file_size) {
                last_file_size = st.st_size;
                if (is_active) {
                    printf("[FILE_EVENT] %s size changed to %ld bytes\n",
                           FILE_PATH, (long)last_file_size);
                    fflush(stdout);
                    total_events++;
                }
            }
        }
    }

    close(fifo_fd);
    close(sock_fd);
    close(file_fd);
    unlink(SOCKET_PATH);

    printf("[Monitor] Shutdown complete.\n");
    return EXIT_SUCCESS;
}
