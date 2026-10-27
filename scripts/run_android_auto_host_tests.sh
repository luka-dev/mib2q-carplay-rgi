#!/usr/bin/env bash
set -euo pipefail

ROOT=$(cd "$(dirname "$0")/.." && pwd)
IMAGE=${AA_TEST_IMAGE:-mib2q-aa-host-tests}
docker build -t "$IMAGE" "$ROOT/android_auto/hook/tests"
docker run --rm -v "$ROOT":/repo -w /repo/android_auto/hook "$IMAGE" bash -lc '
    set -e
    out=$(mktemp -d)
    trap "rm -rf $out" EXIT
    gcc -O1 -std=gnu99 -Isrc -Ivendor tests/ring_test.c src/transport.c -pthread -o "$out/ring"
    gcc -O1 -std=gnu99 -Isrc -Ivendor tests/navx_test.c -o "$out/navx"
    gcc -O1 -std=gnu99 -Isrc -Ivendor tests/uiconfig_test.c -o "$out/uiconfig"
    gcc -O1 -std=gnu99 -Isrc -Ivendor tests/rotary_test.c -o "$out/rotary"
    "$out/ring"
    "$out/navx"
    "$out/uiconfig"
    "$out/rotary"
    python3 tests/band_test.py
    python3 tests/stall_test.py
  '
python3 "$ROOT/tests/test_android_auto_installer.py"
python3 "$ROOT/tests/test_check_firmware.py"
