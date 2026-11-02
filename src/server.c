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
#define HEADER_BUFFER_SIZE 512
#define RESPONSE_BODY_SIZE 2048

enum read_result {
    READ_OK,
    READ_CLOSED,
    READ_ERROR,
    READ_TOO_LARGE,
    READ_BAD
};

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

/*
 * Sends a complete HTTP response on fd: status line, headers, and
 * (if send_body is 1) the body.
 * extra_headers is inserted before the blank line; each header must end in
 * "\r\n". Pass "" for none, never NULL, and never build it from request
 * data. Content-Length is always body_len, even when send_body is 0.
 * Returns 0 on success, or -1 if the response could not be built or sent.
*/
static int send_response(int fd, int status, const char *extra_headers, const char *body, size_t body_len, int send_body) {
    char header_buf[HEADER_BUFFER_SIZE];

    int header_len = snprintf(header_buf, sizeof(header_buf),
                              "HTTP/1.1 %d %s\r\n"
                              "Content-Type: text/plain\r\n"
                              "Content-Length: %zu\r\n"
                              "X-Content-Type-Options: nosniff\r\n"
                              "Connection: close\r\n"
                              "%s"
                              "\r\n",
                              status, http_reason_phrase(status),
                              body_len, extra_headers);

    if (header_len < 0 || (size_t)header_len >= sizeof(header_buf)) {
        fprintf(stderr, "response headers too long\n");
        return -1;
    }

    if (send_all(fd, header_buf, (size_t)header_len) == -1) {
        return -1;
    }

    if (send_body && send_all(fd, body, body_len) == -1) {
        return -1;
    }

    return 0;
}

/*
 * Sends an error response with a short plain-text body naming the status,
 * such as "405 Method Not Allowed\n". extra_headers follows the same rules
 * as in send_response. Returns 0 on success, or -1 on failure.
*/
static int send_error(int fd, int status, const char *extra_headers) {
    char body[128];

    int body_len = snprintf(body, sizeof(body), "%d %s\n", status, http_reason_phrase(status));

    if (body_len < 0 || (size_t)body_len >= sizeof(body)) {
        fprintf(stderr, "error body too long\n");
        return -1;
    }

    return send_response(fd, status, extra_headers, body, (size_t)body_len, 1);
}

static int decide_status(const struct http_request *req, const char **extra_headers) {
    *extra_headers = "";
    switch (req->method) {
        case HTTP_METHOD_GET:
        case HTTP_METHOD_HEAD:
            break;
        case HTTP_METHOD_POST:
        case HTTP_METHOD_PUT:
        case HTTP_METHOD_DELETE:
        case HTTP_METHOD_CONNECT:
        case HTTP_METHOD_OPTIONS:
        case HTTP_METHOD_TRACE:
        case HTTP_METHOD_PATCH:
            *extra_headers = "Allow: GET, HEAD\r\n";
            return 405;
        case HTTP_METHOD_UNKNOWN:
            return 501;
    }

    if (req->has_transfer_encoding) {
        return 501;
    }

    if (req->has_content_length && req->content_length > 0) {
        return 413;
    }

    return 200;
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

            if (result != HTTP_PARSE_OK) {
                send_error(client_fd, http_status_for_parse_result(result), "");
                printf("parse error %d -> %d\n", (int)result, http_status_for_parse_result(result));
                fflush(stdout);
                break;
            }

            const char *extra_headers = "";
            int status = decide_status(&req, &extra_headers);

            if (status != 200) {
                send_error(client_fd, status, extra_headers);
                printf("%s %s -> %d\n", req.method_str, req.target, status);
                fflush(stdout);
                break;
            }

            char body[RESPONSE_BODY_SIZE];
            int body_len = snprintf(body, sizeof(body), "You requested GET %s\n", req.target);

            if (body_len < 0 || (size_t)body_len >= sizeof(body)) {
                send_error(client_fd, 500, "");
                printf("%s %s -> %d\n", req.method_str, req.target, 500);
                fflush(stdout);
                break;
            }

            int send_body = (req.method == HTTP_METHOD_GET);
            send_response(client_fd, 200, "", body, (size_t)body_len, send_body);

            printf("%s %s -> 200\n", req.method_str, req.target);
            fflush(stdout);

            break;
        }
        case READ_TOO_LARGE:
            send_error(client_fd, 431, "");
            printf("request too large\n");
            fflush(stdout);
            break;
        case READ_BAD:
            send_error(client_fd, 400, "");
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