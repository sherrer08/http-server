#include "server.h"
#include "http.h"
#include <stdio.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <string.h>
#include <arpa/inet.h>

#define LISTEN_BACKLOG 16
#define REQUEST_BUF_SIZE 4096
#define HEADER_BUFFER_SIZE 256

enum read_result {
    READ_OK,
    READ_CLOSED,
    READ_ERROR,
    READ_TOO_LARGE,
    READ_BAD
};

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

static enum read_result read_request(int fd, char *buf, size_t cap, size_t *header_len) {
    size_t received = 0;

    for (;;) {
        if (received == cap - 1) {
            return READ_TOO_LARGE;
        }

        ssize_t n = recv(fd, buf + received, cap - 1 - received, 0);

        if (n == 0) {
            return READ_CLOSED;
        } else if (n == -1) {
            perror("recv");
            return READ_ERROR;
        } else {
            if (memchr(buf + received, '\0', (size_t)n) != NULL) {
                return READ_BAD;
            }
        }

        received += (size_t)n;
        buf[received] = '\0';

        char *end = strstr(buf, "\r\n\r\n");
        if (end != NULL) {
            *header_len = (size_t)(end - buf) + 4;
            return READ_OK;
        }
    }
}

/* Sends a fixed 200 OK plain-text response */
static void send_fixed_response(int fd) {
    char header_buf[HEADER_BUFFER_SIZE];

    int header_len = snprintf(header_buf, sizeof(header_buf),
                              "HTTP/1.1 200 OK\r\n"
                              "Content-Type: text/plain\r\n"
                              "Content-Length: %zu\r\n"
                              "Connection: close \r\n"
                              "\r\n",
                              strlen(BODY));

    if (header_len < 0 || (size_t)header_len >= sizeof(header_buf)) {
        fprintf(stderr, "response headers too long\n");
        return;
    }

    if (send_all(fd, header_buf, (size_t)header_len) == -1 || send_all(fd, BODY, strlen(BODY)) == -1) {
        perror("send");
    }
}

static void handle_client(int client_fd) {
    char buf[REQUEST_BUF_SIZE];
    size_t header_len;

    enum read_result rr = read_request(client_fd, buf, sizeof(buf), &header_len);

    switch (rr) {
        case READ_OK: {
            struct http_request req;

            buf[header_len] = '\0';

            enum http_parse_result result = http_parse_request(buf, header_len, &req);

            if (result == HTTP_PARSE_OK) {
                printf("%s %s -> parsed OK\n", req.method_str, req.target);
            } else {
                printf("parse error %d\n", (int)result);
            }
            fflush(stdout);

            send_fixed_response(client_fd);
            break;
        }
        case READ_TOO_LARGE:
            printf("request too large\n");
            fflush(stdout);
            break;
        case READ_BAD:
            printf("bad request\n");
            fflush(stdout);
            break;
        case READ_CLOSED:
        case READ_ERROR:
            break;
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