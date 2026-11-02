# http-server

![CI](https://github.com/sherrer08/http-server/actions/workflows/ci.yml/badge.svg)

An HTTP/1.1 web server written from scratch in C, using only the standard library and Linux system calls. Built to learn sockets, HTTP parsing, and concurrency at the systems level.

## Status

**Work in progress.** The server parses and validates HTTP/1.0 and HTTP/1.1 requests and answers each with the correct status code. Serving files and handling multiple clients at once are next.

## What it does

- Reads each request until the end of its headers, however many pieces it arrives in, with a size limit
- Parses the request line and headers in place, validating them against [RFC 9112](https://www.rfc-editor.org/rfc/rfc9112):
  - method, request target, and version syntax
  - header names and values, including rejecting whitespace before the colon and obsolete line folding
  - exactly one `Host` header in HTTP/1.1
  - `Content-Length` parsed strictly as digits, with overflow detection
  - requests with both `Content-Length` and `Transfer-Encoding` rejected (a common request-smuggling technique)
- Answers `GET` with a plain-text body describing the request, and `HEAD` with identical headers and no body
- Responds to everything else with the matching status:

| Status | When |
|---|---|
| 400 Bad Request | malformed request line or headers, missing or duplicate `Host` |
| 405 Method Not Allowed | known methods other than `GET` and `HEAD` (with an `Allow` header) |
| 413 Content Too Large | requests with a body |
| 414 URI Too Long | request targets over 1024 characters |
| 431 Request Header Fields Too Large | header section over 4 KB, or more than 32 headers |
| 501 Not Implemented | unknown methods, or `Transfer-Encoding` |
| 505 HTTP Version Not Supported | well-formed versions other than 1.0 and 1.1 |

- Logs one line per connection: client address, request line, and status

## Requirements

- Linux
- gcc
- make
- curl (for the smoke test)

## Building and running

```bash
make                 # build the server and the tests into build/
make run             # build (if needed) and run the server on port 8080
./build/server 9000  # run on another port
make clean           # delete everything in build/
```

Then, from another terminal:

```bash
curl -i http://127.0.0.1:8080/hello
```

## Testing

```bash
make test        # unit tests for the HTTP parser
make test-asan   # the same tests with AddressSanitizer and UndefinedBehaviorSanitizer
make smoke       # starts the server and checks real responses with curl
make smoke-asan  # the smoke test against a sanitizer build of the server
make run-asan    # run the server itself with sanitizers enabled
```

The parser has over 250 unit test checks, covering every validation rule with both accepted and rejected inputs, including boundary cases such as the maximum target length, the maximum header count, and the largest representable `Content-Length`. The sanitizer builds catch memory errors (out-of-bounds access, use-after-free, leaks) and undefined behavior that a normal build can miss. CI runs all of these on every push.

## Known limitations

- **Single-threaded:** serves one client at a time.
- **No timeouts:** a client that sends a request very slowly can stall the server.
- **No request bodies:** requests with a body are rejected with 413.
- **No keep-alive:** each connection carries one request and is closed after the response.

## Roadmap

- **M3:** serve static files from a directory, with `sendfile`
- **M4:** keep-alive, read timeouts, and graceful shutdown
- **M5:** concurrency: a thread pool, then an `epoll` event loop, with benchmarks
- **M6:** fuzzing the parser and hardening against slow-client attacks

## Project layout

```
src/                 server, HTTP parser, and entry point
include/             header files
tests/               parser unit tests and the smoke test script
.github/workflows/   CI configuration
build/               compiler output (not committed)
```