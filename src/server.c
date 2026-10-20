#include "server.h"
#include <stdio.h>

int server_listen(int port) {
    printf("server_listen: would listen on port %d\n", port);

    return 3;
}

void server_run(int listen_fd) {
    printf("server_run: would serve clients on fd %d\n", listen_fd);
}