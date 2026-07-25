#!/usr/bin/env bash
set -euo pipefail

ROOT=$(cd "$(dirname "$0")/.." && pwd)
VERSION=2125.1.3
TOOLS_DIR="$ROOT/.tools"
DEST="$TOOLS_DIR/Azahar.app"
MARKER="$TOOLS_DIR/azahar-version"

if [[ $(uname -s) != Darwin ]]; then
    echo "error: automatic Azahar installation currently supports macOS only" >&2
    exit 1
fi

case $(uname -m) in
    arm64)
        ASSET="azahar-macos-arm64-$VERSION.zip"
        SHA256=bec0e28a4592b073ec285510935c24353bdae28bb443961d441ab768d66ffdac
        ;;
    x86_64)
        ASSET="azahar-macos-x86_64-$VERSION.zip"
        SHA256=9874f90982ea17cb12379c303fd63dd7cd169f64e3fdf8fb3852bae1b27d1f75
        ;;
    *)
        echo "error: unsupported macOS architecture: $(uname -m)" >&2
        exit 1
        ;;
esac

if [[ -x "$DEST/Contents/MacOS/azahar" && -f "$MARKER" ]] &&
   [[ $(<"$MARKER") == "$VERSION" ]]; then
    echo "Azahar $VERSION is already installed at $DEST"
    exit 0
fi

URL="https://github.com/azahar-emu/azahar/releases/download/$VERSION/$ASSET"
TMP=$(mktemp -d)
trap 'rm -rf "$TMP"' EXIT
echo "downloading Azahar $VERSION..."
curl --proto '=https' --tlsv1.2 -fL "$URL" -o "$TMP/azahar.zip"
ACTUAL=$(shasum -a 256 "$TMP/azahar.zip" | awk '{print $1}')
if [[ "$ACTUAL" != "$SHA256" ]]; then
    echo "error: Azahar checksum mismatch: $ACTUAL" >&2
    exit 1
fi
unzip -q "$TMP/azahar.zip" -d "$TMP/unpacked"
APP=$(find "$TMP/unpacked" -maxdepth 3 -name '*.app' -type d | head -1)
if [[ -z "$APP" ]]; then
    echo "error: Azahar.app was not found in the release archive" >&2
    exit 1
fi

mkdir -p "$TOOLS_DIR"
rm -rf "$DEST"
mv "$APP" "$DEST"
printf '%s\n' "$VERSION" > "$MARKER"
echo "installed Azahar $VERSION at $DEST"
