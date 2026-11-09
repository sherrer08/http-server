# http-server

![CI](https://github.com/sherrer08/http-server/actions/workflows/ci.yml/badge.svg)

An HTTP/1.1 static-file web server written from scratch in C, using only the standard library and Linux system calls. Built to learn sockets, HTTP parsing, secure file handling, and concurrency at the systems level.

## Status

**Work in progress.** The server parses and validates HTTP/1.0 and HTTP/1.1 requests and serves static files from a document root, with layered defenses against path traversal. Keep-alive, timeouts, and concurrency are next.

## What it does

**Request handling**
- Reads each request until the end of its headers, however many pieces it arrives in, with a size limit
- Parses the request line and headers in place, validating them against [RFC 9112](https://www.rfc-editor.org/rfc/rfc9112): method, target, and version syntax; header names and values; exactly one `Host` in HTTP/1.1; strict `Content-Length` parsing with overflow detection; and rejection of `Content-Length` combined with `Transfer-Encoding` (a request-smuggling technique)

**File serving**
- Maps each request target to a file inside the document root; a folder serves its `index.html`
- Sets `Content-Type` from the file extension (HTML, CSS, JavaScript, JSON, text, common image types, PDF), with `application/octet-stream` for anything unknown
- Sends files with `sendfile`, copying data from disk to the socket inside the kernel, with full handling of partial sends (including files over 2 GB)
- Answers `HEAD` with the same headers as `GET`, including the real `Content-Length`, and no body

**Status codes**

| Status | When |
|---|---|
| 200 OK | a file was found and sent |
| 400 Bad Request | malformed request line, headers, or percent-encoding |
| 403 Forbidden | the file isn't readable or isn't a regular file, or a folder has no `index.html` |
| 404 Not Found | no such file, or a path that is refused for safety |
| 405 Method Not Allowed | methods other than `GET` and `HEAD` (with an `Allow` header) |
| 413 Content Too Large | requests with a body |
| 414 URI Too Long | request targets over 1024 characters |
| 431 Request Header Fields Too Large | header section over 4 KB, or more than 32 headers |
| 501 Not Implemented | unknown methods, or `Transfer-Encoding` |
| 505 HTTP Version Not Supported | versions other than 1.0 and 1.1 |

- Logs one line per connection: client address, request line, status, and whether the body was sent completely

## Security

Every request target comes from the client, so turning it into a path on disk is treated as untrusted input at each step:

- **Percent-decoding happens exactly once,** and every check runs on the decoded result, so encoded forms like `%2e%2e%2f` and double-encoded forms like `%252e` can't slip past the checks. Malformed escapes and decoded control characters are rejected.
- **Two layers against path traversal.** First, the decoded path is rejected if any segment is `..` or starts with `.` (which also keeps hidden files such as `.git` and `.env` from being served). Second, the full path is resolved with `realpath`, following every symbolic link, and the result must be the document root or inside it. The containment check requires the root to be followed by `/`, so a sibling folder like `www-secret` is never mistaken for `www`.
- **Files are checked through the open descriptor.** Files are opened with `O_NOFOLLOW` (so a symbolic link swapped in after the path was checked is refused) and `O_NONBLOCK` (so a named pipe can't freeze the server), then checked with `fstat` on the descriptor itself, closing the time-of-check to time-of-use gap. Only regular files are ever sent.
- **Clients can't crash the server by disconnecting:** `SIGPIPE` is ignored, so a client closing the connection mid-download is handled as an ordinary error.
- **Responses carry `X-Content-Type-Options: nosniff`,** so browsers trust the declared `Content-Type` instead of guessing.
- **No dynamic memory allocation:** every buffer has a fixed size, and every input has a hard limit.

## Requirements

- Linux (uses Linux-specific system calls such as `sendfile`)
- gcc
- make
- curl (for the smoke test)

## Building and running

```bash
make                         # build the server and the tests into build/
make run                     # build (if needed) and serve www/ on port 8080
./build/server 9000          # serve www/ on another port
./build/server 9000 ./site   # serve a different folder
make clean                   # delete everything in build/
```

Then open `http://127.0.0.1:8080/` in a browser, or:

```bash
curl -i http://127.0.0.1:8080/about.html
```

The `www/` folder holds a small sample site. The document root is resolved to an absolute path at startup; the server refuses to start if it isn't a directory, or if it's `/`.

## Testing

```bash
make test        # unit tests for the HTTP parser, path handling, and content types
make test-asan   # the same tests with AddressSanitizer and UndefinedBehaviorSanitizer
make smoke       # end-to-end tests against a running server
make smoke-asan  # the end-to-end tests against a sanitizer build of the server
make run-asan    # run the server itself with sanitizers enabled
```

- **Unit tests:** over 350 checks across three test programs (one per module), covering every parsing, decoding, and path rule with both accepted and rejected inputs, plus boundary cases such as the maximum target length, the largest `Content-Length`, and output buffers one byte too small.
- **End-to-end tests:** `tests/smoke.sh` builds a fresh test site in a temporary folder with `tests/make_site.sh`, including symbolic links that point outside the root, a sibling folder that shares the root's name as a prefix, an unreadable file, a locked folder, a named pipe, hidden files, and file names that need percent-encoding. It then checks status codes, content types, and file contents byte for byte, including eight different path-traversal attempts.

CI runs all of these on every push.

## Known limitations

- **Single-threaded:** serves one client at a time.
- **No timeouts:** a client that sends a request very slowly can stall the server.
- **No keep-alive:** each connection carries one request and is closed after the response.
- **No request bodies:** requests with a body are rejected with 413.
- **No directory listings:** a folder without `index.html` gets 403.
- **No trailing-slash redirect:** `/blog` serves `blog/index.html` directly, rather than redirecting to `/blog/`.
- **No range requests or caching headers** (`Range`, `ETag`, `Last-Modified`).

## Roadmap

- ~~**M1:** socket server~~
- ~~**M2:** HTTP request parser and status codes~~
- ~~**M3:** static file serving with `sendfile`, and path-traversal defenses~~
- **M4:** keep-alive, read timeouts, and graceful shutdown
- **M5:** concurrency: a thread pool, then an `epoll` event loop, with benchmarks
- **M6:** fuzzing the parser and hardening against slow-client attacks

## Project layout

```
src/                 server, HTTP parser, path handling, content types, entry point
include/             header files
tests/               unit tests, the shared test harness, and the end-to-end test scripts
www/                 the sample site
.github/workflows/   CI configuration
build/               compiler output (not committed)
```