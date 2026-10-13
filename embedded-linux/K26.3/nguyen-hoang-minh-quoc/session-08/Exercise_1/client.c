/**
 * @file client.c
 * @brief Monitor CLI Tool via Unix Domain Socket (Stream)
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/un.h>

#define SOCKET_PATH "/tmp/monitor.sock"
#define BUFFER_SIZE 256

int main(void)
{
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

    printf("[monitor-cli] Connected to %s\n", SOCKET_PATH);
    fflush(stdout);

    char input[BUFFER_SIZE];
    char recv_buf[BUFFER_SIZE];

    while (1) {
        printf("> ");
        fflush(stdout);

        if (!fgets(input, sizeof(input), stdin)) {
            break;
        }

        /* Check for quit */
        char cmd[BUFFER_SIZE];
        sscanf(input, "%s", cmd);
        if (strcmp(cmd, "quit") == 0) {
            send(sock_fd, "quit\n", 5, 0);
            break;
        }

        /* Send command to daemon */
        ssize_t bytes_sent = send(sock_fd, input, strlen(input), 0);
        if (bytes_sent < 0) {
            perror("send");
            break;
        }

        /* Receive response */
        memset(recv_buf, 0, sizeof(recv_buf));
        ssize_t bytes_recv = recv(sock_fd, recv_buf, sizeof(recv_buf) - 1, 0);
        if (bytes_recv < 0) {
            perror("recv");
            break;
        }
        if (bytes_recv == 0) {
            printf("[monitor-cli] Server closed connection.\n");
            break;
        }

        recv_buf[bytes_recv] = '\0';
        printf("%s", recv_buf);
        fflush(stdout);
    }

    close(sock_fd);
    return EXIT_SUCCESS;
}
