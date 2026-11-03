#include "path.h"

enum path_result path_decode(const char *target, char *out, size_t out_size) {
    (void)target;
    if (out_size >= 1) {
        out[0] = '\0';
    }
    return PATH_BAD;
}

int path_is_safe(const char *path) {
    (void) path;
    return 0;
}

int path_within_root(const char *root, const char *resolved) {
    (void)root;
    (void)resolved;
    return 0;
}