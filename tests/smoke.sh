#!/usr/bin/env bash
# Starts the server, checks a few responses, and stops it.
# Usage: tests/smoke.sh [path-to-server-binary]
set -euo pipefail

SERVER="${1:-./build/server}"
PORT=18080
URL="http://127.0.0.1:$PORT"

"$SERVER" "$PORT" > /dev/null &
SERVER_PID=$!
trap 'kill "$SERVER_PID" 2>/dev/null || true' EXIT

# Wait for the server to start listening.
for _ in $(seq 50); do
    if curl -s -o /dev/null "$URL/"; then
        break
    fi
    sleep 0.1
done

failures=0

check() {
    local name="$1" expected="$2"
    shift 2
    local actual
    actual=$(curl -s -o /dev/null -w '%{http_code}' "$@")
    if [ "$actual" = "$expected" ]; then
        echo "ok   $name ($actual)"
    else
        echo "FAIL $name: expected $expected, got $actual"
        failures=$((failures + 1))
    fi
}

check "GET"          200 "$URL/hello"
check "HEAD"         200 -I "$URL/hello"
check "HTTP/1.0"     200 --http1.0 "$URL/"
check "POST"         405 -X POST "$URL/"
check "unknown"      501 -X BREW "$URL/"
check "no Host"      400 -H 'Host:' "$URL/"
check "long target"  414 "$URL/$(head -c 1100 /dev/zero | tr '\0' a)"

echo "$failures failed
[ "$failures" -eq 0 ]

