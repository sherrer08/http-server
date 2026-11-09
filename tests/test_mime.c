#include <string.h>
#include "check.h"
#include "mime.h"

static void test_types(void) {
    static const struct {
        const char *path;
        const char *type;
    } cases[] = {
        /* known extensions */
        { "/index.html",          "text/html; charset=utf-8" },
        { "/page.htm",            "text/html; charset=utf-8" },
        { "/css/style.css",       "text/css; charset=utf-8" },
        { "/app.js",              "text/javascript; charset=utf-8" },
        { "/data.json",           "application/json" },
        { "/notes.txt",           "text/plain; charset=utf-8" },
        { "/img/logo.png",        "image/png" },
        { "/photo.jpg",           "image/jpeg" },
        { "/photo.jpeg",          "image/jpeg" },
        { "/logo.svg",            "image/svg+xml" },
        /* case-insensitive */
        { "/INDEX.HTML",          "text/html; charset=utf-8" },
        { "/Photo.JPG",           "image/jpeg" },
        /* the last dot wins */
        { "/archive.tar.gz",      "application/octet-stream" },
        { "/report.final.pdf",    "application/pdf" },
        /* no extension */
        { "/README",              "application/octet-stream" },
        { "/my.folder/readme",    "application/octet-stream" },
        { "/file.",               "application/octet-stream" },
        { "/.htaccess",           "application/octet-stream" },
        { "/",                    "application/octet-stream" },
        { "",                     "application/octet-stream" },
        /* unknown extensions */
        { "/data.xyz",            "application/octet-stream" },
        { "/program.exe",         "application/octet-stream" },
        /* no leading slash */
        { "index.html",           "text/html; charset=utf-8" },
    };
    const size_t count = sizeof(cases) / sizeof(cases[0]);

    for (size_t i = 0; i < count; i++) {
        const char *result = mime_type_for_path(cases[i].path);
        int ok = (result != NULL && strcmp(result, cases[i].type) == 0);
        CHECK(ok);
        if (!ok) {
            fprintf(stderr, "  (path was \"%s\", got \"%s\")\n", cases[i].path, result ? result : "(null)");
        }
    }
}

int main(void) {
    test_types();
    return CHECK_SUMMARY();
}