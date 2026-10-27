#include "http.h"
#include <string.h>

static enum http_parse_result parse_request_line(char *line, struct http_request *req) {
    (void)line;
    (void)req;
    return HTTP_PARSE_BAD_REQUEST;
}

enum http_parse_result http_parse_request(char *buf, size_t len, struct http_request *req) {
    memset(req, 0, sizeof(*req));
    
    if (len < 4 || memcmp(buf + len - 4, "\r\n\r\n", 4) != 0) {
        return HTTP_PARSE_BAD_REQUEST;
    } else if (memchr(buf, '\0', len) != NULL) {
        return HTTP_PARSE_BAD_REQUEST;
    }

    char *line_end = strstr(buf, "\r\n");

    if (line_end == NULL) {
        return HTTP_PARSE_BAD_REQUEST;
    }

    *line_end = '\0';

    char *headers = line_end + 2;
    (void)headers;

    enum http_parse_result result = parse_request_line(buf, req);
    if (result != HTTP_PARSE_OK) {
        return result;
    }

    return HTTP_PARSE_BAD_REQUEST;
}

const char *http_get_header(const struct http_request *req, const char *name) {
    (void)req;
    (void)name;
    return NULL;
}