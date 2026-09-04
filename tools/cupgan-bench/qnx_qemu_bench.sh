#!/bin/sh
set -eu

HERE=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
ROOT=$(CDPATH= cd -- "$HERE/../.." && pwd)
QEMU_ASSETS="$ROOT/../../Tools/qnx-65-sdp-docker/tests/qemu"
VARIANT=4.9
CODEGEN=scalar
SHIFT=0
BENCH_ARGS="--frames 1 --warmup 2"

if [ ! -d "$QEMU_ASSETS" ]; then
    SEARCH=$HERE
    while [ "$SEARCH" != / ]; do
        if [ -d "$SEARCH/Tools/qnx-65-sdp-docker/tests/qemu" ]; then
            QEMU_ASSETS="$SEARCH/Tools/qnx-65-sdp-docker/tests/qemu"
            break
        fi
        SEARCH=$(dirname "$SEARCH")
    done
fi
[ -d "$QEMU_ASSETS" ] || {
    echo "cannot find Tools/qnx-65-sdp-docker/tests/qemu" >&2
    exit 1
}

while [ "$#" -gt 0 ]; do
    case "$1" in
        --variant) VARIANT=$2; shift 2 ;;
        --codegen) CODEGEN=$2; shift 2 ;;
        --shift) SHIFT=$2; shift 2 ;;
        --) shift; BENCH_ARGS="$*"; break ;;
        *) echo "unexpected argument: $1" >&2; exit 2 ;;
    esac
done

BINARY="$ROOT/build/cupgan/qnx-$VARIANT-$CODEGEN/cupgan_bench"
[ -f "$BINARY" ] || {
    echo "build it first: tools/cupgan-bench/build_qnx.sh --variant $VARIANT --codegen $CODEGEN" >&2
    exit 1
}
IMAGE="qnx65-armv7-toolchain:$VARIANT"
command -v qemu-system-arm >/dev/null || {
    echo "qemu-system-arm not installed" >&2
    exit 1
}

WORK=$(mktemp -d)
trap 'rm -rf "$WORK"' EXIT HUP INT TERM
cp "$QEMU_ASSETS/startup-virt" "$QEMU_ASSETS/libstartup.a" \
   "$QEMU_ASSETS/devc-serdebug" "$WORK/"
cp "$BINARY" "$WORK/cupgan_bench"

sed "s|@BENCH_ARGS@|$BENCH_ARGS|g" \
    "$HERE/qnx_test.build.in" > "$WORK/test.build"

docker run --rm --platform=linux/amd64 -v "$WORK":/w -w /w "$IMAGE" sh -c '
T=/opt/qnx650/target/qnx6/armle-v7
cp $T/lib/libc.so.3 $T/lib/libm.so.2 $T/boot/sys/procnto-smp /w/
export MKIFS_PATH=/w:$T/boot/sys:$T/bin:$T/lib:$T/usr/lib
mkifs test.build ifs.bin
' > "$WORK/mkifs.log" 2>&1 || {
    echo "mkifs failed:" >&2
    tail -8 "$WORK/mkifs.log" >&2
    exit 1
}

LOG=${QEMU_KEEP_LOG:-$WORK/qemu.log}
echo "booting QNX 6.5 armle-v7, codegen=$CODEGEN, -icount shift=$SHIFT"
qemu-system-arm \
    -M virt -m 512 -cpu cortex-a15 -accel tcg,thread=single \
    -icount "shift=$SHIFT" \
    -device loader,file="$WORK/ifs.bin",addr=0x40200000,force-raw=on \
    -device loader,addr=0x40200000,cpu-num=0 \
    -display none -serial stdio < /dev/null > "$LOG" 2>&1 &
QEMU_PID=$!
ELAPSED=0
LIMIT=${QEMU_TIMEOUT:-1800}
while kill -0 "$QEMU_PID" 2>/dev/null; do
    grep -q '@@CUPGAN-END@@' "$LOG" 2>/dev/null && break
    if [ "$ELAPSED" -ge "$LIMIT" ]; then
        echo "QEMU timeout after $LIMIT seconds" >&2
        break
    fi
    sleep 1
    ELAPSED=$((ELAPSED + 1))
done
kill -TERM "$QEMU_PID" 2>/dev/null || true
wait "$QEMU_PID" 2>/dev/null || true

grep -q '@@CUPGAN-START@@' "$LOG" || {
    echo "QNX boot did not reach cupgan_bench" >&2
    tail -30 "$LOG" >&2
    exit 1
}
awk '
    /@@CUPGAN-START@@/ { sub(/.*@@CUPGAN-START@@/, ""); inside = 1 }
    /@@CUPGAN-END@@/   { sub(/@@CUPGAN-END@@.*/, ""); if (length($0)) print; exit }
    inside && length($0) { print }
' "$LOG"
grep -q '@@CUPGAN-END@@' "$LOG" || {
    echo "cupgan_bench did not reach the end" >&2
    exit 1
}
