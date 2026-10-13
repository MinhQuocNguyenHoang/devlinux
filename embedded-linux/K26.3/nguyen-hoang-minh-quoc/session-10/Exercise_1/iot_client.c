/**
 * @file iot_client.c
 * @brief Simple client for IoT server
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include "iot_server.h"

int main(void)
{
    int sock_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (sock_fd < 0) {
        perror("socket");
        return EXIT_FAILURE;
    }

    struct sockaddr_in serv_addr;
    memset(&serv_addr, 0, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(SERVER_PORT);
    if (inet_pton(AF_INET, "127.0.0.1", &serv_addr.sin_addr) <= 0) {
        perror("inet_pton");
        close(sock_fd);
        return EXIT_FAILURE;
    }

    if (connect(sock_fd, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
        perror("connect");
        close(sock_fd);
        return EXIT_FAILURE;
    }

    char line[BUFFER_SIZE];
    char recv_buf[BUFFER_SIZE];

    while (fgets(line, sizeof(line), stdin)) {
        char cmd[BUFFER_SIZE];
        sscanf(line, "%s", cmd);

        if (strcasecmp(cmd, "quit") == 0) {
            send(sock_fd, line, strlen(line), 0);
            break;
        }

        if (send(sock_fd, line, strlen(line), 0) < 0) {
            perror("send");
            break;
        }

        memset(recv_buf, 0, sizeof(recv_buf));
        ssize_t n = recv(sock_fd, recv_buf, sizeof(recv_buf) - 1, 0);
        if (n <= 0) {
            break;
        }
        recv_buf[n] = '\0';
        printf("%s", recv_buf);
        fflush(stdout);
    }

    close(sock_fd);
    return EXIT_SUCCESS;
}
