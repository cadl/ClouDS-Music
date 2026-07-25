#!/usr/bin/env bash
set -euo pipefail

ROOT=$(cd "$(dirname "$0")/.." && pwd)
SRC="$ROOT/data/pinyin_dict_src"
THIRD_PARTY="$ROOT/third_party"
COMMIT=3f57a6f69b3393f9fccaae216d3439325786a62f
BASE="https://raw.githubusercontent.com/iDvel/rime-ice/$COMMIT"
mkdir -p "$SRC" "$THIRD_PARTY"

files=(
    "8105.dict.yaml:114111:94652d1e29e9d0d397de0d90f5a39e15f5a8edda4a6c3a7c5cebd9218ca8a664"
    "41448.dict.yaml:387281:873df74783f565e01581938b14bdf41b4e03a8834791f8778ebcbd70054a26d0"
    "others.dict.yaml:16862:6a6b1a77d94c7cdf9203cf426e67f350215d2d73259fe3769c97d2a18f521c28"
    "base.dict.yaml:16620104:903d70ceff821b12bcc46cc63b4ca829464f151bda958a52d500046e10343452"
    "ext.dict.yaml:11927296:1d8458d1f79d32eb5d4ef932fdc832a5c686ca330b57d54e97c816780cf2ea9b"
    "tencent.dict.yaml:17362395:95c22421f390acf067258184d03d6468dbf846bcf17259cf8d6bad440f2434a1"
)
LICENSE_SHA256=3972dc9744f6499f0f9b2dbf76696f2ae7ad8af9b23dde66d6af86c9dfb36986
OUTPUT_SHA256=3f33e2cb54e9c831ffcb92be0a4c3f4a59bea9e12dd0d9beb43e7cc1e2129a9d
TMP=$(mktemp -d "$ROOT/data/pinyin-fetch.XXXXXX")
trap 'rm -rf "$TMP"' EXIT

verify_sha256() {
    local expected=$1
    local file=$2
    local label=$3
    local actual
    actual=$(shasum -a 256 "$file" | awk '{print $1}')
    if [[ "$actual" != "$expected" ]]; then
        echo "error: $label SHA-256 is $actual, expected $expected" >&2
        return 1
    fi
}

for item in "${files[@]}"; do
    name=${item%%:*}
    remainder=${item#*:}
    expected_size=${remainder%%:*}
    expected_sha256=${remainder##*:}
    output="$SRC/$name"
    if [[ -f "$output" ]] &&
       [[ $(wc -c < "$output" | tr -d ' ') == "$expected_size" ]] &&
       verify_sha256 "$expected_sha256" "$output" "$name"; then
        echo "ready: $name"
        continue
    fi
    download="$TMP/$name"
    curl --proto '=https' --tlsv1.2 -fsSL --retry 3 \
        "$BASE/cn_dicts/$name" -o "$download"
    actual_size=$(wc -c < "$download" | tr -d ' ')
    if [[ "$actual_size" != "$expected_size" ]]; then
        echo "error: $name is $actual_size bytes, expected $expected_size" >&2
        exit 1
    fi
    verify_sha256 "$expected_sha256" "$download" "$name"
    mv "$download" "$output"
done

curl --proto '=https' --tlsv1.2 -fsSL "$BASE/LICENSE" \
    -o "$TMP/rime-ice-LGPL-3.0.txt"
verify_sha256 "$LICENSE_SHA256" "$TMP/rime-ice-LGPL-3.0.txt" \
    "rime-ice license"
mv "$TMP/rime-ice-LGPL-3.0.txt" \
    "$THIRD_PARTY/rime-ice-LGPL-3.0.txt"
python3 "$ROOT/tools/gen_pinyin_dict.py"
verify_sha256 "$OUTPUT_SHA256" "$ROOT/romfs/pinyin_dict.bin" \
    "generated pinyin_dict.bin"
