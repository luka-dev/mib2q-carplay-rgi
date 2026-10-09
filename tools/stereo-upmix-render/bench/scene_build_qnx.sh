#!/usr/bin/env bash
# Cross-build scene_engine_bench (or, with --bench mask, scene_mask_bench) the way the hook is built:
# the hook's C flags (scripts/apac_flags.sh, -O2), -mfpu=neon without vfpv4 (no fused
# multiply-add), softfp, -Werror on every file.
# SCENE_CFLAGS_EXTRA adds flags (e.g. "-O3 -fno-math-errno" to try the upmix DSP flags).
set -euo pipefail

HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT="$(cd "$HERE/../../.." && pwd)"
VARIANT=latest
BENCH=engine

while [[ $# -gt 0 ]]; do
    case "$1" in
        --variant) VARIANT="${2:?missing QNX toolchain variant}"; shift 2 ;;
        --bench) BENCH="${2:?missing bench}"; shift 2 ;;
        *) echo "usage: $0 [--variant latest|4.9|8.5] [--bench engine|mask]" >&2; exit 2 ;;
    esac
done
case "$BENCH" in
    engine) SRCS="scene_engine scene_engine_math scene_clusters scene_mask scene_mask_worker"; BIN=scene_engine_bench; OUTDIR=scene-engine-bench ;;
    mask)   SRCS="scene_mask"; BIN=scene_mask_bench; OUTDIR=scene-mask-bench ;;
    *) echo "error: unknown bench $BENCH (engine|mask)" >&2; exit 2 ;;
esac

IMAGE="qnx65-armv7-toolchain:$VARIANT"
docker image inspect "$IMAGE" >/dev/null 2>&1 || {
    echo "error: Docker image $IMAGE is not installed" >&2
    exit 1
}

. "$ROOT/scripts/apac_flags.sh"

docker run --rm --platform=linux/amd64 \
    -v "$ROOT":/src -w /src "$IMAGE" bash -c '
set -e
export PATH=/opt/qnx650/host/linux/x86/usr/bin:$PATH
export QNX_HOST=/opt/qnx650/host/linux/x86
export QNX_TARGET=/opt/qnx650/target/qnx6
CC=arm-unknown-nto-qnx6.5.0eabi-gcc
READELF=arm-unknown-nto-qnx6.5.0eabi-readelf
OUT=build/'"$OUTDIR"'/qnx-"'"$VARIANT"'"
BIN='"$BIN"'
rm -rf "$OUT"
mkdir -p "$OUT"
CFLAGS="'"$APAC_CFLAGS_NOINC"' -fPIC -D__QNX__ -mfpu=neon -mfloat-abi=softfp -Ihook/audio '"${SCENE_CFLAGS_EXTRA:-}"'"

OBJS=""
for f in '"$SRCS"'; do
    $CC $CFLAGS -Werror -c hook/audio/$f.c -o "$OUT/$f.o"
    OBJS="$OBJS $OUT/$f.o"
done
$CC $CFLAGS -Werror -c tools/stereo-upmix-render/bench/$BIN.c -o "$OUT/$BIN.o"
$CC "$OUT/$BIN.o" $OBJS -o "$OUT/$BIN" -lm

$READELF -h "$OUT/$BIN" | grep -E "Class:|Machine:"
echo "built $OUT/$BIN ($(stat -c %s "$OUT/$BIN") bytes)"
'
