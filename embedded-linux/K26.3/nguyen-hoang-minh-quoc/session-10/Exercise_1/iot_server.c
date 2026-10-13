/**
 * @file iot_server.c
 * @brief Multi-Client IoT Server using select()
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <errno.h>
#include <time.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include "iot_server.h"

static volatile sig_atomic_t g_running = 1;

static void sigint_handler(int sig)
{
    (void)sig;
    g_running = 0;
}

int main(void)
{
    srand((unsigned int)time(NULL));

    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = sigint_handler;
    sigemptyset(&sa.sa_mask);
    if (sigaction(SIGINT, &sa, NULL) < 0) {
        perror("sigaction");
        return EXIT_FAILURE;
    }

    int listen_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (listen_fd < 0) {
        perror("socket");
        return EXIT_FAILURE;
    }

    int opt = 1;
    if (setsockopt(listen_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
        perror("setsockopt");
        close(listen_fd);
        return EXIT_FAILURE;
    }

    struct sockaddr_in serv_addr;
    memset(&serv_addr, 0, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    serv_addr.sin_port = htons(SERVER_PORT);

    if (bind(listen_fd, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
        perror("bind");
        close(listen_fd);
        return EXIT_FAILURE;
    }

    if (listen(listen_fd, 5) < 0) {
        perror("listen");
        close(listen_fd);
        return EXIT_FAILURE;
    }

    printf("[Server] Listening on localhost:%d\n", SERVER_PORT);
    fflush(stdout);

    client_t clients[MAX_CLIENTS];
    for (int i = 0; i < MAX_CLIENTS; i++) {
        clients[i].fd = -1;
        clients[i].mode = 0;
        clients[i].last_activity = 0;
    }

    time_t last_broadcast = time(NULL);

    while (g_running) {
        fd_set readfds;
        FD_ZERO(&readfds);
        FD_SET(listen_fd, &readfds);
        int max_fd = listen_fd;

        for (int i = 0; i < MAX_CLIENTS; i++) {
            if (clients[i].fd != -1) {
                FD_SET(clients[i].fd, &readfds);
                if (clients[i].fd > max_fd) {
                    max_fd = clients[i].fd;
                }
            }
        }

        struct timeval tv;
        tv.tv_sec = 1;
        tv.tv_usec = 0;

        int activity = select(max_fd + 1, &readfds, NULL, NULL, &tv);

        if (activity < 0) {
            if (errno == EINTR) {
                break;
            }
            perror("select");
            break;
        }

        /* Check new connection */
        if (FD_ISSET(listen_fd, &readfds)) {
            struct sockaddr_in client_addr;
            socklen_t client_len = sizeof(client_addr);
            int new_fd = accept(listen_fd, (struct sockaddr *)&client_addr, &client_len);
            if (new_fd < 0) {
                if (errno != EINTR) {
                    perror("accept");
                }
            } else {
                int slot = -1;
                for (int i = 0; i < MAX_CLIENTS; i++) {
                    if (clients[i].fd == -1) {
                        slot = i;
                        break;
                    }
                }

                if (slot != -1) {
                    clients[slot].fd = new_fd;
                    clients[slot].mode = 0;
                    clients[slot].last_activity = time(NULL);

                    char ip_str[INET_ADDRSTRLEN];
                    inet_ntop(AF_INET, &client_addr.sin_addr, ip_str, sizeof(ip_str));
                    int port = ntohs(client_addr.sin_port);

                    printf("[Server] Client %d connected from %s:%d\n", slot + 1, ip_str, port);
                    fflush(stdout);
                } else {
                    const char *full_msg = "ERROR: Server full\n";
                    send(new_fd, full_msg, strlen(full_msg), 0);
                    close(new_fd);
                }
            }
        }

        /* Check clients activity */
        for (int i = 0; i < MAX_CLIENTS; i++) {
            if (clients[i].fd != -1 && FD_ISSET(clients[i].fd, &readfds)) {
                char buf[BUFFER_SIZE];
                memset(buf, 0, sizeof(buf));
                ssize_t n = recv(clients[i].fd, buf, sizeof(buf) - 1, 0);
                if (n <= 0) {
                    /* Disconnect or error */
                    printf("[Server] Client %d disconnected.\n", i + 1);
                    fflush(stdout);
                    close(clients[i].fd);
                    clients[i].fd = -1;
                } else {
                    buf[n] = '\0';
                    char *newline = strpbrk(buf, "\r\n");
                    if (newline) {
                        *newline = '\0';
                    }

                    clients[i].last_activity = time(NULL);
                    char resp[BUFFER_SIZE];

                    if (strcmp(buf, "GET_TEMP") == 0) {
                        double temp = 25.0 + (rand() % 100) / 10.0;
                        snprintf(resp, sizeof(resp), "%.1f\n", temp);
                        printf("[Server] Client %d: GET_TEMP → %.1f\n", i + 1, temp);
                        fflush(stdout);
                        send(clients[i].fd, resp, strlen(resp), 0);
                    } else if (strcmp(buf, "GET_HUMIDITY") == 0) {
                        double humidity = 30.0 + (rand() % 300) / 10.0;
                        snprintf(resp, sizeof(resp), "%.1f\n", humidity);
                        printf("[Server] Client %d: GET_HUMIDITY → %.1f\n", i + 1, humidity);
                        fflush(stdout);
                        send(clients[i].fd, resp, strlen(resp), 0);
                    } else if (strncmp(buf, "SET_MODE ", 9) == 0) {
                        int m = atoi(buf + 9);
                        clients[i].mode = m;
                        snprintf(resp, sizeof(resp), "OK\n");
                        printf("[Server] Client %d: SET_MODE %d → OK\n", i + 1, m);
                        fflush(stdout);
                        send(clients[i].fd, resp, strlen(resp), 0);
                    } else if (strcasecmp(buf, "QUIT") == 0) {
                        printf("[Server] Client %d requested QUIT.\n", i + 1);
                        fflush(stdout);
                        close(clients[i].fd);
                        clients[i].fd = -1;
                    } else {
                        snprintf(resp, sizeof(resp), "ERROR: Unknown command\n");
                        send(clients[i].fd, resp, strlen(resp), 0);
                    }
                }
            }
        }

        /* Check broadcast */
        time_t now = time(NULL);
        if (now - last_broadcast >= BROADCAST_INTERVAL) {
            last_broadcast = now;

            int active_count = 0;
            int last_mode = 0;
            for (int i = 0; i < MAX_CLIENTS; i++) {
                if (clients[i].fd != -1) {
                    active_count++;
                    last_mode = clients[i].mode;
                }
            }

            if (active_count > 0) {
                double temp = 25.0 + (rand() % 100) / 10.0;
                double humidity = 30.0 + (rand() % 300) / 10.0;
                char broadcast_msg[BUFFER_SIZE];
                snprintf(broadcast_msg, sizeof(broadcast_msg),
                         "[BROADCAST] Temp=%.1f Humidity=%.1f Mode=%d Clients=%d\n",
                         temp, humidity, last_mode, active_count);

                printf("[Server] Broadcasting to %d clients: Temp=%.1f Humidity=%.1f Mode=%d Clients=%d\n",
                       active_count, temp, humidity, last_mode, active_count);
                fflush(stdout);

                for (int i = 0; i < MAX_CLIENTS; i++) {
                    if (clients[i].fd != -1) {
                        send(clients[i].fd, broadcast_msg, strlen(broadcast_msg), 0);
                    }
                }
            }
        }
    }

    /* Cleanup */
    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (clients[i].fd != -1) {
            close(clients[i].fd);
            clients[i].fd = -1;
        }
    }
    close(listen_fd);

    printf("\n[Server] Shutdown complete.\n");
    return EXIT_SUCCESS;
}
