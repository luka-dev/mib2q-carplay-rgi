#!/usr/bin/env bash
# Cross-build with the same C code-generation flags as libcarplay_hook.so.
set -euo pipefail

HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT="$(cd "$HERE/../../.." && pwd)"
VARIANT=4.9

if [[ "${1:-}" == "--variant" ]]; then
    VARIANT="${2:?missing QNX toolchain variant}"
    shift 2
fi
[[ $# -eq 0 ]] || { echo "usage: $0 [--variant 4.9|8.5]" >&2; exit 2; }

IMAGE="qnx65-armv7-toolchain:$VARIANT"
docker image inspect "$IMAGE" >/dev/null 2>&1 || {
    echo "error: Docker image $IMAGE is not installed" >&2
    exit 1
}

# APAC_CFLAGS_NOINC is also the maintained source of the hook C flags.  Add
# -fPIC because the shipping translation unit is built into a shared object.
# Keeping the definition outside the quoted Docker program prevents drift.
. "$ROOT/scripts/apac_flags.sh"

docker run --rm --platform=linux/amd64 \
    -v "$ROOT":/src -w /src "$IMAGE" bash -c '
set -e
export PATH=/opt/qnx650/host/linux/x86/usr/bin:$PATH
export QNX_HOST=/opt/qnx650/host/linux/x86
export QNX_TARGET=/opt/qnx650/target/qnx6
CC=arm-unknown-nto-qnx6.5.0eabi-gcc
READELF=arm-unknown-nto-qnx6.5.0eabi-readelf
OUT=build/stereo-upmix-bench/qnx-"'"$VARIANT"'"
rm -rf "$OUT"
mkdir -p "$OUT"
CFLAGS="'"$APAC_CFLAGS_NOINC"' -fPIC -D__QNX__ -Ihook/audio"

$CC $CFLAGS -Werror -c \
    tools/stereo-upmix-render/bench/stereo_upmix_bench.c \
    -o "$OUT/stereo_upmix_bench.o"
$CC $CFLAGS -c hook/audio/stereo_upmix.c -o "$OUT/stereo_upmix.o"
$CC "$OUT/stereo_upmix_bench.o" "$OUT/stereo_upmix.o" \
    -o "$OUT/stereo_upmix_bench" -lm

$READELF -h "$OUT/stereo_upmix_bench" | grep -E "Class:|Machine:"
echo "built $OUT/stereo_upmix_bench ($(stat -c %s "$OUT/stereo_upmix_bench") bytes)"
'
