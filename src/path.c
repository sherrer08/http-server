#include "path.h"

static int hex_value(char c) {
    if (c >= '0' && c <= '9') {
        return c - '0';
    } else if (c >= 'a' && c <= 'f') {
        return c -'a' + 10;
    } else if (c >= 'A' && c <= 'F') {
        return c - 'A' + 10;
    } else {
        return -1;
    }
}

static enum path_result fail(char *out, enum path_result r) {
    out[0] = '\0';
    return r;
}

enum path_result path_decode(const char *target, char *out, size_t out_size) {
    size_t i = 0;
    size_t j = 0;
    int byte;

    while (target[i] != '\0' && target[i] != '?') {
        if (j + 1 >= out_size) {
            return fail(out, PATH_TOO_LONG);
        }

        if (target[i] == '%') {
            int hi = hex_value(target[i + 1]);

            if (hi < 0) {
                return fail(out, PATH_BAD);
            }

            int lo = hex_value(target[i + 2]);

            if (lo < 0) {
                return fail(out, PATH_BAD);
            }

            byte = hi * 16 + lo;

            if (byte < 32 || byte == 127) {
                return fail(out, PATH_BAD);
            }

            out[j] = (char)byte;
            j += 1;
            i += 3;
        } else {
            out[j] = target[i];
            j += 1;
            i += 1;
        }
    }

    out[j] = '\0';
    return PATH_OK;
}

int path_is_safe(const char *path) {
    if (path[0] != '/') {
        return 0;
    }

    const char *p = path;

    while (*p != '\0') {
        while (*p == '/') {
            p++;
        }

        if (*p == '\0') {
            break;
        }

        if (*p == '.') {
            return 0;
        }

        while (*p != '/' && *p != '\0') {
            p++;
        }
    }

    return 1;
}

int path_within_root(const char *root, const char *resolved) {
    (void)root;
    (void)resolved;
    return 0;
}