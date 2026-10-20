#ifndef SERVER_H
#define SERVER_H

/*
 * Creates a TCP socket listening on all network interfaces at the given
 * port (1-65535).
 * Returns the listening socket's file decriptor on success. On failure,
 * prints the reason to stderr, closes anything it opened, and returns -1.
 * The caller owns the returned descriptor and must close it when done.
*/
int server_listen(int port);

/*
 * Accepts and serves clients on listen_fd, one at a time: for each
 * connections, reads the request, sends a fixed HTTP response, and closes
 * the connection. listen_fd must come from server_listen.
 * Errors on a single connection are printed to stderr and the server
 * moves on to the next client.
 * Does not return. Does not close listen_fd.
*/
void server_run(int listen_fd);

#endif
