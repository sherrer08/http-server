#include "server.h"
#include <stdio.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <string.h>
#include <arpa/inet.h>

#define LISTEN_BACKLOG 16
#define REQUEST_BUF_SIZE 4096
#define HEADER_BUFFER_SIZE 256

static const char BODY[] = "Hello, world!\n";

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

/*
 * Sends all len bytes of data on fd, calling send() as many times as needed.
 * Returns 0 on success, or -1 on error (errno is set).
 * Uses MSG_NOSIGNAL so a closed connection returns an error instead of
 * killing the process with SIGPIPE.
 */
static int send_all(int fd, const char *data, size_t len) {
    size_t sent = 0;

    while (sent < len) {
        ssize_t n = send(fd, data + sent, len - sent, MSG_NOSIGNAL);

        if (n == -1) {
            return -1;
        }

        sent += (size_t)n;
    }

    return 0;
}

static void handle_client(int client_fd) {
    /* M1 simplification: assumes the whole request arrives in one recv.
       M2 will read until the end of the headers. */
    char buf[REQUEST_BUF_SIZE];
    ssize_t n = recv(client_fd, buf, sizeof(buf) - 1, 0);

    if (n > 0) {
        buf[n] = '\0';
        
        printf("received %zd bytes:\n%s", n, buf);
        fflush(stdout);

        char header_buf[HEADER_BUFFER_SIZE];

        int header_len = snprintf(header_buf, sizeof(header_buf),
                                  "HTTP/1.1 200 OK\r\n"
                                  "Content-Type: text/plain\r\n"
                                  "Content-Length: %zu\r\n"
                                  "Connection: close\r\n"
                                  "\r\n",
                                  strlen(BODY));
        
        if (header_len < 0 || (size_t)header_len >= sizeof(header_buf)) {
            fprintf(stderr, "response headers too long\n");
        } else if (send_all(client_fd, header_buf, (size_t)header_len) == -1 ||
                   send_all(client_fd, BODY, strlen(BODY)) == -1) {
            perror("send");
        }
    } else if (n == 0) {
        printf("client closed without sending\n");
    } else {
        perror("recv");
    }

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