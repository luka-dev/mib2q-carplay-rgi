#!/bin/bash
# Build the CarPlay LD_PRELOAD hook (libcarplay_hook.so) for QNX/ARMv7 in Docker.
#
# Uses the self-contained image `qnx65-armv7-toolchain` (Tools/qnx-65-sdp-docker,
# GCC 4.9.4).  The hook links only -lz -lsocket (both in the SDP), so no BSP
# import stubs are needed.
#
#   ./scripts/build_hook.sh                        # default (LOG=1)
#   LOG=0 ./scripts/build_hook.sh                  # logging disabled
#   LOG_RGD_PACKET_RAW=1 ./scripts/build_hook.sh   # raw RGD logging (needs LOG=1)
#
# NOTE: this is GCC 4.9.4, not the stock QNX 4.4.2.  The hook uses no __thread
# (verified) so the emutls trap does not apply; the build asserts emutls==0 below.
set -e

[ "$#" -eq 0 ] || { echo "usage: ./scripts/build_hook.sh"; exit 2; }

IMG=qnx65-armv7-toolchain:latest
SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
PROJECT_DIR="$(cd "$SCRIPT_DIR/.." && pwd)"
OUT="$PROJECT_DIR/build/libcarplay_hook.so"
mkdir -p "$(dirname "$OUT")"

LOG="${LOG:-1}"
LOG_RGD_PACKET_RAW="${LOG_RGD_PACKET_RAW:-0}"
[[ "$LOG" == "0" || "$LOG" == "1" ]] || { echo "Invalid LOG=$LOG (0|1)"; exit 1; }
[[ "$LOG_RGD_PACKET_RAW" == "0" || "$LOG_RGD_PACKET_RAW" == "1" ]] || { echo "Invalid LOG_RGD_PACKET_RAW"; exit 1; }
[[ "$LOG_RGD_PACKET_RAW" == "1" && "$LOG" != "1" ]] && { echo "LOG_RGD_PACKET_RAW=1 requires LOG=1"; exit 1; }

CFLAGS_EXTRA="-D__QNX__"
[ "$LOG" == "0" ] && CFLAGS_EXTRA="$CFLAGS_EXTRA -DENABLE_LOGGING=0" && echo "Logging DISABLED"
[ "$LOG_RGD_PACKET_RAW" == "1" ] && CFLAGS_EXTRA="$CFLAGS_EXTRA -DRGD_TRACE_RAW_FULL=1" && echo "RGD raw packet logging ENABLED"

if ! docker image inspect "$IMG" >/dev/null 2>&1; then
    echo "ERROR: docker image '$IMG' not found."
    echo "Build it once:  docker build --platform=linux/amd64 -t $IMG $PROJECT_DIR/../../Tools/qnx-65-sdp-docker"
    exit 1
fi

echo "=== CarPlay Hook Build (Docker $IMG) ==="

docker run --rm --platform=linux/amd64 -v "$PROJECT_DIR":/src "$IMG" bash -c '
  set -e
  export PATH=/opt/qnx650/host/linux/x86/usr/bin:$PATH
  export QNX_HOST=/opt/qnx650/host/linux/x86 QNX_TARGET=/opt/qnx650/target/qnx6
  CC=arm-unknown-nto-qnx6.5.0eabi-gcc
  NM=arm-unknown-nto-qnx6.5.0eabi-nm
  READELF=arm-unknown-nto-qnx6.5.0eabi-readelf
  cd /src/hook
  SRCS="framework/logging.c framework/bus.c framework/iap2_protocol.c framework/hook_framework.c \
        routeguidance/rgd_tlv.c routeguidance/rgd_hook.c coverart/coverart_hook.c \
        main.c"
  $CC -shared -fPIC -O2 -std=gnu99 -fdata-sections -ffunction-sections '"$CFLAGS_EXTRA"' \
      -I. $SRCS -o /src/build/libcarplay_hook.so -Wl,--gc-sections -lz -lsocket
  # emutls trap: the hook must never carry thread-local emutls (QNX 6.5 crash).
  n=$($NM /src/build/libcarplay_hook.so 2>/dev/null | grep -ci emutls || true)
  [ "$n" = "0" ] || { echo "REJECTED: $n emutls symbols present"; exit 1; }
  init_size=$($READELF -W -S /src/build/libcarplay_hook.so | awk "\$2 == \".init_array\" { print \$6 }")
  [ "$init_size" = "000004" ] || { echo "REJECTED: .init_array size=$init_size (expected compiler-only 000004)"; exit 1; }
  n=$($NM -an /src/build/libcarplay_hook.so | grep -cE "rgd_module_(init|fini)" || true)
  [ "$n" = "0" ] || { echo "REJECTED: $n eager RGD constructor/destructor symbols"; exit 1; }
  echo "  built build/libcarplay_hook.so (emutls=0 init_array=compiler-only)"
'

echo ""
echo "Compiled: $OUT"
ls -lh "$OUT"
