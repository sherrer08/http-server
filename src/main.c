#include <stdio.h>
#include <stdlib.h>
#include <limits.h>
#include <sys/stat.h>
#include "server.h"

static void usage(const char *prog) {
    fprintf(stderr, "usage: %s [port] [docroot]\n", prog);
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
    int port = 8080;
    const char *docroot_arg = "www";

    if (argc > 3) {
        usage(argv[0]);
        return EXIT_FAILURE;
    }
    
    if (argc >= 2 && parse_port(argv[1], &port) != 0) {
        usage(argv[0]);
        return EXIT_FAILURE;
    }

    if (argc == 3) {
        docroot_arg = argv[2];
    }

    char root[PATH_MAX];

    if (realpath(docroot_arg, root) == NULL) {
        perror(docroot_arg);
        return EXIT_FAILURE;
    }

    struct stat st;

    if (stat(root, &st) == -1) {
        perror(root);
        return EXIT_FAILURE;
    }

    if (!S_ISDIR(st.st_mode)) {
        fprintf(stderr, "%s: not a directory\n", root);
        return EXIT_FAILURE;
    }

    printf("serving %s on port %d\n", root, port);

    int listen_fd = server_listen(port);

    if (listen_fd < 0) {
        return EXIT_FAILURE;
    }

    server_run(listen_fd, root);

    /* listen_fd should be closed once server_run can return */
    return EXIT_SUCCESS;
}