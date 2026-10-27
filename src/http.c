#include "http.h"

enum http_parse_result http_parse_request(char *buf, size_t len, struct http_request *req) {
    (void)buf;
    (void)len;
    (void)req;
    return HTTP_PARSE_BAD_REQUEST;
}

const char *http_get_header(const struct http_request *req, const char *name) {
    (void)req;
    (void)name;
    return NULL;
}