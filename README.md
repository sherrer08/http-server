# <name>

![CI](https://github.com/sherrer08/http-server/actions/workflows/ci.yml/badge.svg)

An HTTP/1.1 web server written from scratch in C, using only the standard library and Linux system calls. Built to learn sockets, HTTP parsing, and concurrency at the systems level.

## Status

**Work in progress.** The build system, test suite, and sanitizer checks are in place. The server itself is being built next, starting with a basic socket listener.

## Requirements

- Linux (uses Linux-specific system calls such as `epoll` and `sendfile`)
- gcc
- make

## Building and running

```bash
make            # build the server and the tests into build/
make run        # build (if needed) and run the server
make clean      # delete everything in build/
```

## Testing

```bash
make test       # build and run the unit tests
make test-asan  # run the tests with AddressSanitizer and UndefinedBehaviorSanitizer
```

`test-asan` compiles the tests and library code with sanitizers enabled, which catches memory errors (out-of-bounds access, use-after-free, leaks) and undefined behavior (like signed integer overflow) that a normal build can miss.

## Project layout

```
src/       source files
include/   header files
tests/     test programs
build/     compiler output (not committed)
```
