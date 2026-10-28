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
        if (req.header_count == 1) {
            CHECK(strcmp(req.headers[0].name, "Host") == 0);
            CHECK(strcmp(req.headers[0].value, "x") == 0);
        }
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

    result = parse("\r\n\r\n", buf, sizeof(buf), &req);
    CHECK(result == HTTP_PARSE_BAD_REQUEST);

    result = parse("\r\nGET / HTTP/1.1\r\nHost: x\r\n\r\n", buf, sizeof(buf), &req);
    CHECK(result == HTTP_PARSE_BAD_REQUEST);
}

static void test_methods(void) {
    static const struct {
        const char *name;
        enum http_method expected;
    } cases[] = {
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
    const size_t count = sizeof(cases) / sizeof(cases[0]);

    for (size_t i = 0; i < count; i++) {
        char text[256];
        char buf[1024];
        struct http_request req;
        enum http_parse_result result;

        snprintf(text, sizeof(text), "%s / HTTP/1.1\r\nHost: x\r\n\r\n", cases[i].name);

        result = parse(text, buf, sizeof(buf), &req);
        CHECK(result == HTTP_PARSE_OK);
        CHECK(req.method == cases[i].expected);

        if (result != HTTP_PARSE_OK || req.method != cases[i].expected) {
            fprintf(stderr, " (method was %s)\n", cases[i].name);
        }
    }

    char buf[1024];
    struct http_request req;
    enum http_parse_result result;

    result = parse("BREW / HTTP/1.1\r\nHost: x\r\n\r\n", buf, sizeof(buf), &req);
    CHECK(result == HTTP_PARSE_OK);
    CHECK(req.method == HTTP_METHOD_UNKNOWN);
    if (result == HTTP_PARSE_OK) {
        CHECK(strcmp(req.method_str, "BREW") == 0);
    }

    result = parse("get / HTTP/1.1\r\nHost: x\r\n\r\n", buf, sizeof(buf), &req);
    CHECK(result == HTTP_PARSE_OK);
    CHECK(req.method == HTTP_METHOD_UNKNOWN);
    if (result == HTTP_PARSE_OK) {
        CHECK(strcmp(req.method_str, "get") == 0);
    }

    result = parse("GE(T / HTTP/1.1\r\nHost: x\r\n\r\n", buf, sizeof(buf), &req);
    CHECK(result == HTTP_PARSE_BAD_REQUEST);

    result = parse("G/T / HTTP/1.1\r\nHost: x\r\n\r\n", buf, sizeof(buf), &req);
    CHECK(result == HTTP_PARSE_BAD_REQUEST);

    result = parse("GE\tT / HTTP/1.1\r\nHost: x\r\n\r\n", buf, sizeof(buf), &req);
    CHECK(result == HTTP_PARSE_BAD_REQUEST);

    result = parse("G\xc3\x89T / HTTP/1.1\r\nHost: x\r\n\r\n", buf, sizeof(buf), &req);
    CHECK(result == HTTP_PARSE_BAD_REQUEST);
}

static void test_targets(void) {
    static const struct {
        const char *target;
        enum http_parse_result expected;
    } cases[] = {
        /* valid */
        { "/",                      HTTP_PARSE_OK },
        { "/a/b/c.html",            HTTP_PARSE_OK },
        { "/search?q=hello&page=2", HTTP_PARSE_OK },
        { "/a%20b",                 HTTP_PARSE_OK },
        { "/~user/file",            HTTP_PARSE_OK },
        /* invalid */
        { "index.html",             HTTP_PARSE_BAD_REQUEST },
        { "http://example.com/",    HTTP_PARSE_BAD_REQUEST },
        { "*",                      HTTP_PARSE_BAD_REQUEST },
        { "/a\tb",                  HTTP_PARSE_BAD_REQUEST },
        { "/a\x01" "b",             HTTP_PARSE_BAD_REQUEST },
        { "/a\x7f",                 HTTP_PARSE_BAD_REQUEST },
        { "/caf\xc3\xa9",           HTTP_PARSE_BAD_REQUEST },
    };
    const size_t count = sizeof(cases) / sizeof(cases[0]);

    for (size_t i = 0; i < count; i++) {
        char text[256];
        char buf[1024];
        struct http_request req;
        enum http_parse_result result;

        snprintf(text, sizeof(text), "GET %s HTTP/1.1\r\nHost: x\r\n\r\n", cases[i].target);

        result = parse(text, buf, sizeof(buf), &req);
        CHECK(result == cases[i].expected);

        if (result == HTTP_PARSE_OK && cases[i].expected == HTTP_PARSE_OK) {
            CHECK(strcmp(req.target, cases[i].target) == 0);
        }

        if (result != cases[i].expected) {
            fprintf(stderr, " (target was \"%s\")\n", cases[i].target);
        }
    }

    char path[HTTP_MAX_TARGET_LEN + 2];
    char text[2048];
    char buf[2048];
    struct http_request req;
    enum http_parse_result result;

    memset(path, 'a', HTTP_MAX_TARGET_LEN - 1);
    path[HTTP_MAX_TARGET_LEN - 1] = '\0';

    snprintf(text, sizeof(text), "GET /%s HTTP/1.1\r\nHost: x\r\n\r\n", path);
    result = parse(text, buf, sizeof(buf), &req);
    CHECK(result == HTTP_PARSE_OK);
    if (result == HTTP_PARSE_OK) {
        CHECK(strlen(req.target) == HTTP_MAX_TARGET_LEN);
    }

    memset(path, 'a', HTTP_MAX_TARGET_LEN);
    path[HTTP_MAX_TARGET_LEN] = '\0';

    snprintf(text, sizeof(text), "GET /%s HTTP/1.1\r\nHost: x\r\n\r\n", path);
    result = parse(text, buf, sizeof(buf), &req);
    CHECK(result == HTTP_PARSE_URI_TOO_LONG);

    memset(path, 'a', HTTP_MAX_TARGET_LEN + 1);
    path[HTTP_MAX_TARGET_LEN + 1] = '\0';

    snprintf(text, sizeof(text), "GET %s HTTP/1.1\r\nHost: x\r\n\r\n", path);
    result = parse(text, buf, sizeof(buf), &req);
    CHECK(result == HTTP_PARSE_URI_TOO_LONG);
}

static void test_versions(void) {
    static const struct {
        const char *version;
        enum http_parse_result expected;
        int minor;
    } cases[] = {
        { "HTTP/1.1",  HTTP_PARSE_OK, 1 },
        { "HTTP/1.0",  HTTP_PARSE_OK, 0 },
        /* 505: well-formed, unsupported */
        { "HTTP/2.0",  HTTP_PARSE_VERSION_NOT_SUPPORTED, 0 },
        { "HTTP/1.2",  HTTP_PARSE_VERSION_NOT_SUPPORTED, 0 },
        { "HTTP/0.9",  HTTP_PARSE_VERSION_NOT_SUPPORTED, 0 },
        { "HTTP/9.9",  HTTP_PARSE_VERSION_NOT_SUPPORTED, 0 },
        /* 400: not a valid version */
        { "http/1.1",  HTTP_PARSE_BAD_REQUEST, 0 },
        { "HTTPS/1.1", HTTP_PARSE_BAD_REQUEST, 0 },
        { "HTTP/1.10", HTTP_PARSE_BAD_REQUEST, 0 },
        { "HTTP/1",    HTTP_PARSE_BAD_REQUEST, 0 },
        { "HTTP/11",   HTTP_PARSE_BAD_REQUEST, 0 },
        { "HTTP/a.b",  HTTP_PARSE_BAD_REQUEST, 0 },
        { "HTTP/1.1x", HTTP_PARSE_BAD_REQUEST, 0 },
    };

    const size_t count = sizeof(cases) / sizeof(cases[0]);

    for (size_t i = 0; i < count; i++) {
        char text[256];
        char buf[1024];
        struct http_request req;
        enum http_parse_result result;

        snprintf(text, sizeof(text), "GET / %s\r\nHost: x\r\n\r\n", cases[i].version);

        result = parse(text, buf, sizeof(buf), &req);
        CHECK(result == cases[i].expected);

        if (result == HTTP_PARSE_OK && cases[i].expected == HTTP_PARSE_OK) {
            CHECK(req.version_minor == cases[i].minor);
        }

        if (result != cases[i].expected) {
            fprintf(stderr, " (version was \"%s\")\n", cases[i].version);
        }
    }

    char buf[1024];
    struct http_request req;
    enum http_parse_result result;

    /* a third space puts "HTTP/1.1 x" in the version */
    result = parse("GET / HTTP/1.1 x\r\nHost: x\r\n\r\n", buf, sizeof(buf), &req);
    CHECK(result == HTTP_PARSE_BAD_REQUEST);

    /* a bare \n makes the request line run into the next line */
    result = parse("GET / HTTP/1.1\nHost: x\r\n\r\n", buf, sizeof(buf), &req);
    CHECK(result == HTTP_PARSE_BAD_REQUEST);
}

static void test_headers(void) {
    char buf[1024];
    struct http_request req;
    enum http_parse_result result;

    /* no headers: the first line after the request line is the blank one */
    result = parse("GET / HTTP/1.0\r\n\r\n", buf, sizeof(buf), &req);
    CHECK(result == HTTP_PARSE_OK);
    CHECK(req.header_count == 0);

    /* anything after the blank line is rejected */
    result = parse("GET / HTTP/1.1\r\nHost: x\r\n\r\nextra\r\n\r\n", buf, sizeof(buf), &req);
    CHECK(result == HTTP_PARSE_BAD_REQUEST);

    /* two requests in one buffer (request smuggling) */
    result = parse("GET / HTTP/1.1\r\nHost: x\r\n\r\n"
                   "GET /admin HTTP/1.1\r\nHost: x\r\n\r\n", buf, sizeof(buf), &req);
    CHECK(result == HTTP_PARSE_BAD_REQUEST);
}

int main(void) {
    test_minimal_request();
    test_garbage();
    test_header_lookup();
    test_input_checks();
    test_methods();
    test_targets();
    test_versions();
    test_headers();

    printf("%d checks, %d failed\n", checks_run, checks_failed);

    if (checks_failed > 0) {
        return EXIT_FAILURE;
    } else {
        return EXIT_SUCCESS;
    }
}