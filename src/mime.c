#include "mime.h"

const char *mime_type_for_path(const char *path) {
    (void)path;
    return "application/octet-stream";
}