#!/usr/bin/env bash
# Run armle-v7 programs on real QNX 6.5 under QEMU, with data files placed in
# the IFS beside them and an optional libm override.
#
#   tools/qnx_qemu_run.sh [--variant 4.9|8.5] [--libm FILE]
#                         [--file FILE]... [--run "prog args"]...
#
# Tools/qnx-65-sdp-docker/tests/qemu-run.sh does the same for a single bare
# binary.  The APAC tests need their fixture in the image, want several programs
# in one boot (a boot costs about a minute), and the pow question needs the head
# unit's own libm.so.2 rather than the SDP's - so this is that script with those
# three knobs.  The boot pieces (startup-virt, libstartup.a, devc-serdebug,
# u-boot.bin) are still the toolchain repo's; only the driver is duplicated.
#
# Everything given with --file lands in /proc/boot, which is also the cwd, so a
# program reads its fixture as a plain basename.
set -euo pipefail

HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
Q=""
D="$HERE"
while [ "$D" != "/" ]; do
    [ -d "$D/Tools/qnx-65-sdp-docker/tests/qemu" ] && { Q="$D/Tools/qnx-65-sdp-docker/tests/qemu"; break; }
    D="$(dirname "$D")"
done
[ -n "$Q" ] || { echo "cannot find Tools/qnx-65-sdp-docker/tests/qemu above $HERE" >&2; exit 1; }

VAR=4.9
LIBM=""
FILES=()
RUNS=()
while [ $# -gt 0 ]; do
    case "$1" in
        --variant) VAR="$2"; shift ;;
        --libm)    LIBM="$2"; shift ;;
        --file)    FILES+=("$2"); shift ;;
        --run)     RUNS+=("$2"); shift ;;
        *) echo "unexpected argument: $1" >&2; exit 2 ;;
    esac
    shift
done
[ "${#RUNS[@]}" -gt 0 ] || { echo "nothing to --run" >&2; exit 2; }

IMG="qnx65-armv7-toolchain:$VAR"
command -v qemu-system-arm >/dev/null || { echo "qemu-system-arm not installed" >&2; exit 1; }
docker image inspect "$IMG" >/dev/null 2>&1 || { echo "$IMG not built" >&2; exit 1; }

WORK=$(mktemp -d); trap 'rm -rf "$WORK"' EXIT
cp "$Q/startup-virt" "$Q/libstartup.a" "$Q/devc-serdebug" "$WORK/"
NAMES=""
for f in ${FILES+"${FILES[@]}"}; do cp "$f" "$WORK/"; NAMES="$NAMES
$(basename "$f")"; done
[ -n "$LIBM" ] && { cp "$LIBM" "$WORK/libm.so.2"; touch "$WORK/OWN_LIBM"; }

SCRIPT=""
for r in "${RUNS[@]}"; do
    SCRIPT="$SCRIPT
    display_msg \"@@RUN $r\"
    $r"
done

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
    display_msg "@@QNXRUN-START@@"$SCRIPT
    display_msg "@@QNXRUN-END@@"
}
[type=link] /usr/lib/ldqnx.so.2=/proc/boot/libc.so.3
[perms=+r,+x]
libc.so.3
libm.so.2
procnto-smp
devc-serdebug$NAMES
EOF

docker run --rm --platform=linux/amd64 -v "$WORK":/w -w /w "$IMG" sh -c '
T=/opt/qnx650/target/qnx6/armle-v7
cp $T/lib/libc.so.3 $T/boot/sys/procnto-smp /w/ 2>/dev/null || true
if [ ! -f /w/OWN_LIBM ]; then cp $T/lib/libm.so.2 /w/ 2>/dev/null || true; fi
export MKIFS_PATH=/w:$T/boot/sys:$T/bin:$T/lib:$T/usr/lib
mkifs test.build ifs.bin' >"$WORK/mkifs.log" 2>&1 || {
    echo "mkifs failed:" >&2; tail -8 "$WORK/mkifs.log" >&2; exit 1; }
[ -f "$WORK/ifs.bin" ] || { echo "mkifs produced no image:" >&2; tail -8 "$WORK/mkifs.log" >&2; exit 1; }

mkdir -p "$WORK/fat"; mv "$WORK/ifs.bin" "$WORK/fat/"
LOG="$WORK/qemu.log"
( printf '\n'; sleep 4
  printf 'virtio scan\n'; sleep 2
  printf 'fatload virtio 0:1 0x40200000 ifs.bin\n'; sleep "${QEMU_LOAD_SECS:-10}"
  printf 'go 0x40200000\n'; sleep "${QEMU_RUN_SECS:-90}" ) | \
  timeout "${QEMU_TIMEOUT:-400}" qemu-system-arm -M virt -m 512 -cpu cortex-a15 \
    -bios "$Q/u-boot.bin" -drive file=fat:rw:"$WORK/fat",format=raw,media=disk \
    -display none -serial stdio > "$LOG" 2>&1 || true

grep -q '@@QNXRUN-START@@' "$LOG" || { echo "=== boot did not reach the program ===" >&2; tail -20 "$LOG" >&2; exit 1; }
awk '
  /@@QNXRUN-START@@/ { sub(/.*@@QNXRUN-START@@/, ""); inside = 1 }
  /@@QNXRUN-END@@/   { sub(/@@QNXRUN-END@@.*/, ""); if (length($0)) print; exit }
  inside && length($0) { print }
' "$LOG"
[ -n "${QEMU_KEEP_LOG:-}" ] && cp "$LOG" "$QEMU_KEEP_LOG"
grep -q '@@QNXRUN-END@@' "$LOG" || { echo "did not reach the end - a program hung or took the system down" >&2; exit 1; }
rc=0
if grep -qE 'exited status=[1-9]' "$LOG"; then
    grep -oE "Process [0-9]+ \([^)]*\) exited status=[0-9]+" "$LOG" | grep -v 'status=0$' >&2; rc=1; fi
if grep -qE 'terminated SIG' "$LOG"; then
    grep -oE 'Process [0-9]+ \([^)]*\) terminated SIG[A-Z]+[^ ]*( [a-z]+=[0-9a-fx]+)*' "$LOG" >&2; rc=1; fi
exit $rc
