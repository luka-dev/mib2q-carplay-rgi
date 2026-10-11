#!/bin/bash
# Build the CarPlay LD_PRELOAD hook (libcarplay_hook.so) for QNX/ARMv7 in Docker.
#
# Uses the self-contained image `qnx65-armv7-toolchain`
# (https://github.com/luka-dev/qnx65-armv7-toolchain, GCC 8.5).  The hook links only -lz -lsocket (both in the SDP), so no BSP
# import stubs are needed.
#
#   ./scripts/build_hook.sh                        # the one production image
#   LOG_RGD_PACKET_RAW=1 ./scripts/build_hook.sh   # + raw RGD packet hex dumps
#
# NOTE: this is GCC 8.5, not the stock QNX 4.4.2.  The hook uses no __thread
# (verified) so the emutls trap does not apply; the build asserts emutls==0 below.
set -e

[ "$#" -eq 0 ] || { echo "usage: ./scripts/build_hook.sh"; exit 2; }

IMG=${QNX_TOOLCHAIN_IMAGE:-qnx65-armv7-toolchain:latest}
SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
PROJECT_DIR="$(cd "$SCRIPT_DIR/.." && pwd)"
case "$(uname -s)" in
    MINGW*|MSYS*|CYGWIN*) PROJECT_DIR="$(cygpath -m "$PROJECT_DIR")"; export MSYS_NO_PATHCONV=1 ;;
esac
OUT="$PROJECT_DIR/build/libcarplay_hook.so"
mkdir -p "$(dirname "$OUT")"

# One production image: logging compiled in, WARN only, INFO behind carplay_verbose.
# The old LOG=0 variant is gone.
[ "${LOG:-}" = 0 ] && { echo "LOG=0 is gone: the hook always logs WARN; touch carplay_verbose for more"; exit 1; }
LOG_RGD_PACKET_RAW="${LOG_RGD_PACKET_RAW:-0}"
[[ "$LOG_RGD_PACKET_RAW" == "0" || "$LOG_RGD_PACKET_RAW" == "1" ]] || { echo "Invalid LOG_RGD_PACKET_RAW"; exit 1; }

CFLAGS_EXTRA="-D__QNX__"
[ "$LOG_RGD_PACKET_RAW" == "1" ] && CFLAGS_EXTRA="$CFLAGS_EXTRA -DRGD_TRACE_RAW_FULL=1" && echo "RGD raw packet logging ENABLED"

if ! docker image inspect "$IMG" >/dev/null 2>&1; then
    echo "ERROR: docker image '$IMG' not found."
    echo "Build it once from https://github.com/luka-dev/qnx65-armv7-toolchain :  ./host-scripts/qnx-run.sh build"
    exit 1
fi

echo "=== CarPlay Hook Build (Docker $IMG) ==="

docker run --rm --platform=linux/amd64 -v "$PROJECT_DIR":/host "$IMG" bash -c '
  set -e
  export PATH=/opt/qnx650/host/linux/x86/usr/bin:$PATH
  export QNX_HOST=/opt/qnx650/host/linux/x86 QNX_TARGET=/opt/qnx650/target/qnx6
  CC=arm-unknown-nto-qnx6.5.0eabi-gcc
  NM=arm-unknown-nto-qnx6.5.0eabi-nm
  READELF=arm-unknown-nto-qnx6.5.0eabi-readelf
  # The 32-bit QNX binutils cannot read Docker Desktop bind-mount inode numbers.
  mkdir -p /src/build
  cp -a /host/hook /src/
  cd /src/hook
  SRCS="framework/logging.c framework/state_trace.c framework/signal_guard.c framework/bus.c \
        framework/iap2_protocol.c framework/hook_framework.c \
        routeguidance/rgd_tlv.c routeguidance/rgd_hook.c coverart/jpeg_safety.c coverart/coverart_stream.c coverart/coverart_hook.c \
        main.c"
  $CC -shared -fPIC -O2 -std=gnu99 -fvisibility=hidden -fdata-sections -ffunction-sections '"$CFLAGS_EXTRA"' \
      -I. $SRCS -o /src/build/libcarplay_hook.so -Wl,--gc-sections \
      -Wl,--version-script=/src/hook/carplay_hook.exports.map -lz -lsocket
  # The LD_PRELOAD ABI is an exact allowlist: hidden-by-default compilation plus
  # the version script, checked against what actually landed in .dynsym.
  awk "/global:/{g=1;next} /local:/{g=0} g{gsub(/[;[:space:]]/,\"\"); if(length) print}" \
      /src/hook/carplay_hook.exports.map | sort > /tmp/exports_expected
  $NM -D --defined-only /src/build/libcarplay_hook.so | awk "NF >= 3 { print \$3 }" | sort > /tmp/exports_actual
  diff -u /tmp/exports_expected /tmp/exports_actual || { echo "REJECTED: dynamic exports differ from the allowlist"; exit 1; }
  # emutls trap: the hook must never carry thread-local emutls (QNX 6.5 crash).
  n=$($NM /src/build/libcarplay_hook.so 2>/dev/null | grep -ci emutls || true)
  [ "$n" = "0" ] || { echo "REJECTED: $n emutls symbols present"; exit 1; }
  init_size=$($READELF -W -S /src/build/libcarplay_hook.so | awk "\$2 == \".init_array\" { print \$6 }")
  [ "$init_size" = "000004" ] || { echo "REJECTED: .init_array size=$init_size (expected compiler-only 000004)"; exit 1; }
  n=$($NM -an /src/build/libcarplay_hook.so | grep -cE "rgd_module_(init|fini)" || true)
  [ "$n" = "0" ] || { echo "REJECTED: $n eager RGD constructor/destructor symbols"; exit 1; }
  mkdir -p /host/build
  cp /src/build/libcarplay_hook.so /host/build/
  echo "  built build/libcarplay_hook.so (emutls=0 init_array=compiler-only)"
'

echo ""
echo "Compiled: $OUT"
ls -lh "$OUT"
