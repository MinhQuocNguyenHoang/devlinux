/**
 * @file receiver.c
 * @brief Collector server for sensor telemetry via UDP Internet Socket
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

#define COLLECTOR_PORT 9000
#define BUFFER_SIZE    256

static volatile sig_atomic_t g_running = 1;

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

    int sock_fd = socket(AF_INET, SOCK_DGRAM, 0);
    if (sock_fd < 0) {
        perror("socket");
        return EXIT_FAILURE;
    }

    int opt = 1;
    if (setsockopt(sock_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
        perror("setsockopt");
        close(sock_fd);
        return EXIT_FAILURE;
    }

    struct sockaddr_in serv_addr;
    memset(&serv_addr, 0, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    serv_addr.sin_port = htons(COLLECTOR_PORT);

    if (bind(sock_fd, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
        perror("bind");
        close(sock_fd);
        return EXIT_FAILURE;
    }

    printf("[Collector] Listening on 0.0.0.0:%d...\n", COLLECTOR_PORT);
    fflush(stdout);

    char buf[BUFFER_SIZE];
    struct sockaddr_in client_addr;
    socklen_t client_len = sizeof(client_addr);

    while (g_running) {
        memset(buf, 0, sizeof(buf));
        ssize_t n = recvfrom(sock_fd, buf, sizeof(buf) - 1, 0,
                             (struct sockaddr *)&client_addr, &client_len);
        if (n < 0) {
            if (errno == EINTR) {
                break;
            }
            perror("recvfrom");
            continue;
        }

        buf[n] = '\0';

        time_t now = time(NULL);
        struct tm *t = localtime(&now);
        char ts[16] = {0};
        strftime(ts, sizeof(ts), "%H:%M:%S", t);

        char ip_str[INET_ADDRSTRLEN] = {0};
        inet_ntop(AF_INET, &client_addr.sin_addr, ip_str, sizeof(ip_str));
        int port = ntohs(client_addr.sin_port);

        printf("[%s] %s:%d → %s\n", ts, ip_str, port, buf);
        fflush(stdout);
    }

    close(sock_fd);
    printf("[Collector] Stopped.\n");
    return EXIT_SUCCESS;
}
