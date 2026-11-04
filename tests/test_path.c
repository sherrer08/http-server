#include <string.h>
#include "check.h"
#include "path.h"

static void test_decode(void) {
    static const struct {
        const char *target;
        enum path_result result;
        const char *expected;   /* only used when result is PATH_OK */
    } cases[] = {
        /* ordinary paths */
        { "/",              PATH_OK, "/" },
        { "/index.html",    PATH_OK, "/index.html" },
        { "/a/b/c.txt",     PATH_OK, "/a/b/c.txt" },
        /* escapes */
        { "/a%20b",         PATH_OK, "/a b" },
        { "/%41%42",        PATH_OK, "/AB" },
        { "/%4a%4A",        PATH_OK, "/JJ" },
        { "/caf%C3%A9",     PATH_OK, "/caf\xc3\xa9" },
        { "/a%2Fb",         PATH_OK, "/a/b" },
        { "/a+b",           PATH_OK, "/a+b" },
        /* query strings are cut off, and not checked */
        { "/a?x=1",         PATH_OK, "/a" },
        { "/?x",            PATH_OK, "/" },
        { "/a?x=%zz",       PATH_OK, "/a" },
        /* decoded exactly once */
        { "/%252e%252e",    PATH_OK, "/%2e%2e" },
        { "/%2e%2e/",       PATH_OK, "/../" },
        /* malformed escapes */
        { "/a%",            PATH_BAD, NULL },
        { "/a%4",           PATH_BAD, NULL },
        { "/a%zz",          PATH_BAD, NULL },
        { "/a%g1",          PATH_BAD, NULL },
        { "/a%G1",          PATH_BAD, NULL },
        { "/a%1g",          PATH_BAD, NULL },
        /* forbidden decoded bytes */
        { "/a%00",          PATH_BAD, NULL },
        { "/a%0A",          PATH_BAD, NULL },
        { "/a%1F",          PATH_BAD, NULL },
        { "/a%7F",          PATH_BAD, NULL },
        { "/a%20",          PATH_OK, "/a " },
    };
    const size_t count = sizeof(cases) / sizeof(cases[0]);

    for (size_t i = 0; i < count; i++) {
        char out[256];
        enum path_result r = path_decode(cases[i].target, out, sizeof(out));
        int ok;

        CHECK(r == cases[i].result);

        if (cases[i].result == PATH_OK) {
            ok = (r == PATH_OK && strcmp(out, cases[i].expected) == 0);
            CHECK(ok);
        } else {
            ok = (r == cases[i].result && out[0] == '\0');
            CHECK(out[0] == '\0');
        }

        if (!ok) {
            fprintf(stderr, "  (target was \"%s\")\n", cases[i].target);
        }
    }

    /* the output buffer's size limit */
    char small[5];
    CHECK(path_decode("/abc", small, 5) == PATH_OK);
    CHECK(strcmp(small, "/abc") == 0);
    CHECK(path_decode("/abc", small, 4) == PATH_TOO_LONG);
    CHECK(small[0] == '\0');
    CHECK(path_decode("/%41", small, 3) == PATH_OK);
    CHECK(strcmp(small, "/A") == 0);
    CHECK(path_decode("/", small, 1) == PATH_TOO_LONG);
    CHECK(small[0] == '\0');
}

static void test_is_safe(void) {
    static const struct {
        const char *path;
        int safe;
    } cases[] = {
        /* safe */
        { "/",                      1 },
        { "/index.html",            1 },
        { "/a/b/c.html",            1 },
        { "/a//b",                  1 },
        { "/a..b",                  1 },
        { "/a.b/c",                 1 },
        { "/blog/",                 1 },
        /* traversal */
        { "/..",                    0 },
        { "/../etc/passwd",         0 },
        { "/a/../../etc/passwd",    0 },
        { "/a/..",                  0 },
        { "/a/../b",                0 },
        { "//../etc/passwd",        0 },
        /* dot segments and hidden files */
        { "/.",                     0 },
        { "/./a",                   0 },
        { "/.hidden",               0 },
        { "/a/.git/config",         0 },
        { "/.env",                  0 },
        /* not starting with '/' */
        { "",                       0 },
        { "a/b",                    0 },
        { "../etc/passwd",          0 },
    };
    const size_t count = sizeof(cases) / sizeof(cases[0]);

    for (size_t i = 0; i < count; i++) {
        int got = path_is_safe(cases[i].path);
        CHECK(got == cases[i].safe);
        if (got != cases[i].safe) {
            fprintf(stderr, "  (path was \"%s\")\n", cases[i].path);
        }
    }
}

static void test_within_root(void) {
    static const struct {
        const char *root;
        const char *resolved;
        int expected;
    } cases[] = {
        { "/srv/www", "/srv/www",               1 },
        { "/srv/www", "/srv/www/index.html",    1 },
        { "/srv/www", "/srv/www/a/b/c",         1 },
        { "/srv/www", "/srv/www-secret",        0 },
        { "/srv/www", "/srv/www-secret/key",    0 },
        { "/srv/www", "/srv/wwwx/index.html",   0 },
        { "/srv/www", "/srv/ww",                0 },
        { "/srv/www", "/srv",                   0 },
        { "/srv/www", "/etc/passwd",            0 },
        { "/srv/www", "",                       0 },
    };
    const size_t count = sizeof(cases) / sizeof(cases[0]);

    for (size_t i = 0; i < count; i++) {
        int got = path_within_root(cases[i].root, cases[i].resolved);
        CHECK(got == cases[i].expected);
        if (got != cases[i].expected) {
            fprintf(stderr, "  (resolved was \"%s\")\n", cases[i].resolved);
        }
    }
}

int main(void) {
    test_decode();
    test_is_safe();
    test_within_root();
    return CHECK_SUMMARY();
}