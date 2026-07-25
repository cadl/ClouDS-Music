#!/usr/bin/env bash
set -euo pipefail

ROOT=$(cd "$(dirname "$0")/.." && pwd)
APP=${AZAHAR_APP:-"$ROOT/.tools/Azahar.app"}
BIN="$APP/Contents/MacOS/azahar"

if [[ ! -x "$BIN" ]]; then
    echo "error: Azahar is not installed at $APP" >&2
    echo "run: make azahar-install" >&2
    exit 1
fi

if [[ $(uname -s) == Darwin ]]; then
    open -na "$APP" --args "$@"
else
    exec "$BIN" "$@"
fi
