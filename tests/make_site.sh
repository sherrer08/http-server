#!/usr/bin/env bash
# Builds the test site used by smoke.sh.
# Usage: tests/make_site.sh BASE_DIR
# Creates BASE_DIR/site (the document root) and BASE_DIR/site-secret
# (a folder next to it, which the server must never serve).
set -euo pipefail

if [ "$#" -ne 1 ]; then
    echo "usage: $0 BASE_DIR" >&2
    exit 1
fi

BASE="$1"
SITE="$BASE/site"
SECRET="$BASE/site-secret"

# the repository's top folder, wherever the script is run from
REPO="$(cd "$(dirname "$0")/.." && pwd)"

mkdir -p "$SITE/sub" "$SITE/noindex" "$SITE/locked" "$SECRET"

# --- ordinary files ---
cp "$REPO/www/index.html" "$REPO/www/about.html" \
   "$REPO/www/style.css" "$REPO/www/logo.svg" "$SITE/"
echo '<!DOCTYPE html><title>sub</title><p>sub index</p>' > "$SITE/sub/index.html"
echo 'a page in a folder without index.html' > "$SITE/noindex/page.html"

# --- edge cases for decoding and sending ---
: > "$SITE/empty.txt"
echo 'spaces' > "$SITE/file with space.txt"
echo 'accent' > "$SITE/café.txt"
head -c 10485760 /dev/urandom > "$SITE/random.bin"

# --- files the server must refuse ---
echo 'hidden' > "$SITE/.hidden"
echo 'unreadable' > "$SITE/unreadable.txt"
chmod 000 "$SITE/unreadable.txt"
echo 'locked' > "$SITE/locked/file.txt"
chmod 000 "$SITE/locked"
mkfifo "$SITE/pipe"

# --- symbolic links: one inside the root, three leading outside ---
ln -s about.html "$SITE/inside-link.html"
ln -s / "$SITE/escape-link"
ln -s /etc/passwd "$SITE/escape-file-link"
echo 'secret' > "$SECRET/key.txt"
ln -s ../site-secret "$SITE/secret-link"