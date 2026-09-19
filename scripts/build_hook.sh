#!/bin/bash
# Build the CarPlay LD_PRELOAD hook (libcarplay_hook.so) for QNX/ARMv7 in Docker.
#
# Replaces the old VM-SSH recipe (root@192.168.64.16 + stock ntoarmv7-gcc 4.4.2).
# Now uses a self-contained `qnx65-armv7-toolchain` image
# (Tools/qnx-65-sdp-docker).  `latest` retains the existing default; the
# independently guarded GCC 8.5 build is selected with
# QNX_TOOLCHAIN_VARIANT=8.5.  No VM required.  The hook links only SDP system
# libraries, so no BSP import stubs are needed.
#
#   ./scripts/build_hook.sh                        # default image (LOG=1)
#   QNX_TOOLCHAIN_VARIANT=8.5 ./scripts/build_hook.sh
#   LOG=0 ./scripts/build_hook.sh                  # pre-prod: logs/captures disabled
#   LOG=0 AUDIO_STATE_TRACE=1 ./scripts/build_hook.sh # compact lifecycle trace only
#   LOG=0 ALTSCREEN_PACE_TRACE=1 ./scripts/build_hook.sh # stream-111 timing only
#   LOG_RGD_PACKET_RAW=1 ./scripts/build_hook.sh   # raw RGD logging (needs LOG=1)
#
# NOTE: these are not the stock QNX 4.4.2 compiler.  The hook uses no __thread
# (verified) so the emutls trap does not apply; every variant still runs the
# same emutls/runtime/constructor/link guards below.
#
# THE APAC DECODER.  It ships as optional build/libcarplay_apac.so and is
# dlopen'd only for an enabled buffered Spatial stream.  The main LD_PRELOAD
# hook therefore has no DT_NEEDED edge to the codec: a missing or rejected codec
# disables type-103 decode fail-closed without breaking ordinary CarPlay.
# The decoder is C++ and its versioned seam is C, so objects are built
# separately and the .so is linked with GCC, not G++ - the C++ driver would put
# libstdc++.so.6 in DT_NEEDED, and that library is not on the head unit's image
# in a configuration this code may rely on.  The safety assertions at the end of
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

QNX_TOOLCHAIN_VARIANT="${QNX_TOOLCHAIN_VARIANT:-latest}"
case "$QNX_TOOLCHAIN_VARIANT" in
  latest|4.9|8.5) ;;
  *) echo "Invalid QNX_TOOLCHAIN_VARIANT=$QNX_TOOLCHAIN_VARIANT (latest|4.9|8.5)"; exit 1 ;;
esac
IMG="qnx65-armv7-toolchain:$QNX_TOOLCHAIN_VARIANT"
SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
PROJECT_DIR="$(cd "$SCRIPT_DIR/.." && pwd)"
OUT="$PROJECT_DIR/build/libcarplay_hook.so"
APAC_OUT="$PROJECT_DIR/build/libcarplay_apac.so"
mkdir -p "$(dirname "$OUT")"

LOG="${LOG:-1}"
LOG_RGD_PACKET_RAW="${LOG_RGD_PACKET_RAW:-0}"
AUDIO_STATE_TRACE="${AUDIO_STATE_TRACE:-0}"
ALTSCREEN_PACE_TRACE="${ALTSCREEN_PACE_TRACE:-0}"
[[ "$LOG" == "0" || "$LOG" == "1" ]] || { echo "Invalid LOG=$LOG (0|1)"; exit 1; }
[[ "$LOG_RGD_PACKET_RAW" == "0" || "$LOG_RGD_PACKET_RAW" == "1" ]] || { echo "Invalid LOG_RGD_PACKET_RAW"; exit 1; }
[[ "$AUDIO_STATE_TRACE" == "0" || "$AUDIO_STATE_TRACE" == "1" ]] || { echo "Invalid AUDIO_STATE_TRACE=$AUDIO_STATE_TRACE (0|1)"; exit 1; }
[[ "$ALTSCREEN_PACE_TRACE" == "0" || "$ALTSCREEN_PACE_TRACE" == "1" ]] || { echo "Invalid ALTSCREEN_PACE_TRACE=$ALTSCREEN_PACE_TRACE (0|1)"; exit 1; }
[[ "$LOG_RGD_PACKET_RAW" == "1" && "$LOG" != "1" ]] && { echo "LOG_RGD_PACKET_RAW=1 requires LOG=1"; exit 1; }

CFLAGS_EXTRA="-D__QNX__ -mfpu=neon -mfloat-abi=softfp"
echo "Stereo upmix renderer: hssr-universal"
[ "$LOG" == "0" ] && CFLAGS_EXTRA="$CFLAGS_EXTRA -DENABLE_LOGGING=0" && echo "Logging and diagnostic captures DISABLED"
[ "$AUDIO_STATE_TRACE" == "1" ] && CFLAGS_EXTRA="$CFLAGS_EXTRA -DENABLE_STATE_TRACE=1" && echo "Compact lifecycle trace ENABLED"
[ "$ALTSCREEN_PACE_TRACE" == "1" ] && CFLAGS_EXTRA="$CFLAGS_EXTRA -DENABLE_ALTSCREEN_PACE_TRACE=1" && echo "Stream-111 pacing trace ENABLED (no media capture)"
[ "$LOG_RGD_PACKET_RAW" == "1" ] && CFLAGS_EXTRA="$CFLAGS_EXTRA -DRGD_TRACE_RAW_FULL=1" && echo "RGD raw packet logging ENABLED"

if ! docker image inspect "$IMG" >/dev/null 2>&1; then
    echo "ERROR: docker image '$IMG' not found."
    echo "Build it once:  docker build --platform=linux/amd64 -t $IMG $PROJECT_DIR/../../Tools/qnx-65-sdp-docker"
    exit 1
fi

. "$(dirname "$0")/apac_flags.sh"

# Optional protected (VIN-locked) build. Set PROT_VIN (17-char) and PROT_SECRET
# to bind both .so to one car: on any other VIN the hook stays inert (stock
# CarPlay) and the APAC codec refuses (fail-closed). Default build is unchanged.
PROT_BUILD=0
if [ -n "${PROT_VIN:-}" ]; then
  [ ${#PROT_VIN} -eq 17 ] || { echo "PROT_VIN must be 17 chars"; exit 1; }
  [ -n "${PROT_SECRET:-}" ] || { echo "PROT_SECRET must be set for a protected build"; exit 1; }
  PROT_BUILD=1
  echo "Protected build: VIN-locked to $PROT_VIN"
fi

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

  # Protected build: compile the VIN-gate protector objects (pure C, no
  # constructor, hidden visibility -> no new dynamic exports, .init_array stays
  # compiler-only). Linked into both .so; each gets its own PROT slot.
  PROT_BUILD='"$PROT_BUILD"'
  PROT_SECRET='"${PROT_SECRET:-}"'
  PROT_OBJS=""; PROT_CC=""
  if [ "$PROT_BUILD" = "1" ]; then
      for p in prot_rt prot_vin carplay_prot; do
          $CC -O2 -fPIC -fvisibility=hidden -std=gnu99 -I/src/protector \
              -DPROT_SECRET="\"$PROT_SECRET\"" -c /src/protector/$p.c -o $OBJ/$p.o
      done
      PROT_OBJS="$OBJ/prot_rt.o $OBJ/prot_vin.o $OBJ/carplay_prot.o"
      PROT_CC="-DCARPLAY_PROT -I/src/protector"
  fi

  # The APAC decoder port and its C shim.  The flag set comes from
  # scripts/apac_flags.sh, which apac_decoder/bench/build_qnx_bench.sh sources
  # too - a benchmark compiled with drifted flags reports headroom for a binary
  # nobody ships, and says nothing about it.
  CXXFLAGS="'"$APAC_CXXFLAGS_NOINC"' \
            -I/src/apac_decoder/include -I/src/hook/audio"
  for f in /src/apac_decoder/src/*.cpp /src/hook/audio/apac_bridge.cpp; do
      $CXX $CXXFLAGS $PROT_CC -c "$f" -o "$OBJ/cxx_$(basename "$f" .cpp).o"
  done
  echo "  compiled $(ls $OBJ/cxx_*.o | wc -l | tr -d " ") APAC decoder objects"

  # Optional decoder library. One versioned C function table is its only
  # contract with libcarplay_hook.so.
  $CC -shared -fPIC -O2 $OBJ/cxx_*.o $PROT_OBJS -o /src/build/libcarplay_apac.so \
      -Wl,--gc-sections -Wl,-soname,libcarplay_apac.so -lm

  cd /src/hook
  SRCS="framework/logging.c framework/state_trace.c framework/signal_guard.c framework/bus.c framework/iap2_protocol.c framework/hook_framework.c \
        framework/airplay_seams.c \
        routeguidance/rgd_tlv.c routeguidance/rgd_hook.c \
        coverart/jpeg_safety.c coverart/coverart_stream.c coverart/coverart_hook.c \
        altscreen/altscreen_av.c altscreen/altscreen_hook.c audio/spatial_downmix.c \
        audio/stereo_upmix.c audio/hssr_dsp.c audio/hssr_neon.c audio/hssr_control_worker.c \
        audio/apap_crypto.c audio/apap_lifecycle.c audio/soundcheck.c audio/apac_codec_loader.c \
        audio/aac_bridge.c audio/audio_hook.c main.c"
  $CC -shared -fPIC -O2 -std=gnu99 -fvisibility=hidden -fdata-sections -ffunction-sections '"$CFLAGS_EXTRA"' $PROT_CC \
      -I. -I/src/apac_decoder/include $SRCS $PROT_OBJS \
      -o /src/build/libcarplay_hook.so -Wl,--gc-sections \
      -Wl,--version-script=/src/hook/carplay_hook.exports.map \
      -lz -lsocket -lm

  fail() { echo "REJECTED: $*"; exit 1; }
  SO=/src/build/libcarplay_hook.so
  TRACE_ENABLED='"$AUDIO_STATE_TRACE"'

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
  n=$($READELF -d $SO | grep -c libcarplay_apac || true)
  [ "$n" = "0" ] || fail "the hook has a hard dependency on libcarplay_apac"
  n=$($NM -D $SO | grep -cE " T snd_pcm_(open_name|plugin_params|plugin_setup|plugin_write|close)$" || true)
  [ "$n" = "5" ] || fail "QSA bridge exports=$n (expected open/params/setup/write/close)"
  n=$($NM -D $SO | grep -cE " T HID(RemotePlaybackFillReport|DevicePostReport)$" || true)
  if [ "$TRACE_ENABLED" = "1" ]; then
      [ "$n" = "2" ] || fail "state-trace HID seam exports=$n (expected 2)"
  else
      [ "$n" = "0" ] || fail "quiet build unexpectedly exports $n state-trace HID seams"
  fi

  # The LD_PRELOAD ABI is an exact allowlist.  Hidden-by-default compilation
  # prevents accidental compiler exports; the version script prevents a missed
  # annotation from widening it again.  Quiet builds omit the two trace-only HID
  # seams, while diagnostic builds must expose them.
  awk "
      /global:/ { in_global=1; next }
      /local:/  { in_global=0 }
      in_global {
          gsub(/[;[:space:]]/, \"\");
          if (length) print
      }
  " /src/hook/carplay_hook.exports.map | sort > /tmp/hook_exports_expected_all
  if [ "$TRACE_ENABLED" = "1" ]; then
      cp /tmp/hook_exports_expected_all /tmp/hook_exports_expected
  else
      grep -vE "^HID(DevicePostReport|RemotePlaybackFillReport)$" \
          /tmp/hook_exports_expected_all > /tmp/hook_exports_expected
  fi
  $NM -D --defined-only $SO | awk "NF >= 3 { print \$3 }" | sort \
      > /tmp/hook_exports_actual
  if ! diff -u /tmp/hook_exports_expected /tmp/hook_exports_actual; then
      fail "dynamic export set differs from the interpose allowlist"
  fi
  echo "  dynamic exports:$(wc -l < /tmp/hook_exports_actual | tr -d " ") exact interpose symbols"

  # 6. ldqnx must run compiler bookkeeping only.  Module constructors used to
  # reach LOG_INFO -> pthread_create while the QNX loader lock was still held,
  # making cold-start success depend on scheduler timing.  A normal GCC shared
  # object has one 4-byte frame_dummy entry; anything larger is a regression.
  init_size=$($READELF -W -S $SO | awk "\$2 == \".init_array\" { print \$6 }")
  [ "$init_size" = "000004" ] || fail ".init_array size=$init_size (expected compiler-only 000004)"
  n=$($NM -an $SO | grep -cE "(rgd|altscreen|audio)_module_(init|fini)" || true)
  [ "$n" = "0" ] || fail "$n eager module constructor/destructor symbols"

  # Validate the optional decoder independently: dlopen must not reintroduce
  # the QNX emutls/constructor/runtime hazards removed from the main hook.
  APAC_SO=/src/build/libcarplay_apac.so
  n=$($NM $APAC_SO 2>/dev/null | grep -ci emutls || true)
  [ "$n" = "0" ] || fail "APAC: $n emutls symbols present"
  n=$($NM $APAC_SO 2>/dev/null | grep -cE "__cxa_throw|__cxa_allocate_exception|__gxx_personality|_ZTI|_ZTS|_ZTV" || true)
  [ "$n" = "0" ] || fail "APAC: $n exception/RTTI symbols"
  n=$($NM -D --undefined-only $APAC_SO 2>/dev/null | grep -cE "_Znw|_Znа|_Zna|_ZdlPv|_ZdaPv|__cxa_guard|_ZSt|_ZNSt" || true)
  [ "$n" = "0" ] || fail "APAC: $n C++ runtime/STL symbols"
  n=$($READELF -d $APAC_SO | grep -c libstdc || true)
  [ "$n" = "0" ] || fail "APAC links libstdc++"
  init_size=$($READELF -W -S $APAC_SO | awk "\$2 == \".init_array\" { print \$6 }")
  [ "$init_size" = "000004" ] || fail "APAC: .init_array size=$init_size (expected compiler-only 000004)"
  n=$($NM -D $APAC_SO | grep -c " T spa_apac_codec_get_api" || true)
  [ "$n" = "1" ] || fail "APAC: versioned C API export missing/duplicated"

  echo "  undefined:$($NM -D --undefined-only $SO | sed "s/^ *//" | awk "\$1 == \"U\" { printf \" %s\", \$2 }" | head -c 400)"
  echo "  built build/libcarplay_apac.so (ABI v1, emutls=0 exceptions=0 rtti=0 stl=0 libstdc++=0)"
  echo "  built build/libcarplay_hook.so (APAC=dlopen, emutls=0 init_array=compiler-only)"
'

if [ "$AUDIO_STATE_TRACE" = "1" ]; then
    strings "$OUT" | grep -q "IAP_NOW_PLAYING_START" || {
        echo "REJECTED: compact state-trace markers missing"; exit 1;
    }
fi
if [ "$ALTSCREEN_PACE_TRACE" = "1" ]; then
    strings "$OUT" | grep -q "111 pace gen=" || {
        echo "REJECTED: stream-111 pacing markers missing"; exit 1;
    }
fi

echo ""
echo "Compiled: $OUT"
ls -lh "$OUT"
echo "Compiled: $APAC_OUT"
ls -lh "$APAC_OUT"

if [ "$PROT_BUILD" = "1" ]; then
  DEST="$PROJECT_DIR/build/protected_$PROT_VIN"
  echo "=== Assembling protected build in build/protected_$PROT_VIN/ ==="
  rm -rf "$DEST"; mkdir -p "$DEST"
  # Full deployable set (mirrors scripts/package_mib.py). Copy what build/ has;
  # jar/renderers/models come from build_java.sh / build_renderers.sh - run those
  # first for a complete package. The two .so are stamped IN this folder, so the
  # generic build/*.so stay unstamped (inert everywhere).
  SET="libcarplay_hook.so libcarplay_apac.so maneuver_render altscreen_render \
       carplay_hook.jar hssr_model.bin hssr_guard.bin flag_atlas.rgba \
       carplay_startup.sh carplay_monitor.sh carplay_processes.sh carplay_cleanup.sh \
       carplay_child.json carplay_pf.patch deva-ctrl-qc.so"
  miss=""
  for f in $SET; do
    if [ -f "$PROJECT_DIR/build/$f" ]; then cp -p "$PROJECT_DIR/build/$f" "$DEST/"
    else miss="$miss $f"; fi
  done
  [ -n "$miss" ] && echo "  note: not in build/ (build separately):$miss"

  echo "=== Stamping the two .so in the protected folder for VIN $PROT_VIN ==="
  printf '\x5a' > /tmp/prot_consts.bin
  for so in libcarplay_hook.so libcarplay_apac.so; do
    PROT_SECRET="$PROT_SECRET" python3 "$PROJECT_DIR/protector/protect.py" "$DEST/$so" \
        --vin "$PROT_VIN" --mode blind --payload-file /tmp/prot_consts.bin
  done
  rm -f /tmp/prot_consts.bin
  { echo "VIN=$PROT_VIN"; echo "built=$(date -u +%Y-%m-%dT%H:%M:%SZ)"; \
    echo "stamped=libcarplay_hook.so libcarplay_apac.so (blind sentinel gate)"; \
    echo "note=other cars -> hook inert (stock CarPlay), apac fail-closed"; } > "$DEST/PROTECTED.txt"
  ( cd "$DEST" && ls -1 | sed 's/^/  /' )
  echo "Protected build ready: build/protected_$PROT_VIN/  (ACTIVE only on $PROT_VIN)"
fi
