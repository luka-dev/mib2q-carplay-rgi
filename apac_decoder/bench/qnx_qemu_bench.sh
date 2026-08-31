#!/usr/bin/env bash
# Run apac_bench on real QNX 6.5 / armle-v7 under QEMU, with -icount, so that
# what the guest measures is an INSTRUCTION COUNT and not a wall clock.
#
#   apac_decoder/bench/qnx_qemu_bench.sh [--variant 4.9|8.5] [--shift N]
#                                        [--blob FILE] [-- ARGS...]
#
# WHY -icount.  QEMU is not a model of a Krait.  Its TCG has no pipeline, no
# cache and no branch predictor, so the host time an emulated program takes is a
# fact about this Mac and the translator, not about the head unit.  What it does
# have is exact instruction retirement, and -icount shift=N ties the guest's
# view of time to it: each instruction advances the virtual clock by 2^N ns, and
# every timer the guest reads is derived from that clock.  So a guest-side
# measurement is a count of the instructions the target's own binary executed,
# which is a real property of the port and of the ARMv7 code GCC generated for
# it.  Turning that into seconds still needs an IPC assumption, and PORT_STATUS
# states the one it uses rather than hiding it in a headline number.
#
# apac_bench does not take this on trust: it times two loops of known length and
# one that is memory bound, and only reports instructions if all three agree on
# the cost of an instruction.  Without -icount the memory-bound loop is far
# dearer per instruction than the arithmetic ones and the conversion is refused.
#
# This is tools/qnx_qemu_run.sh with the icount knob and a longer leash; the
# boot pieces are still Tools/qnx-65-sdp-docker's.
set -euo pipefail

HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
APAC="$(cd "$HERE/.." && pwd)"
ROOT="$(cd "$APAC/.." && pwd)"
Q="$ROOT/../../Tools/qnx-65-sdp-docker/tests/qemu"
[ -d "$Q" ] || { D="$HERE"; while [ "$D" != "/" ]; do
        [ -d "$D/Tools/qnx-65-sdp-docker/tests/qemu" ] && { Q="$D/Tools/qnx-65-sdp-docker/tests/qemu"; break; }
        D="$(dirname "$D")"; done; }
[ -d "$Q" ] || { echo "cannot find Tools/qnx-65-sdp-docker/tests/qemu" >&2; exit 1; }

VAR=4.9
SHIFT=0
BLOB=/tmp/apac_bench.blob
ARGS="--reps 1"
while [ $# -gt 0 ]; do
    case "$1" in
        --variant) VAR="$2"; shift ;;
        --shift)   SHIFT="$2"; shift ;;
        --blob)    BLOB="$2"; shift ;;
        --)        shift; ARGS="$*"; break ;;
        *) echo "unexpected argument: $1" >&2; exit 2 ;;
    esac
    shift
done

BIN="$APAC/.build-bench/qnx-$VAR/apac_bench"
[ -f "$BIN" ] || { echo "build it first: apac_decoder/bench/build_qnx_bench.sh --variant $VAR" >&2; exit 1; }
[ -f "$BLOB" ] || { echo "make it first: apac_decoder/bench/make_blob.py" >&2; exit 1; }
IMG="qnx65-armv7-toolchain:$VAR"
command -v qemu-system-arm >/dev/null || { echo "qemu-system-arm not installed" >&2; exit 1; }

WORK=$(mktemp -d); trap 'rm -rf "$WORK"' EXIT
cp "$Q/startup-virt" "$Q/libstartup.a" "$Q/devc-serdebug" "$WORK/"
cp "$BIN" "$WORK/apac_bench"
cp "$BLOB" "$WORK/apac_bench.blob"

cat > "$WORK/test.build" <<EOF
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
    display_msg "@@QNXRUN-START@@"
    apac_bench --blob /proc/boot/apac_bench.blob $ARGS
    display_msg "@@QNXRUN-END@@"
}
[type=link] /usr/lib/ldqnx.so.2=/proc/boot/libc.so.3
[perms=+r,+x]
libc.so.3
libm.so.2
procnto-smp
devc-serdebug
apac_bench
apac_bench.blob
EOF

docker run --rm --platform=linux/amd64 -v "$WORK":/w -w /w "$IMG" sh -c '
T=/opt/qnx650/target/qnx6/armle-v7
cp $T/lib/libc.so.3 $T/lib/libm.so.2 $T/boot/sys/procnto-smp /w/ 2>/dev/null || true
export MKIFS_PATH=/w:$T/boot/sys:$T/bin:$T/lib:$T/usr/lib
mkifs test.build ifs.bin' >"$WORK/mkifs.log" 2>&1 || {
    echo "mkifs failed:" >&2; tail -8 "$WORK/mkifs.log" >&2; exit 1; }
[ -f "$WORK/ifs.bin" ] || { echo "mkifs produced no image:" >&2; tail -8 "$WORK/mkifs.log" >&2; exit 1; }

LOG="${QEMU_KEEP_LOG:-$WORK/qemu.log}"
echo "booting QNX 6.5 armle-v7 under QEMU, -icount shift=$SHIFT (log: $LOG)"
# No u-boot.  Tools/qnx-65-sdp-docker's runner boots through u-boot and drives
# it by typing at fixed real-time delays; under -icount everything takes about
# ten times the real time it used to and those delays stop lining up.  QEMU's
# generic loader does the two things u-boot was there for - put the image at
# 0x40200000 and start the CPU there - and does them deterministically.
timeout "${QEMU_TIMEOUT:-7200}" qemu-system-arm -M virt -m 512 -cpu cortex-a15 \
    -accel tcg,thread=single -icount "shift=$SHIFT" \
    -device loader,file="$WORK/ifs.bin",addr=0x40200000,force-raw=on \
    -device loader,addr=0x40200000,cpu-num=0 \
    -display none -serial stdio < /dev/null > "$LOG" 2>&1 || true

grep -q '@@QNXRUN-START@@' "$LOG" || { echo "=== boot did not reach the program ===" >&2; tail -20 "$LOG" >&2; exit 1; }
awk '
  /@@QNXRUN-START@@/ { sub(/.*@@QNXRUN-START@@/, ""); inside = 1 }
  /@@QNXRUN-END@@/   { sub(/@@QNXRUN-END@@.*/, ""); if (length($0)) print; exit }
  inside && length($0) { print }
' "$LOG"
grep -q '@@QNXRUN-END@@' "$LOG" || { echo "(did not reach the end - it hung, or the run was cut short)" >&2; exit 1; }
