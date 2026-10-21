#include <stdio.h>
#include <stdlib.h>
#include "server.h"

static void usage(const char *prog) {
    fprintf(stderr, "usage: %s [port]\n", prog);
}

static int parse_port(const char *text, int *port) {
    char *end;
    long val = strtol(text, &end, 10);

    if (end == text || *end != '\0' || val < 1 || val > 65535) {
        return -1;
    }

    *port = (int)val;

    return 0;
}

int main(int argc, char *argv[]) {
    int port;

    if (argc == 1) {
        port = 8080;
    } else if (argc == 2) {
        if (parse_port(argv[1], &port) != 0) {
            usage(argv[0]);
            return EXIT_FAILURE;
        }
    } else {
        usage(argv[0]);
        return EXIT_FAILURE;
    }

    int listen_fd = server_listen(port);

    if (listen_fd < 0) {
        return EXIT_FAILURE;
    }

    server_run(listen_fd);

    /* listen_fd should be closed once server_run can return */
    return EXIT_SUCCESS;
}