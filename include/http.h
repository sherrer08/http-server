#ifndef HTTP_H
#define HTTP_H

#include <stddef.h>

#define HTTP_MAX_HEADERS 32
#define HTTP_MAX_TARGET_LEN 1024

/* Result of parsing a request; each error maps to an HTTP status code. */
enum http_parse_result {
    HTTP_PARSE_OK,                      // Status code depends on request
    HTTP_PARSE_BAD_REQUEST,             // Status code 400
    HTTP_PARSE_URI_TOO_LONG,            // Status code 414
    HTTP_PARSE_TOO_MANY_HEADERS,        // Status code 431
    HTTP_PARSE_VERSION_NOT_SUPPORTED    // Status code 505
};

/* The request methods the server recognizes; UNKNOWN for anything else. */
enum http_method {
    HTTP_METHOD_UNKNOWN,
    HTTP_METHOD_GET,
    HTTP_METHOD_HEAD,
    HTTP_METHOD_POST,
    HTTP_METHOD_PUT,
    HTTP_METHOD_DELETE,
    HTTP_METHOD_CONNECT,
    HTTP_METHOD_OPTIONS,
    HTTP_METHOD_TRACE,
    HTTP_METHOD_PATCH
};

/* One header line: name and value, pointing into the request buffer. */
struct http_header {
    const char *name;
    const char *value;
};

/* A parsed request. All string fields point into the buffer given to
   http_parse_request, and are valid only while that buffer is. */
struct http_request {
    enum http_method method;
    const char *method_str;     // method as sent, e.g. "GET"
    const char *target;         // request target, e.g. "/index.html"
    int version_minor;          // 0 for HTTP/1.0, 1 for HTTP/1.1
    struct http_header headers[HTTP_MAX_HEADERS];
    size_t header_count;        // number of valid entries in headers 
};

/*
 * Parses an HTTP/1.x request header section in place.
 * buf must hold exactly len bytes: the request line, the header lines, and
 * the terminating blank line, ending in "\r\n\r\n". Lines must end in "\r\n";
 * a bare "\n" is rejected as a bad request.
 * The parser writes '\0' bytes into buf to terminate each field.
 * On HTTP_PARSE_OK, *req is filled in with pointers into buf: it is valid only
 * while buf exists and is not modified. On any other result, *req must not be
 * used. Does no I/O.
 */
enum http_parse_result http_parse_request(char *buf, size_t len, struct http_request *req);

/*
 * Returns the value of the first header named name (compared
 * case-insensitively), or NULL if there is none. The returned string
 * points into the same buffer as req and has the same lifetime.
 */
const char *http_get_header(const struct http_request *req, const char *name);

#endif