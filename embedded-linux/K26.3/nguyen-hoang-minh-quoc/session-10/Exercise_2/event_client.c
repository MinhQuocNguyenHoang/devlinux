/**
 * @file event_client.c
 * @brief Test client for event_monitor via Unix domain socket
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/un.h>
#include "event_monitor.h"

int main(void)
{
    char line[BUFFER_SIZE];
    char recv_buf[BUFFER_SIZE];

    while (fgets(line, sizeof(line), stdin)) {
        char cmd[BUFFER_SIZE];
        sscanf(line, "%s", cmd);

        int sock_fd = socket(AF_UNIX, SOCK_STREAM, 0);
        if (sock_fd < 0) {
            perror("socket");
            return EXIT_FAILURE;
        }

        struct sockaddr_un addr;
        memset(&addr, 0, sizeof(addr));
        addr.sun_family = AF_UNIX;
        strncpy(addr.sun_path, SOCKET_PATH, sizeof(addr.sun_path) - 1);

        if (connect(sock_fd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
            perror("connect");
            close(sock_fd);
            return EXIT_FAILURE;
        }

        if (send(sock_fd, line, strlen(line), 0) < 0) {
            perror("send");
            close(sock_fd);
            break;
        }

        memset(recv_buf, 0, sizeof(recv_buf));
        ssize_t n = recv(sock_fd, recv_buf, sizeof(recv_buf) - 1, 0);
        if (n > 0) {
            recv_buf[n] = '\0';
            printf("%s", recv_buf);
            fflush(stdout);
        }

        close(sock_fd);

        if (strcasecmp(cmd, "EXIT") == 0) {
            break;
        }
    }

    return EXIT_SUCCESS;
}
