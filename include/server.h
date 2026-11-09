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
 * Accepts and serves clients on listen_fd, one at a time. For each
 * connection: reads the request's header section, parses and validates it,
 * and, for GET and HEAD, resolves the target to a regular file inside
 * docroot and sends it (or sends the matching error status), then logs one
 * line and closes the connection. docroot must be an absolute, resolved
 * path to a directory, valid for as long as the server runs.
 * Errors on a single connection are answered or logged, and the server
 * moves on to the next client.
 * Does not return. Does not close listen_fd.
 */
void server_run(int listen_fd, const char *docroot);

#endif
