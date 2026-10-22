#include "server.h"
#include <stdio.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <string.h>
#include <arpa/inet.h>

#define LISTEN_BACKLOG 16

int server_listen(int port) {
    int fd = socket(AF_INET, SOCK_STREAM, 0);

    if (fd == -1) {
        perror("socket");

        return -1;
    }

    int opt = 1;

    if (setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) == -1) {
        perror("setsockopt");

        close(fd);

        return -1;
    }

    struct sockaddr_in addr;

    memset(&addr, 0, sizeof(addr));
    
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    addr.sin_addr.s_addr = htonl(INADDR_ANY);

    if (bind(fd, (struct sockaddr *)&addr, sizeof(addr)) == -1) {
        perror("bind");

        close(fd);

        return -1;
    }

    if (listen(fd, LISTEN_BACKLOG) == -1) {
        perror("listen");

        close(fd);

        return -1;
    }

    return fd;
}

static void handle_client(int client_fd) {
    close(client_fd);
}

void server_run(int listen_fd) {
    struct sockaddr_in client_addr;

    for (;;) {
        socklen_t addrlen = sizeof(client_addr);
        int client_fd = accept(listen_fd, (struct sockaddr *)&client_addr, &addrlen);

        if (client_fd == -1) {
            perror("accept");
            continue;
        }

        char ip[INET_ADDRSTRLEN];

        inet_ntop(AF_INET, &client_addr.sin_addr, ip, sizeof(ip));

        printf("connection from %s:%d\n", ip, ntohs(client_addr.sin_port));

        handle_client(client_fd);
    }
}