#!/bin/bash
# Local-only, separate videoencoderservice candidate. Does NOT install/enable it.
set -euo pipefail
[ "$#" -eq 1 ] || { echo "usage: $0 /path/to/stock/videoencoderservice" >&2; exit 2; }
PROJECT_DIR=$(cd "$(dirname "$0")/.." && pwd)
OUT="$PROJECT_DIR/build/capture-clock"
mkdir -p "$OUT"
python3 "$PROJECT_DIR/native/capture_clock/verify_firmware.py" "$1" > "$OUT/firmware.json"
docker run --rm --platform=linux/amd64 -v "$PROJECT_DIR":/src qnx65-armv7-toolchain:latest bash -c '
set -euo pipefail
export PATH=/opt/qnx650/host/linux/x86/usr/bin:$PATH
export QNX_HOST=/opt/qnx650/host/linux/x86 QNX_TARGET=/opt/qnx650/target/qnx6
CC=arm-unknown-nto-qnx6.5.0eabi-gcc
NM=arm-unknown-nto-qnx6.5.0eabi-nm
READELF=arm-unknown-nto-qnx6.5.0eabi-readelf
SO=/src/build/capture-clock/libvc_capture_clock.so
$CC -std=gnu99 -O2 -marm -Wall -Wextra -Werror -fPIC -fvisibility=hidden \
  -shared -Wl,--version-script=/src/native/capture_clock/exports.map -Wl,-soname,libvc_capture_clock.so /src/native/capture_clock/capture_clock.c /src/native/capture_clock/entry_arm.S -o "$SO"
$READELF -h "$SO" | grep -q "Machine:.*ARM"
! $NM "$SO" | grep -E "emutls|__cxa_throw|__gxx_personality"
$NM -D --defined-only "$SO" | awk '\''$2 == "T" {print $3}'\'' | sort > /src/build/capture-clock/exports.txt
printf "%s\n" _ZN4util10TimerGroup10startTimerEj timer_settime | sort > /tmp/expected-exports
cmp /tmp/expected-exports /src/build/capture-clock/exports.txt
$READELF -d "$SO" > /src/build/capture-clock/dynamic.txt
arm-unknown-nto-qnx6.5.0eabi-objdump -d "$SO" > /src/build/capture-clock/disassembly.txt
'
shasum -a 256 "$OUT/libvc_capture_clock.so"
