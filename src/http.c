#include "http.h"
#include <string.h>

static const char TOKEN_CHARS[] =
    "!#$%&'*+-.^_`|~"
    "0123456789"
    "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
    "abcdefghijklmnopqrstuvwxyz";

static enum http_method method_from_string(const char *s) {
    static const struct {
        const char *name;
        enum http_method method;
    } METHODS[] = {
        { "GET",     HTTP_METHOD_GET },
        { "HEAD",    HTTP_METHOD_HEAD },
        { "POST",    HTTP_METHOD_POST },
        { "PUT",     HTTP_METHOD_PUT },
        { "DELETE",  HTTP_METHOD_DELETE },
        { "CONNECT", HTTP_METHOD_CONNECT },
        { "OPTIONS", HTTP_METHOD_OPTIONS },
        { "TRACE",   HTTP_METHOD_TRACE },
        { "PATCH",   HTTP_METHOD_PATCH },
    };

    size_t rows = sizeof(METHODS) / sizeof(METHODS[0]);

    for (size_t i = 0; i < rows; i++) {
        if (strcmp(s, METHODS[i].name) == 0) {
            return METHODS[i].method;
        }
    }

    return HTTP_METHOD_UNKNOWN;
}

static enum http_parse_result check_target(const char *target) {
    if (strlen(target) > HTTP_MAX_TARGET_LEN) {
        return HTTP_PARSE_URI_TOO_LONG;
    }

    if (target[0] != '/') {
        return HTTP_PARSE_BAD_REQUEST;
    }

    for (size_t i = 0; target[i] != '\0'; i++) {
        unsigned char c = (unsigned char)target[i];
        if (c < '!' || c > '~') {
            return HTTP_PARSE_BAD_REQUEST;
        }
    }

    return HTTP_PARSE_OK;
}

static enum http_parse_result parse_request_line(char *line, struct http_request *req) {
    char *sp1 = strchr(line, ' ');

    if (sp1 == NULL) {
        return HTTP_PARSE_BAD_REQUEST;
    }

    *sp1 = '\0';
    req->method_str = line;
    char *target = sp1 + 1;

    char *sp2 = strchr(target, ' ');

    if (sp2 == NULL) {
        return HTTP_PARSE_BAD_REQUEST;
    }

    *sp2 = '\0';
    req->target = target;
    char *version = sp2 + 1;

    if (line[0] == '\0' || target[0] == '\0' || version[0] == '\0') {
        return HTTP_PARSE_BAD_REQUEST;
    }

    if (strspn(line, TOKEN_CHARS) != strlen(line)) {
        return HTTP_PARSE_BAD_REQUEST;
    }

    req->method = method_from_string(line);

    enum http_parse_result result = check_target(target);
    if (result != HTTP_PARSE_OK) {
        return result;
    }

    return HTTP_PARSE_OK;
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

    return HTTP_PARSE_OK;
}

const char *http_get_header(const struct http_request *req, const char *name) {
    (void)req;
    (void)name;
    return NULL;
}