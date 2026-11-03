#ifndef PATH_H
#define PATH_H

#include <stddef.h>

/* Result of decoding a request target into a file path. */
enum path_result {
    PATH_OK,
    PATH_BAD,       // malformed %-escape. or an escape that decodes to '\0'
    PATH_TOO_LONG   // the decoded path does not fit in the output buffer
};

/*
 * Decodes the path part of a request taget: everything before the first
 * '?', with each "%XX" escape replaced by the byte it encodes.
 * Decodes exactly once: the output is never decoded again, so "%252e"
 * becomes "%2e", not ".".
 * Writes the result to out, which has room for out_size bytes (at least 1).
 * On PATH_OK, out holds the decoded path, '\0'-terminated. Never writes
 * more than out_size bytes. On any other result, out holds an empty string.
*/
enum path_result path_decode(const char *target, char *out, size_t out_size);

/*
 * Returns 1 if path, a decoded request path, contains no ".." segment, and 
 * 0 otherwise. A segment is the text between two '/' characters, or before
 * the first or after the last one. Does not touch the file system.
*/
int path_is_safe(const char *path);

/*
 * Returns 1 if resolved is root itself or a path inside it (root followed
 * by '/'), and 0 otherwise. Both must be absolute paths as produced by
 * realpath, with no trailing '/', Compare the strings only; does not touch
 * the file system.
*/
int path_within_root(const char *root, const char *resolved);

#endif