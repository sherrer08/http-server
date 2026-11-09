#!/usr/bin/env bash
# Builds the test site, starts the server on it, checks its responses, and
# cleans up.
# Usage: tests/smoke.sh [path-to-server-binary]
set -euo pipefail

SERVER="${1:-./build/server}"
PORT=18080
URL="http://127.0.0.1:$PORT"

BASE=""
SERVER_PID=""

cleanup() {
    if [ -n "$SERVER_PID" ]; then
        kill "$SERVER_PID" 2>/dev/null || true
        wait "$SERVER_PID" 2>/dev/null || true
    fi
    if [ -n "$BASE" ]; then
        chmod -R u+rwx "$BASE" 2>/dev/null || true
        rm -rf "$BASE"
    fi
}
trap cleanup EXIT

# --- build the test site and start the server on it ---
BASE="$(mktemp -d)"
SITE="$BASE/site"
"$(dirname "$0")/make_site.sh" "$BASE"

"$SERVER" "$PORT" "$SITE" > "$BASE/server.log" 2>&1 &
SERVER_PID=$!

# wait for the server to start listening
for _ in $(seq 50); do
    if curl -s -o /dev/null --max-time 1 "$URL/"; then
        break
    fi
    sleep 0.1
done

if ! kill -0 "$SERVER_PID" 2>/dev/null; then
    echo "server failed to start:"
    cat "$BASE/server.log"
    exit 1
fi

failures=0

report() {
    local ok="$1" name="$2" detail="$3"
    if [ "$ok" -eq 1 ]; then
        echo "ok   $name"
    else
        echo "FAIL $name: $detail"
        failures=$((failures + 1))
    fi
}

# check NAME EXPECTED_STATUS CURL_ARGS...
check() {
    local name="$1" expected="$2"
    shift 2
    local actual
    actual=$(curl -s -o /dev/null -w '%{http_code}' --max-time 5 "$@" || true)
    if [ "$actual" = "$expected" ]; then
        report 1 "$name" ""
    else
        report 0 "$name" "expected $expected, got $actual"
    fi
}

# check_type NAME EXPECTED_TYPE CURL_ARGS...
check_type() {
    local name="$1" expected="$2"
    shift 2
    local actual
    actual=$(curl -s -o /dev/null -w '%{content_type}' --max-time 5 "$@" || true)
    if [ "$actual" = "$expected" ]; then
        report 1 "$name" ""
    else
        report 0 "$name" "expected \"$expected\", got \"$actual\""
    fi
}

# check_body NAME FILE CURL_ARGS...
check_body() {
    local name="$1" file="$2"
    shift 2
    if curl -s --max-time 30 "$@" | cmp -s - "$file"; then
        report 1 "$name" ""
    else
        report 0 "$name" "body differs from $file"
    fi
}

# --- ordinary files ---
check      "GET /"              200 "$URL/"
check      "GET file"           200 "$URL/about.html"
check      "HEAD file"          200 -I "$URL/about.html"
check      "HTTP/1.0"           200 --http1.0 "$URL/"
check      "subfolder index"    200 "$URL/sub/"
check      "empty file"         200 "$URL/empty.txt"
check      "space in name"      200 "$URL/file%20with%20space.txt"
check      "non-ASCII name"     200 "$URL/caf%C3%A9.txt"
check      "query string"       200 "$URL/about.html?x=1"
check      "link inside root"   200 "$URL/inside-link.html"

# --- content types ---
check_type "html type"   "text/html; charset=utf-8"  "$URL/"
check_type "css type"    "text/css; charset=utf-8"   "$URL/style.css"
check_type "svg type"    "image/svg+xml"             "$URL/logo.svg"
check_type "bin type"    "application/octet-stream"  "$URL/random.bin"
check_type "error type"  "text/plain; charset=utf-8" "$URL/missing.html"

# --- bodies, byte for byte ---
check_body "about body"   "$SITE/about.html"  "$URL/about.html"
check_body "random body"  "$SITE/random.bin"  "$URL/random.bin"
check_body "empty body"   "$SITE/empty.txt"   "$URL/empty.txt"
check_body "accent body"  "$SITE/café.txt"    "$URL/caf%C3%A9.txt"

# --- not found ---
check "missing file"      404 "$URL/missing.html"
check "file as folder"    404 "$URL/about.html/x"
check "hidden file"       404 "$URL/.hidden"

# --- traversal, every form ---
check "dotdot"            404 --path-as-is "$URL/../etc/passwd"
check "deep dotdot"       404 --path-as-is "$URL/../../../../etc/passwd"
check "encoded dotdot"    404 "$URL/%2e%2e/%2e%2e/etc/passwd"
check "encoded slash"     404 "$URL/..%2f..%2fetc%2fpasswd"
check "double encoded"    404 "$URL/%252e%252e/etc/passwd"
check "escape link"       404 "$URL/escape-link/etc/passwd"
check "escape file link"  404 "$URL/escape-file-link"
check "prefix sibling"    404 "$URL/secret-link/key.txt"

# --- refused ---
check "no index"          403 "$URL/noindex/"
check "named pipe"        403 "$URL/pipe"
if [ "$(id -u)" -ne 0 ]; then
    check "unreadable file" 403 "$URL/unreadable.txt"
    check "locked folder"   403 "$URL/locked/file.txt"
else
    echo "skip unreadable checks (running as root)"
fi

# --- bad requests ---
check "malformed escape"  400 "$URL/a%zz"
check "POST"              405 -X POST "$URL/"
check "unknown method"    501 -X BREW "$URL/"
check "no Host"           400 -H 'Host:' "$URL/"
check "long target"       414 "$URL/$(head -c 1100 /dev/zero | tr '\0' a)"

echo "$failures failed"

if [ "$failures" -ne 0 ]; then
    echo "--- server log ---"
    cat "$BASE/server.log"
    exit 1
fi