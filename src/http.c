#include "http.h"
#include <string.h>

enum http_parse_result http_parse_request(char *buf, size_t len, struct http_request *req) {
    memset(req, 0, sizeof(*req));
    
    if (len < 4 || memcmp(buf + len - 4, "\r\n\r\n", 4) != 0) {
        return HTTP_PARSE_BAD_REQUEST;
    } else if (memchr(buf, '\0', len) != NULL) {
        return HTTP_PARSE_BAD_REQUEST;
    }

    return HTTP_PARSE_BAD_REQUEST;
}

const char *http_get_header(const struct http_request *req, const char *name) {
    (void)req;
    (void)name;
    return NULL;
}