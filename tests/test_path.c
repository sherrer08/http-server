#include <string.h>
#include "check.h"
#include "path.h"

static void test_decode(void) {
    char out[256];

    enum path_result r = path_decode("/index.html", out, sizeof(out));
    CHECK(r == PATH_OK);
    if (r == PATH_OK) {
        CHECK(strcmp(out, "/index.html") == 0);
    }

    r = path_decode("/a%zz", out, sizeof(out));
    CHECK(r == PATH_BAD);
    CHECK(out[0] == '\0');
}

static void test_is_safe(void) {
    CHECK(path_is_safe("/index.html") == 1);
    CHECK(path_is_safe("/../etc/password") == 0);
}

static void test_within_root(void) {
    CHECK(path_within_root("/srv/www", "/srv/www/index.html") == 1);
    CHECK(path_within_root("/srv/www", "/etc/passwd") == 0);
}

int main(void) {
    test_decode();
    test_is_safe();
    test_within_root();
    return CHECK_SUMMARY();
}