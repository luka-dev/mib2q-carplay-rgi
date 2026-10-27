#!/bin/sh
set -eu

HERE=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
ROOT=$(CDPATH= cd -- "$HERE/../.." && pwd)
VARIANT=4.9
CODEGEN=scalar

while [ "$#" -gt 0 ]; do
    case "$1" in
        --variant) VARIANT=$2; shift 2 ;;
        --codegen) CODEGEN=$2; shift 2 ;;
        *) echo "unexpected argument: $1" >&2; exit 2 ;;
    esac
done

case "$CODEGEN" in
    scalar|neon-auto|neon-fast|neon-manual) ;;
    *) echo "--codegen must be scalar, neon-auto, neon-fast or neon-manual" >&2; exit 2 ;;
esac

IMAGE="qnx65-armv7-toolchain:$VARIANT"
docker image inspect "$IMAGE" >/dev/null 2>&1 || {
    echo "ERROR: $IMAGE not built" >&2
    exit 1
}

docker run --rm --platform=linux/amd64 -v "$ROOT":/src -w /src \
    -e CUPGAN_QNX_VARIANT="$VARIANT" -e CUPGAN_CODEGEN="$CODEGEN" \
    "$IMAGE" bash -c '
set -eu
export PATH=/opt/qnx650/host/linux/x86/usr/bin:$PATH
export QNX_HOST=/opt/qnx650/host/linux/x86
export QNX_TARGET=/opt/qnx650/target/qnx6
CC=arm-unknown-nto-qnx6.5.0eabi-gcc
OBJDUMP=arm-unknown-nto-qnx6.5.0eabi-objdump
READELF=arm-unknown-nto-qnx6.5.0eabi-readelf
OUT=build/cupgan/qnx-${CUPGAN_QNX_VARIANT}-${CUPGAN_CODEGEN}
mkdir -p "$OUT"
. /src/scripts/apac_flags.sh
CFLAGS="$APAC_CFLAGS_NOINC -D__QNX__ -DNDEBUG -Werror -pedantic"
if [ "$CUPGAN_CODEGEN" = neon-auto ]; then
    CFLAGS="$CFLAGS -O3 -ftree-vectorize -mfpu=neon -mfloat-abi=softfp"
fi
if [ "$CUPGAN_CODEGEN" = neon-fast ]; then
    CFLAGS="$CFLAGS -O3 -ftree-vectorize -ffast-math -mfpu=neon -mfloat-abi=softfp"
fi
if [ "$CUPGAN_CODEGEN" = neon-manual ]; then
    CFLAGS="$CFLAGS -O3 -ftree-vectorize -ffast-math -mfpu=neon -mfloat-abi=softfp -DCUPGAN_USE_NEON"
fi
$CC $CFLAGS tools/cupgan-bench/cupgan_bench.c \
    -o "$OUT/cupgan_bench" -lm
echo "built $OUT/cupgan_bench ($(stat -c %s "$OUT/cupgan_bench") bytes)"
$READELF -A "$OUT/cupgan_bench" | grep -E "Tag_CPU|Tag_VFP|Tag_Advanced_SIMD" || true
echo "floating-point/vector instruction counts:"
$OBJDUMP -d "$OUT/cupgan_bench" | \
    grep -Eo "vmla|vmls|vmul|vadd|vld[1-4]|vst[1-4]|fmac|fmul|fadd" | \
    sort | uniq -c || true
'
