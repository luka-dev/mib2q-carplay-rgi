#!/usr/bin/env bash
# scene_engine_bench (or --bench mask: scene_mask_bench with its weight blob) under QEMU -icount on real
# QNX 6.5/ARMv7, the qnx_qemu_bench.sh flow without the HSSR model files: guest ClockCycles is calibrated against two exact instruction loops and the program
# refuses to call the result an instruction count if that calibration does not agree.
# BENCH_QEMU_PLUGIN=/path/libl1d.so[,size=..,ways=..,line=..] adds the L1D model (metric=l1d_cache in
# QEMU_KEEP_LOG); -icount alone counts instructions and models no cache.
set -euo pipefail

HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT="$(cd "$HERE/../../.." && pwd)"
VARIANT=latest
SHIFT=0
SECONDS_ARG=60
MASK_ARG=""
HANDOFF_ARG=""
NOPUSH_ARG=""
BENCH=engine
WEIGHTS=""

while [[ $# -gt 0 ]]; do
    case "$1" in
        --variant) VARIANT="${2:?missing variant}"; shift 2 ;;
        --shift) SHIFT="${2:?missing shift}"; shift 2 ;;
        --seconds) SECONDS_ARG="${2:?missing seconds}"; shift 2 ;;
        --mask) MASK_ARG="--mask"; shift ;;
        --handoff) HANDOFF_ARG="--handoff"; shift ;;
        --no-push) NOPUSH_ARG="--no-push"; shift ;;
        --bench) BENCH="${2:?missing bench}"; shift 2 ;;
        --weights) WEIGHTS="${2:?missing weights}"; shift 2 ;;
        *) echo "usage: $0 [--variant latest|4.9|8.5] [--shift N] [--seconds 1..65] [--mask [--handoff]] [--no-push] [--bench engine|mask --weights SMSKNET1.bin]" >&2; exit 2 ;;
    esac
done
case "$BENCH" in
    engine) BIN=scene_engine_bench; OUTDIR=scene-engine-bench; RUN="scene_engine_bench --qemu-icount --seconds $SECONDS_ARG $MASK_ARG $HANDOFF_ARG $NOPUSH_ARG" ;;
    mask)
        [[ -f "$WEIGHTS" ]] || { echo "error: --bench mask needs --weights SMSKNET1.bin" >&2; exit 2; }
        BIN=scene_mask_bench; OUTDIR=scene-mask-bench; RUN="scene_mask_bench --qemu-icount --seconds $SECONDS_ARG --weights /proc/boot/masknet.bin" ;;
    *) echo "error: unknown bench $BENCH (engine|mask)" >&2; exit 2 ;;
esac

QEMU_ASSETS=""
SEARCH="$HERE"
while [[ "$SEARCH" != / ]]; do
    if [[ -d "$SEARCH/Tools/qnx-65-sdp-docker/tests/qemu" ]]; then
        QEMU_ASSETS="$SEARCH/Tools/qnx-65-sdp-docker/tests/qemu"
        break
    fi
    SEARCH="$(dirname "$SEARCH")"
done
[[ -n "$QEMU_ASSETS" ]] || {
    echo "error: cannot find Tools/qnx-65-sdp-docker/tests/qemu" >&2
    exit 1
}
command -v qemu-system-arm >/dev/null || {
    echo "error: qemu-system-arm is not installed" >&2
    exit 1
}
"$HERE/scene_build_qnx.sh" --variant "$VARIANT" --bench "$BENCH"

IMAGE="qnx65-armv7-toolchain:$VARIANT"
BINARY="$ROOT/build/$OUTDIR/qnx-$VARIANT/$BIN"
WORK="$(mktemp -d)"
QEMU_WRAPPER_PID=""
cleanup() {
    if [[ -n "$QEMU_WRAPPER_PID" ]]; then
        kill "$QEMU_WRAPPER_PID" 2>/dev/null || true
        wait "$QEMU_WRAPPER_PID" 2>/dev/null || true
    fi
    rm -rf "$WORK"
}
trap cleanup EXIT
trap 'exit 130' INT
trap 'exit 143' TERM

cp "$QEMU_ASSETS/startup-virt" "$QEMU_ASSETS/libstartup.a" "$QEMU_ASSETS/devc-serdebug" "$WORK/"
cp "$BINARY" "$WORK/$BIN"
WEIGHTS_LINE=""
if [[ -n "$WEIGHTS" ]]; then
    cp "$WEIGHTS" "$WORK/masknet.bin"
    WEIGHTS_LINE="masknet.bin"
fi

cat > "$WORK/bench.build" <<EOT
[linker="(40;1;)qcc -bootstrap -nostdlib -Wl,--no-keep-memory -Vgcc_ntoarmv7%(m!=3,%(m!=6,%(e==0, -EL%)%(e==1, -EB%)%)%)%(h!=0, -Wl,-Ttext -Wl,0x%t%)%(d!=0, -Wl,-Tdata -Wl,0x%d%) -o%o %i %[M -L%^i -Wl,-uinit_%n -lmod_%n%] /w/libstartup.a -L/opt/qnx650/target/qnx6/armle-v7/lib -L/opt/qnx650/target/qnx6/armle-v7/usr/lib -llzo -lucl -ldrvr"]
[linker="(40;2;)cp %i %o"]
[image=0x40200000]
[virtual=armle-v7,raw] .bootstrap = {
    startup-virt -v -S
    PATH=:/proc/boot LD_LIBRARY_PATH=:/proc/boot procnto-smp -vvv
}
[+script] .script = {
    procmgr_symlink ../../proc/boot/libc.so.3 /usr/lib/ldqnx.so.2
    devc-serdebug -e -F -S
    waitfor /dev/ser1 4
    reopen /dev/ser1
    display_msg "@@SCENE-BENCH-START@@"
    $RUN
    display_msg "@@SCENE-BENCH-END@@"
}
[type=link] /usr/lib/ldqnx.so.2=/proc/boot/libc.so.3
[perms=+r,+x]
libc.so.3
libm.so.2
procnto-smp
devc-serdebug
$BIN
$WEIGHTS_LINE
EOT

docker run --rm --platform=linux/amd64 -v "$WORK":/w -w /w "$IMAGE" sh -c '
set -e
TARGET=/opt/qnx650/target/qnx6/armle-v7
cp "$TARGET/lib/libc.so.3" "$TARGET/lib/libm.so.2" "$TARGET/boot/sys/procnto-smp" /w/
export MKIFS_PATH=/w:$TARGET/boot/sys:$TARGET/bin:$TARGET/lib:$TARGET/usr/lib
mkifs bench.build bench.ifs
' >"$WORK/mkifs.log" 2>&1 || {
    echo "mkifs failed:" >&2
    tail -12 "$WORK/mkifs.log" >&2
    exit 1
}

LOG="${QEMU_KEEP_LOG:-$WORK/qemu.log}"
echo "booting QNX/ARMv7 with -icount shift=$SHIFT (not host wall time)"
PLUGIN_ARGS=()
[[ -n "${BENCH_QEMU_PLUGIN:-}" ]] && PLUGIN_ARGS=(-plugin "$BENCH_QEMU_PLUGIN" -d plugin)
timeout "${QEMU_TIMEOUT:-3600}" qemu-system-arm \
    -M virt -m 512 -cpu cortex-a15 -accel tcg,thread=single \
    -icount "shift=$SHIFT" \
    "${PLUGIN_ARGS[@]}" \
    -device loader,file="$WORK/bench.ifs",addr=0x40200000,force-raw=on \
    -device loader,addr=0x40200000,cpu-num=0 \
    -display none -serial stdio -monitor none \
    </dev/null >"$LOG" 2>&1 &
QEMU_WRAPPER_PID=$!

# QNX keeps running after its image script ends: stop QEMU once the END marker is in the log.
while kill -0 "$QEMU_WRAPPER_PID" 2>/dev/null; do
    if grep -q '@@SCENE-BENCH-END@@' "$LOG" 2>/dev/null; then
        kill "$QEMU_WRAPPER_PID" 2>/dev/null || true
        wait "$QEMU_WRAPPER_PID" 2>/dev/null || true
        QEMU_WRAPPER_PID=""
        break
    fi
    sleep 1
done
if [[ -n "$QEMU_WRAPPER_PID" ]]; then
    wait "$QEMU_WRAPPER_PID" 2>/dev/null || true
    QEMU_WRAPPER_PID=""
fi

grep -q '@@SCENE-BENCH-START@@' "$LOG" || {
    echo "error: QNX boot did not reach the benchmark" >&2
    tail -30 "$LOG" >&2
    exit 1
}
awk '
    /@@SCENE-BENCH-START@@/ { sub(/.*@@SCENE-BENCH-START@@/, ""); inside = 1 }
    /@@SCENE-BENCH-END@@/ { sub(/@@SCENE-BENCH-END@@.*/, ""); if (length($0)) print; complete = 1; exit }
    inside && length($0) { print }
    END { if (!complete) exit 1 }
' "$LOG" || {
    echo "error: benchmark did not reach its END marker" >&2
    tail -30 "$LOG" >&2
    exit 1
}
