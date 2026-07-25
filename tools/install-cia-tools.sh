#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
PLATFORM="$(uname -s)-$(uname -m)"
PREFIX="$ROOT_DIR/.tools/cia-tools/$PLATFORM"
DOWNLOAD_DIR="$ROOT_DIR/.tools/cia-tools/downloads"
SOURCE_DIR="$ROOT_DIR/.tools/cia-tools/sources/$PLATFORM"

MAKEROM_COMMIT="e8f5f529c54ff9b22a2491a480ffa69206bf7b19"
MAKEROM_SHA256="6b757ab8b8e4715047db9ddeb91f89e10e7165c3e803315d9d131a9297ca4fe0"
MAKEROM_URL="https://codeload.github.com/3DSGuy/Project_CTR/tar.gz/$MAKEROM_COMMIT"

BANNERTOOL_COMMIT="734d33be79fd3f8c29c6296158f06ac7c5ca9dcb"
BANNERTOOL_SHA256="5619490d9057a6bc44f187453b3fae832bcc173976136ddee2e0170bdaea4bf1"
BANNERTOOL_URL="https://codeload.github.com/carstene1ns/3ds-bannertool/tar.gz/$BANNERTOOL_COMMIT"

VERSION_KEY="makerom=$MAKEROM_COMMIT bannertool=$BANNERTOOL_COMMIT patches=2"
VERSION_FILE="$PREFIX/versions.txt"

if [[ -x "$PREFIX/bin/makerom" && -x "$PREFIX/bin/bannertool" && \
      -f "$VERSION_FILE" ]] && grep -Fqx "$VERSION_KEY" "$VERSION_FILE"; then
    echo "CIA tools already installed for $PLATFORM."
    exit 0
fi

for command in curl tar make cmake patch; do
    if ! command -v "$command" >/dev/null 2>&1; then
        echo "error: required command not found: $command" >&2
        exit 1
    fi
done

mkdir -p "$PREFIX/bin" "$DOWNLOAD_DIR" "$SOURCE_DIR"

verify_sha256() {
    local expected="$1"
    local file="$2"
    if command -v sha256sum >/dev/null 2>&1; then
        printf '%s  %s\n' "$expected" "$file" | sha256sum -c - >/dev/null
    else
        printf '%s  %s\n' "$expected" "$file" | shasum -a 256 -c - >/dev/null
    fi
}

fetch_archive() {
    local url="$1"
    local expected="$2"
    local output="$3"
    if [[ -f "$output" ]] && verify_sha256 "$expected" "$output"; then
        return
    fi
    if [[ -f "$output" ]]; then
        mv "$output" "$output.invalid.$(date +%s)"
    fi
    curl -fL "$url" -o "$output.download"
    verify_sha256 "$expected" "$output.download"
    mv "$output.download" "$output"
}

extract_archive() {
    local archive="$1"
    local destination="$2"
    local marker="$destination/.source-ready"
    if [[ -f "$marker" ]]; then
        return
    fi
    mkdir -p "$destination"
    tar -xzf "$archive" --strip-components=1 -C "$destination"
    printf 'ready\n' > "$marker"
}

if command -v nproc >/dev/null 2>&1; then
    JOBS="$(nproc)"
else
    JOBS="$(sysctl -n hw.ncpu 2>/dev/null || printf '2')"
fi
if [[ "$JOBS" -gt 4 ]]; then JOBS=4; fi

# The project Makefile exports ARM target flags. Host-side packaging tools must
# not inherit them when they configure their native compiler.
HOST_ENV=(env -u CC -u CXX -u CPPFLAGS -u CFLAGS -u CXXFLAGS \
    -u LDFLAGS -u LD -u AR -u AS)

MAKEROM_ARCHIVE="$DOWNLOAD_DIR/Project_CTR-$MAKEROM_COMMIT.tar.gz"
BANNERTOOL_ARCHIVE="$DOWNLOAD_DIR/bannertool-$BANNERTOOL_COMMIT.tar.gz"
MAKEROM_SOURCE="$SOURCE_DIR/Project_CTR-$MAKEROM_COMMIT"
BANNERTOOL_SOURCE="$SOURCE_DIR/bannertool-$BANNERTOOL_COMMIT"

fetch_archive "$MAKEROM_URL" "$MAKEROM_SHA256" "$MAKEROM_ARCHIVE"
fetch_archive "$BANNERTOOL_URL" "$BANNERTOOL_SHA256" "$BANNERTOOL_ARCHIVE"
extract_archive "$MAKEROM_ARCHIVE" "$MAKEROM_SOURCE"
extract_archive "$BANNERTOOL_ARCHIVE" "$BANNERTOOL_SOURCE"

if ! grep -Fq 'MAKEROM_VERSION_MINOR = VER_MINOR' \
        "$MAKEROM_SOURCE/makerom/src/user_settings.c"; then
    patch -d "$MAKEROM_SOURCE" -p1 \
        < "$ROOT_DIR/tools/patches/makerom-title-version.patch"
elif grep -Fq 'APP_NAME, VER_MAJOR, VER_MINOR, VER_PATCH, AUTHORS' \
        "$MAKEROM_SOURCE/makerom/src/user_settings.c"; then
    patch -d "$MAKEROM_SOURCE" -p1 \
        < "$ROOT_DIR/tools/patches/makerom-display-version.patch"
fi

if grep -Fq 'u8 pad[padLength] = {0};' \
        "$BANNERTOOL_SOURCE/source/3ds/lz11.cpp"; then
    patch -d "$BANNERTOOL_SOURCE" -p1 \
        < "$ROOT_DIR/tools/patches/bannertool-fixed-padding.patch"
elif ! grep -Fq 'u8 pad[4] = {0};' \
        "$BANNERTOOL_SOURCE/source/3ds/lz11.cpp"; then
    echo "error: bannertool padding source no longer matches the pinned patch" >&2
    exit 1
fi

echo "Building makerom for $PLATFORM..."
"${HOST_ENV[@]}" make -C "$MAKEROM_SOURCE/makerom" deps -j"$JOBS"
"${HOST_ENV[@]}" make -C "$MAKEROM_SOURCE/makerom" -j"$JOBS"
install -m 755 "$MAKEROM_SOURCE/makerom/bin/makerom" "$PREFIX/bin/makerom"

echo "Building bannertool for $PLATFORM..."
"${HOST_ENV[@]}" cmake --fresh -S "$BANNERTOOL_SOURCE" \
    -B "$BANNERTOOL_SOURCE/build" \
    -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX="$PREFIX"
"${HOST_ENV[@]}" cmake --build "$BANNERTOOL_SOURCE/build" \
    --parallel "$JOBS"
"${HOST_ENV[@]}" cmake --install "$BANNERTOOL_SOURCE/build"

printf '%s\n' "$VERSION_KEY" > "$VERSION_FILE"
echo "Installed CIA tools in $PREFIX/bin."
