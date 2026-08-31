#!/bin/bash
# Build the CarPlay LD_PRELOAD hook (libcarplay_hook.so) for QNX/ARMv7 in Docker.
#
# Replaces the old VM-SSH recipe (root@192.168.64.16 + stock ntoarmv7-gcc 4.4.2).
# Now uses the self-contained image `qnx65-armv7-toolchain` (Tools/qnx-65-sdp-docker,
# GCC 4.9.4).  No VM required.  The hook links only -lz -lsocket (both in the SDP),
# so no BSP import stubs are needed.
#
#   ./scripts/build_hook.sh                        # default (LOG=1)
#   LOG=0 ./scripts/build_hook.sh                  # logging disabled
#   LOG_RGD_PACKET_RAW=1 ./scripts/build_hook.sh   # raw RGD logging (needs LOG=1)
#
# NOTE: this is GCC 4.9.4, not the stock QNX 4.4.2.  The hook uses no __thread
# (verified) so the emutls trap does not apply; the build asserts emutls==0 below.
#
# THE APAC DECODER.  Since stage j the hook also links the APAC decoder port
# (apac_decoder/src/*.cpp) plus its C shim hook/audio/apac_bridge.cpp, so the
# type-103 buffered stream can be decoded rather than only captured.  That code
# is C++ and the hook is C, so the C++ objects are built separately and the .so
# is linked with GCC, not G++ - linking with the C++ driver would put
# libstdc++.so.6 in DT_NEEDED, and that library is not on the head unit's image
# in a configuration this code may rely on.  The four assertions at the end of
# the build are the same ones apac_decoder/tools/build_qnx.sh makes, applied to
# the object that actually ships:
#
#   emutls=0        __thread in an LD_PRELOADed .so is a hard crash on QNX 6.5
#   exceptions=0    no __cxa_throw / __gxx_personality reached the link
#   rtti=0          no _ZTI/_ZTS/_ZTV
#   stl=0           no operator new/delete, no std::, no __cxa_guard
#
# -D_NO_CPP_INLINES: QNX 6.5's <math.h> defines its own C++ overloads of
# sqrt/sin/cos/... in namespace std, which collide with GCC's <cmath>.
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

. "$(dirname "$0")/apac_flags.sh"

echo "=== CarPlay Hook Build (Docker $IMG) ==="

docker run --rm --platform=linux/amd64 -v "$PROJECT_DIR":/src "$IMG" bash -c '
  set -e
  export PATH=/opt/qnx650/host/linux/x86/usr/bin:$PATH
  export QNX_HOST=/opt/qnx650/host/linux/x86 QNX_TARGET=/opt/qnx650/target/qnx6
  CC=arm-unknown-nto-qnx6.5.0eabi-gcc
  CXX=arm-unknown-nto-qnx6.5.0eabi-g++
  NM=arm-unknown-nto-qnx6.5.0eabi-nm
  READELF=arm-unknown-nto-qnx6.5.0eabi-readelf
  OBJ=/tmp/hookobj
  rm -rf $OBJ; mkdir -p $OBJ

  # The APAC decoder port and its C shim.  The flag set comes from
  # scripts/apac_flags.sh, which apac_decoder/bench/build_qnx_bench.sh sources
  # too - a benchmark compiled with drifted flags reports headroom for a binary
  # nobody ships, and says nothing about it.
  CXXFLAGS="'"$APAC_CXXFLAGS_NOINC"' \
            -I/src/apac_decoder/include -I/src/hook/audio"
  for f in /src/apac_decoder/src/*.cpp /src/hook/audio/apac_bridge.cpp; do
      $CXX $CXXFLAGS -c "$f" -o "$OBJ/cxx_$(basename "$f" .cpp).o"
  done
  echo "  compiled $(ls $OBJ/cxx_*.o | wc -l | tr -d " ") APAC decoder objects"

  cd /src/hook
  SRCS="framework/logging.c framework/bus.c framework/iap2_protocol.c framework/hook_framework.c \
        routeguidance/rgd_tlv.c routeguidance/rgd_hook.c coverart/coverart_hook.c \
        altscreen/altscreen_av.c altscreen/altscreen_hook.c audio/spatial_downmix.c \
        audio/apap_crypto.c audio/audio_hook.c main.c"
  $CC -shared -fPIC -O2 -std=gnu99 -fdata-sections -ffunction-sections '"$CFLAGS_EXTRA"' \
      -I. -I/src/apac_decoder/include $SRCS $OBJ/cxx_*.o \
      -o /src/build/libcarplay_hook.so -Wl,--gc-sections -lz -lsocket -lm

  fail() { echo "REJECTED: $*"; exit 1; }
  SO=/src/build/libcarplay_hook.so

  # 1. emutls trap: the hook must never carry thread-local emutls (QNX 6.5 crash).
  n=$($NM $SO 2>/dev/null | grep -ci emutls || true)
  [ "$n" = "0" ] || fail "$n emutls symbols present"

  # 2/3. no exceptions and no RTTI reached the link.
  n=$($NM $SO 2>/dev/null | grep -cE "__cxa_throw|__cxa_allocate_exception|__gxx_personality|_ZTI|_ZTS|_ZTV" || true)
  [ "$n" = "0" ] || fail "$n exception/RTTI symbols"

  # 4. nothing from the C++ runtime or the STL, and no static-init guards.
  n=$($NM -D --undefined-only $SO 2>/dev/null | grep -cE "_Znw|_Znа|_Zna|_ZdlPv|_ZdaPv|__cxa_guard|_ZSt|_ZNSt" || true)
  [ "$n" = "0" ] || fail "$n C++ runtime/STL symbols"

  # 5. and therefore no libstdc++ in DT_NEEDED, which the unit must not require.
  n=$($READELF -d $SO | grep -c libstdc || true)
  [ "$n" = "0" ] || fail "the hook links libstdc++"

  # 6. ldqnx must run compiler bookkeeping only.  Module constructors used to
  # reach LOG_INFO -> pthread_create while the QNX loader lock was still held,
  # making cold-start success depend on scheduler timing.  A normal GCC shared
  # object has one 4-byte frame_dummy entry; anything larger is a regression.
  init_size=$($READELF -W -S $SO | awk "\$2 == \".init_array\" { print \$6 }")
  [ "$init_size" = "000004" ] || fail ".init_array size=$init_size (expected compiler-only 000004)"
  n=$($NM -an $SO | grep -cE "(rgd|altscreen|audio)_module_(init|fini)" || true)
  [ "$n" = "0" ] || fail "$n eager module constructor/destructor symbols"

  echo "  undefined:$($NM -D --undefined-only $SO | sed "s/^ *//" | awk "\$1 == \"U\" { printf \" %s\", \$2 }" | head -c 400)"
  echo "  built build/libcarplay_hook.so (emutls=0 exceptions=0 rtti=0 stl=0 libstdc++=0 init_array=compiler-only)"
'

echo ""
echo "Compiled: $OUT"
ls -lh "$OUT"
