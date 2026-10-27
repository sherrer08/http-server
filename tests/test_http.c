#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "http.h"

static int checks_run = 0;
static int checks_failed = 0;

#define CHECK(cond)                                                 \
    do {                                                            \
        checks_run++;                                               \
        if (!(cond)) {                                              \
            checks_failed++;                                        \
            fprintf(stderr, "%s:%d: CHECK failed: %s\n",            \
                    __FILE__, __LINE__, #cond);                     \
        }                                                           \
    } while (0)

static enum http_parse_result parse_len(const char *text, size_t len, char *buf, size_t buf_size, struct http_request *req) {
    if (len + 1 > buf_size) {
        fprintf(stderr, "test bug: request text does not fit in buffer\n");
        return HTTP_PARSE_BAD_REQUEST;
    }

    memcpy(buf, text, len);
    buf[len] = '\0';

    return http_parse_request(buf, len, req);
}

static enum http_parse_result parse(const char *text, char *buf, size_t buf_size, struct http_request *req) {
    return parse_len(text, strlen(text), buf, buf_size, req);
}

static void test_minimal_request(void) {
    char buf[1024];
    struct http_request req;
    enum http_parse_result result;

    result = parse("GET / HTTP/1.1\r\nHost: x\r\n\r\n", buf, sizeof(buf), &req);
    CHECK(result == HTTP_PARSE_OK);

    if (result == HTTP_PARSE_OK) {
        CHECK(req.method == HTTP_METHOD_GET);
        CHECK(strcmp(req.target, "/") == 0);
        CHECK(req.version_minor == 1);
        CHECK(req.header_count == 1);
        CHECK(strcmp(req.headers[0].name, "Host") == 0);
        CHECK(strcmp(req.headers[0].value, "x") == 0);
    }
}

static void test_garbage(void) {
    char buf[1024];
    struct http_request req;
    enum http_parse_result result;

    result = parse("hello\r\n\r\n", buf, sizeof(buf), &req);
    CHECK(result == HTTP_PARSE_BAD_REQUEST);
}

static void test_header_lookup(void) {
    char buf[1024];
    struct http_request req;
    enum http_parse_result result;

    result = parse("GET / HTTP/1.1\r\nHost: x\r\n\r\n", buf, sizeof(buf), &req);
    CHECK(result == HTTP_PARSE_OK);

    if (result == HTTP_PARSE_OK) {
        const char *host = http_get_header(&req, "host");
        CHECK(host != NULL);
        if (host != NULL) {
            CHECK(strcmp(host, "x") == 0);
        }

        CHECK(http_get_header(&req, "Accept") == NULL);
    }

}

static void test_input_checks(void) {
    char buf[1024];
    struct http_request req;
    enum http_parse_result result;

    static const char text[] = "GET / HTTP/1.1\r\nHost: x\0y\r\n\r\n";

    result = parse("GET / HTTP/1.1\r\nHost: x\r\n", buf, sizeof(buf), &req);
    CHECK(result == HTTP_PARSE_BAD_REQUEST);

    result = parse("", buf, sizeof(buf), &req);
    CHECK(result == HTTP_PARSE_BAD_REQUEST);

    result = parse("\r\n", buf, sizeof(buf), &req);
    CHECK(result == HTTP_PARSE_BAD_REQUEST);

    result = parse_len(text, sizeof(text) - 1, buf, sizeof(buf), &req);
    CHECK(result == HTTP_PARSE_BAD_REQUEST);
}

int main(void) {
    test_minimal_request();
    test_garbage();
    test_header_lookup();
    test_input_checks();

    printf("%d checks, %d failed\n", checks_run, checks_failed);

    if (checks_failed > 0) {
        return EXIT_FAILURE;
    } else {
        return EXIT_SUCCESS;
    }
}