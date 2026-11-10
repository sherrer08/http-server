#include "server.h"
#include "http.h"
#include "path.h"
#include "mime.h"
#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <string.h>
#include <arpa/inet.h>
#include <limits.h>
#include <errno.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/sendfile.h>

#define LISTEN_BACKLOG 16
#define REQUEST_BUF_SIZE 4096
#define HEADER_BUFFER_SIZE 512

enum read_result {
    READ_OK,
    READ_CLOSED,
    READ_ERROR,
    READ_TOO_LARGE,
    READ_BAD
};

struct connection {
    int fd;                      /* the client socket */
    const char *peer;            /* "ip:port", for logging */
    char buf[REQUEST_BUF_SIZE];  /* bytes received, not yet handled */
    size_t len;                  /* how many bytes of buf are in use */
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

/*
 * Sends size bytes of the file in_fd, starting from its beginning, to the
 * socket out_fd, calling sendfile() as many times as needed.
 * Returns 0 if every byte was sent, or -1 on error (errno is set), including
 * when the file turns out shorter than size.
*/
static int sendfile_all(int out_fd, int in_fd, off_t size) {
    off_t offset = 0;

    while (offset < size) {
        off_t remaining  = size - offset;
        ssize_t n = sendfile(out_fd, in_fd, &offset, (size_t)remaining);

        if (n == -1) {
            return -1;
        }

        if (n == 0) {
            return -1;
        }
    }

    return 0;
}

/*
 * Reads from conn's socket until conn->buf holds a complete HTTP header
 * section, ending in "\r\n\r\n". Bytes already in the buffer (left over
 * from an earlier request) are checked first, so recv is only called when
 * they don't already hold a complete header section. The buffer is kept
 * '\0'-terminated, with one byte always free for the terminator.
 * On READ_OK, *header_len is the length of the header section, including
 * the "\r\n\r\n"; any bytes after it are the start of the next request.
 * Returns READ_TOO_LARGE if the buffer fills up without a complete header
 * section, READ_BAD if a '\0' byte arrives, READ_CLOSED if the client
 * closes first (conn->len is 0 if it closed between requests), and
 * READ_ERROR if recv fails (the reason is printed).
 */
static enum read_result read_request(struct connection *conn, size_t *header_len) {
    for (;;) {
        char *end = strstr(conn->buf, "\r\n\r\n");
        if (end != NULL) {
            *header_len = (size_t)(end - conn->buf) + 4;
            return READ_OK;
        }

        if (conn->len == REQUEST_BUF_SIZE - 1) {
            return READ_TOO_LARGE;
        }

        ssize_t n = recv(conn->fd, conn->buf + conn->len, REQUEST_BUF_SIZE - 1 - conn->len, 0);

        if (n == 0) {
            return READ_CLOSED;
        } else if (n == -1) {
            perror("recv");
            return READ_ERROR;
        } else {
            if (memchr(conn->buf + conn->len, '\0', (size_t)n) != NULL) {
                return READ_BAD;
            }
        }

        conn->len += (size_t)n;
        conn->buf[conn->len] = '\0';
    }
}

/*
 * Removes a handled request's header_len bytes from the front of
 * conn->buf, moving any bytes after it (the start of the next request) to
 * the front. Keeps the buffer '\0'-terminated.
*/
static void consume_request(struct connection *conn, size_t header_len) {
    size_t leftover = conn->len - header_len;

    memmove(conn->buf, conn->buf + header_len, leftover);
    conn->len = leftover;
    conn->buf[conn->len] = '\0';
}

/*
 * Sends the status line and headers of an HTTP response on fd, ending with
 * the blank line. Content-Length is content_length; the caller must then
 * send exactly that many body bytes (or none, for HEAD).
 * content_type must be a constant from the server, never built from
 * request data. extra_headers is inserted before the blank line; each
 * header must end in "\r\n". Pass "" for none, never NULL, and never build
 * it from request data.
 * Returns 0 on success, or -1 if the headers could not be built or sent.
*/
static int send_headers(int fd, int status, const char *content_type, const char *extra_headers, long long content_length) {
    char header_buf[HEADER_BUFFER_SIZE];

    int header_len = snprintf(header_buf, sizeof(header_buf),
                              "HTTP/1.1 %d %s\r\n"
                              "Content-Type: %s\r\n"
                              "Content-Length: %lld\r\n"
                              "X-Content-Type-Options: nosniff\r\n"
                              "Connection: close\r\n"
                              "%s"
                              "\r\n",
                              status, http_reason_phrase(status), content_type,
                              content_length, extra_headers);

    if (header_len < 0 || (size_t)header_len >= sizeof(header_buf)) {
        fprintf(stderr, "response headers too long\n");
        return -1;
    }

    return send_all(fd, header_buf, (size_t)header_len);
}

/*
 * Sends a complete HTTP response on fd whose body is in memory: the
 * headers (see send_headers), then body if send_body is 1. Content-Length
 * is always body_len, even when send_body is 0.
 * Returns 0 on success, or -1 on failure.
*/
static int send_response(int fd, int status, const char *content_type, const char *extra_headers, const char *body, size_t body_len, int send_body) {
    if (send_headers(fd, status, content_type, extra_headers, (long long)body_len) == -1) {
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

    return send_response(fd, status, "text/plain; charset=utf-8", extra_headers, body, (size_t)body_len, 1);
}

/*
 * Decides the status for a successfully parsed request, based on what this
 * server supports: 405 for known methods other than GET and HEAD, 501 for
 * unknown methods and for Transfer-Encoding, 413 for a non-zero
 * Content-Length, and 200 otherwise.
 * Always sets *extra_headers: to "Allow: GET, HEAD\r\n" for a 405, and to
 * "" otherwise.
 */
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

/*
 * Prints one log line for a connection: the client's address, the request
 * line in quotes (or "-" if req is NULL because the request didn't parse),
 * and the status sent (or "-" if status is 0 because nothing was sent).
 * req's string fields must still be valid, so the request buffer must
 * still exist. Flushes stdout.
 */
static void log_request(const char *peer, const struct http_request *req, int status, int incomplete) {
    printf("%s", peer);
    
    if (req != NULL) {
        printf(" \"%s %s HTTP/1.%d\"", req->method_str, req->target, req->version_minor);
    } else {
        printf(" -");
    }

    if (status != 0) {
        printf(" %d", status);
    } else {
        printf(" -");
    }

    if (incomplete != 0){
        printf(" (incomplete)");
    }

    printf("\n");
    fflush(stdout);
}

/*
 * Maps an errno value from realpath, stat, or open to the status to send:
 * 404 for a missing file or path component, 403 for a permission problem,
 * 414 for a path that's too long, and 500 for anything unexpected.
*/
static int status_for_errno(int err) {
    switch (err) {
        case ENOENT:
        case ENOTDIR:
        case ELOOP:
            return 404;
        case EACCES:
            return 403;
        case ENAMETOOLONG:
            return 414;
        default:
            return 500;
    }
}

/*
 * Turns a request target into the real path of a file inside docroot.
 * Decodes the target, rejects unsafe paths, resolves symbolic links and
 * "//" with realpath, and checks the result is inside docroot. A directory
 * resolves to its index.html.
 * resolved must have room for PATH_MAX bytes. Returns 200 with the path in
 * resolved, or the error status to send (resolved is then unspecified).
*/
static int resolve_target(const char *docroot, const char *target, char *resolved) {
    char decoded[PATH_MAX];

    switch (path_decode(target, decoded, sizeof(decoded))) {
        case PATH_OK:
            break;
        case PATH_BAD:
            return 400;
        case PATH_TOO_LONG:
            return 414;
    }

    if (!path_is_safe(decoded)) {
        return 404;
    }

    char full[PATH_MAX];
    int n = snprintf(full, sizeof(full), "%s%s", docroot, decoded);
    if (n < 0 || (size_t)n >= sizeof(full)) {
        return 414;
    }

    if (realpath(full, resolved) == NULL) {
        return status_for_errno(errno);
    }

    if (!path_within_root(docroot, resolved)) {
        return 404;
    }

    struct stat st;
    if (stat(resolved, &st) == -1) {
        return status_for_errno(errno);
    }

    if (S_ISDIR(st.st_mode)) {
        char index[PATH_MAX];
        n = snprintf(index, sizeof(index), "%s/index.html", resolved);
        if (n < 0 || (size_t)n >= sizeof(index)) {
            return 414;
        }

        if (realpath(index, resolved) == NULL) {
            if (errno == ENOENT) {
                return 403;
            }
            return status_for_errno(errno);
        }

        if (!path_within_root(docroot, resolved)) {
            return 404;
        }
    }

    return 200;
}

/*
 * Opens the file at path (from resolve_target) for sending, and checks it
 * through the open descriptor: it must be a regular file. Does not follow a
 * symbolic link in the last part of the path, and does not block on named
 * pipes. Returns 200, with the open descriptor in *fd_out and the file's
 * size in *size_out, or the error status to send (nothing is left open).
 * On 200, the caller must close *fd_out.
*/
static int open_file(const char *path, int *fd_out, off_t *size_out) {
    int fd = open(path, O_RDONLY | O_NOFOLLOW | O_NONBLOCK);

    if (fd == -1) {
        return status_for_errno(errno);
    }

    struct stat st;
    if (fstat(fd, &st) == -1) {
        close(fd);
        return 500;
    }

    if (!S_ISREG(st.st_mode)) {
        close(fd);
        return 403;
    }

    *fd_out = fd;
    *size_out = st.st_size;
    return 200;
}

/*
 * Parses the request in conn->buf (header_len bytes, '\0'-terminated by
 * the caller) and sends the response: an error, or the requested file.
 * Fills in *req and sets *parsed to 1 if the request parsed, and sets
 * *incomplete to 1 if a file's body could not be fully sent.
 * Returns the status sent.
*/
static int respond(struct connection *conn, size_t header_len, const char *docroot, struct http_request *req, int *parsed, int *incomplete) {
    enum http_parse_result result = http_parse_request(conn->buf, header_len, req);

    if (result != HTTP_PARSE_OK) {
        int status = http_status_for_parse_result(result);
        send_error(conn->fd, status, "");
        return status;
    }

    *parsed = 1;

    const char *extra_headers = "";
    int status = decide_status(req, &extra_headers);

    if (status != 200) {
        send_error(conn->fd, status, extra_headers);
        return status;
    }

    char resolved[PATH_MAX];
    status = resolve_target(docroot, req->target, resolved);

    if (status != 200) {
        send_error(conn->fd, status, "");
        return status;
    }

    int fd;
    off_t size;
    status = open_file(resolved, &fd, &size);

    if (status != 200) {
        send_error(conn->fd, status, "");
        return status;
    }

    const char *type = mime_type_for_path(resolved);

    if (send_headers(conn->fd, status, type, "", (long long)size) == 0 && req->method == HTTP_METHOD_GET) {
        if (sendfile_all(conn->fd, fd, size) == -1) {
            *incomplete = 1;
        }
    }

    close(fd);
    return status;
}

/*
* Handles the request whose header section is the first header_len bytes
* of conn->buf: responds to it, and logs one line. Temporarily writes a
* '\0' after the header section for the parser, and restores the
* original byte (the start of any pipelined request) before returning.
*/
static void handle_request(struct connection *conn, size_t header_len, const char *docroot) {
    struct http_request req;
    int parsed = 0;
    int incomplete = 0;

    char saved = conn->buf[header_len];
    conn->buf[header_len] = '\0';

    int status = respond(conn, header_len, docroot, &req, &parsed, &incomplete);

    log_request(conn->peer, parsed ? &req : NULL, status, incomplete);

    conn->buf[header_len] = saved;
}

/*
 * Handles one connection: reads requests from client_fd and responds to
 * each, logging one line per request. Bytes after a request's header
 * section are kept for the next request. peer is the client's address,
 * as "ip:port", used only for logging. Closes client_fd before returning.
 */
static void handle_connection(int client_fd, const char *peer, const char *docroot) {
    struct connection conn;
    conn.fd = client_fd;
    conn.peer = peer;
    conn.len = 0;
    conn.buf[0] = '\0';

    for (;;) {
        size_t header_len;
        enum read_result rr = read_request(&conn, &header_len);

        if (rr != READ_OK) {
            int status = 0;

            switch (rr) {
                case READ_TOO_LARGE:
                    status = 431;
                    send_error(conn.fd, status, "");
                    break;
                case READ_BAD:
                    status = 400;
                    send_error(conn.fd, status, "");
                    break;
                case READ_CLOSED:
                case READ_ERROR:
                case READ_OK:
                    break;
            }

            log_request(conn.peer, NULL, status, 0);
            break;
        }

        handle_request(&conn, header_len, docroot);
        consume_request(&conn, header_len);

        break;
    }

    close(conn.fd);
}


void server_run(int listen_fd, const char *docroot) {
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

        char peer[INET_ADDRSTRLEN + 8];
        snprintf(peer, sizeof(peer), "%s:%d", ip, ntohs(client_addr.sin_port));

        handle_connection(client_fd, peer, docroot);
    }
}