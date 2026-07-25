#!/usr/bin/env bash
set -euo pipefail

ROOT=$(cd "$(dirname "$0")/.." && pwd)
URL=https://curl.se/ca/cacert.pem
EXPECTED_SHA256=86a1f3366afac7c6f8ae9f3c779ac221129328c43f0ab2b8817eb2f362a5025c
TMP=$(mktemp)
trap 'rm -f "$TMP"' EXIT

curl --proto '=https' --tlsv1.2 -fsSL "$URL" -o "$TMP"
ACTUAL=$(shasum -a 256 "$TMP" | awk '{print $1}')
if [[ "$ACTUAL" != "$EXPECTED_SHA256" ]]; then
    echo "error: unexpected cacert.pem SHA-256: $ACTUAL" >&2
    exit 1
fi

mkdir -p "$ROOT/romfs"
mv "$TMP" "$ROOT/romfs/cacert.pem"
trap - EXIT
echo "installed Mozilla CA bundle: $EXPECTED_SHA256"
