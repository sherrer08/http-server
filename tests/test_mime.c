#include <string.h>
#include "check.h"
#include "mime.h"

static void test_types(void) {
    const char *t;

    t = mime_type_for_path("index.html");
    CHECK(t != NULL);
    if (t != NULL) {
        CHECK(strcmp(t, "text/html; charset=utf-8") == 0);
    }

    t = mime_type_for_path("data.xyz");
    CHECK(t != NULL);
    if (t != NULL) {
        CHECK(strcmp(t, "application/octet-stream") == 0);
    }
}

int main(void) {
    test_types();
    return CHECK_SUMMARY();
}