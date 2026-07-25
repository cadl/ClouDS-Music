#!/usr/bin/env bash
set -euo pipefail

PROJECT_ROOT=$(cd "$(dirname "$0")/.." && pwd)
status=0

while IFS= read -r path; do
    basename=${path##*/}
    case "$basename" in
        auth.bin|dspfirm.cdc|hardware.log|.env|*.key|*.p12|*.part|*.dmp|*.dump|*.3dsx|*.cia|*.elf|*.smdh|*.bnr)
            echo "error: forbidden generated, sensitive, or proprietary file is tracked: $path" >&2
            status=1
            ;;
    esac
done < <(git -C "$PROJECT_ROOT" ls-files)

if [[ "$status" != 0 ]]; then
    exit 1
fi

echo "repository hygiene checks passed"
