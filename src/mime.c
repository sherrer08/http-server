#include "mime.h"
#include <string.h>
#include <strings.h>

static const char DEFAULT_TYPE[] = "application/octet-stream";

static const struct {
    const char *ext;
    const char *type;
} TYPES[] = {
    { "html", "text/html; charset=utf-8" },
    { "htm", "text/html; charset=utf-8" },
    { "css", "text/css; charset=utf-8" },
    { "js", "text/javascript; charset=utf-8" },
    { "json", "application/json" },
    { "txt", "text/plain; charset=utf-8" },
    { "png", "image/png" },
    { "jpg", "image/jpeg" },
    { "jpeg", "image/jpeg" },
    { "gif", "image/gif" },
    { "svg", "image/svg+xml" },
    { "ico", "image/x-icon" },
    { "webp", "image/webp" },
    { "pdf", "application/pdf" },
};

const char *mime_type_for_path(const char *path) {
    const char *slash = strrchr(path, '/');
    const char *name = (slash != NULL) ? slash + 1 : path;

    const char *dot = strrchr(name, '.');

    if (dot == NULL || dot == name || dot[1] == '\0') {
        return DEFAULT_TYPE;
    }

    const char *ext = dot + 1;
    const size_t count = sizeof(TYPES) / sizeof(TYPES[0]);

    for (size_t i = 0; i < count; i++) {
        if (strcasecmp(ext, TYPES[i].ext) == 0) {
            return TYPES[i].type;
        }
    }

    return DEFAULT_TYPE;
}