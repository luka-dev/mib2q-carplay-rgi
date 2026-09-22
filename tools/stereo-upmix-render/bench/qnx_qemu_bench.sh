#!/usr/bin/env bash
# Execute the ARMv7/QNX binary under QEMU -icount.  Guest ClockCycles is first
# calibrated against two exact instruction loops; the program refuses to call
# the result an instruction count if that calibration does not agree.
set -euo pipefail

HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT="$(cd "$HERE/../../.." && pwd)"
VARIANT=4.9
SHIFT=0
BLOCKS=128
WARMUP=8
BACKEND=hssr-universal
ASYNC_ARG=""
PACING_ARG=""
SCENE_ARG=""
MODEL="${HSSR_MODEL:-$ROOT/build/hssr-selection-v11/hssr_model.bin}"

while [[ $# -gt 0 ]]; do
    case "$1" in
        --variant) VARIANT="${2:?missing variant}"; shift 2 ;;
        --shift) SHIFT="${2:?missing shift}"; shift 2 ;;
        --blocks) BLOCKS="${2:?missing block count}"; shift 2 ;;
        --warmup) WARMUP="${2:?missing warmup count}"; shift 2 ;;
        --backend) BACKEND="${2:?missing backend}"; shift 2 ;;
        --model) MODEL="${2:?missing model}"; shift 2 ;;
        --scene) SCENE_ARG="--scene"; shift ;;
        --async-controller) ASYNC_ARG="--async-controller"; shift ;;
        --realtime-pacing) PACING_ARG="--realtime-pacing"; shift ;;
        *) echo "usage: $0 [--variant 4.9|8.5] [--shift N] [--blocks N] [--warmup N] [--backend hssr-universal] [--model HSSR.bin] [--scene] [--async-controller] [--realtime-pacing]" >&2; exit 2 ;;
    esac
done

[[ "$BACKEND" == hssr-universal ]] || {
    echo "error: only hssr-universal is available" >&2
    exit 2
}
[[ -f "$MODEL" ]] || {
    echo "error: HSSR model not found: $MODEL" >&2
    exit 1
}

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
"$HERE/build_qnx.sh" --variant "$VARIANT"

IMAGE="qnx65-armv7-toolchain:$VARIANT"
BINARY="$ROOT/build/stereo-upmix-bench/qnx-$VARIANT/stereo_upmix_bench"
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

cp "$QEMU_ASSETS/startup-virt" "$QEMU_ASSETS/libstartup.a" \
   "$QEMU_ASSETS/devc-serdebug" "$WORK/"
cp "$BINARY" "$WORK/stereo_upmix_bench"
cp "$MODEL" "$WORK/hssr_model.bin"

cat > "$WORK/bench.build" <<EOF
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
    display_msg "@@UPMIX-BENCH-START@@"
    stereo_upmix_bench --qemu-icount --backend $BACKEND --model /proc/boot/hssr_model.bin --blocks $BLOCKS --warmup $WARMUP $SCENE_ARG $ASYNC_ARG $PACING_ARG
    display_msg "@@UPMIX-BENCH-END@@"
}
[type=link] /usr/lib/ldqnx.so.2=/proc/boot/libc.so.3
[perms=+r,+x]
libc.so.3
libm.so.2
procnto-smp
devc-serdebug
stereo_upmix_bench
hssr_model.bin
EOF

docker run --rm --platform=linux/amd64 -v "$WORK":/w -w /w \
    "$IMAGE" sh -c '
set -e
TARGET=/opt/qnx650/target/qnx6/armle-v7
cp "$TARGET/lib/libc.so.3" "$TARGET/lib/libm.so.2" \
   "$TARGET/boot/sys/procnto-smp" /w/
export MKIFS_PATH=/w:$TARGET/boot/sys:$TARGET/bin:$TARGET/lib:$TARGET/usr/lib
mkifs bench.build bench.ifs
' >"$WORK/mkifs.log" 2>&1 || {
    echo "mkifs failed:" >&2
    tail -12 "$WORK/mkifs.log" >&2
    exit 1
}

LOG="${QEMU_KEEP_LOG:-$WORK/qemu.log}"
echo "booting QNX/ARMv7 with -icount shift=$SHIFT (not host wall time)"
# BENCH_QEMU_PLUGIN=/path/to/libl1d.so[,size=...,ways=...,line=...] adds the
# data-cache model.  -icount alone counts instructions and models no cache.
PLUGIN_ARGS=()
[[ -n "${BENCH_QEMU_PLUGIN:-}" ]] && PLUGIN_ARGS=(-plugin "$BENCH_QEMU_PLUGIN" -d plugin)
timeout "${QEMU_TIMEOUT:-900}" qemu-system-arm \
    -M virt -m 512 -cpu cortex-a15 -accel tcg,thread=single \
    -icount "shift=$SHIFT" \
    "${PLUGIN_ARGS[@]}" \
    -device loader,file="$WORK/bench.ifs",addr=0x40200000,force-raw=on \
    -device loader,addr=0x40200000,cpu-num=0 \
    -display none -serial stdio -monitor none \
    </dev/null >"$LOG" 2>&1 &
QEMU_WRAPPER_PID=$!

# QNX deliberately keeps running after its image script reaches the end.  Stop
# this one explicit QEMU process as soon as the serial marker is durable rather
# than making every successful benchmark wait for the timeout.
while kill -0 "$QEMU_WRAPPER_PID" 2>/dev/null; do
    if grep -q '@@UPMIX-BENCH-END@@' "$LOG" 2>/dev/null; then
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

grep -q '@@UPMIX-BENCH-START@@' "$LOG" || {
    echo "error: QNX boot did not reach the benchmark" >&2
    tail -30 "$LOG" >&2
    exit 1
}
awk '
    /@@UPMIX-BENCH-START@@/ {
        sub(/.*@@UPMIX-BENCH-START@@/, ""); inside = 1
    }
    /@@UPMIX-BENCH-END@@/ {
        sub(/@@UPMIX-BENCH-END@@.*/, "");
        if (length($0)) print;
        complete = 1;
        exit
    }
    inside && length($0) { print }
    END { if (!complete) exit 1 }
' "$LOG" || {
    echo "error: benchmark did not reach its END marker" >&2
    tail -30 "$LOG" >&2
    exit 1
}
