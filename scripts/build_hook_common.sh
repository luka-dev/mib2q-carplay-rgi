#!/bin/bash
# Shared compile+verify core for the CarPlay hook, sourced by the build entrypoints:
#   build_hook.sh            -> build into build/unprotected/
# >>> protected
#   build_hook_protected.sh  -> VIN-locked build into build/protected_<VIN>/
#
# There is ONE copy of the compile and the ABI-safety verify on purpose. The
# protected .so must clear the very same emutls / exports-allowlist / init_array
# gates as the shipping one; a forked second copy of that verify would drift and
# could ship a bricked binary to a tester. Protection is a compile-time concern
# (the gate is #ifdef CARPLAY_PROT and, unstamped, fails inert on every car), so
# it cannot be a post-step over an ordinary .so - hence a shared core, not a
# wrapper.
# <<< protected
#
# The caller sets, then calls carplay_build_hook:
#   OUTDIR_REL   (required)  output dir relative to the repo root, e.g. build/unprotected
# >>> protected
#   PROT_BUILD   (0/1, default 0)   1 links the VIN-gate protector objects
#   PROT_SECRET  (hex)              required when PROT_BUILD=1
# <<< protected
# It also honors: QNX_TOOLCHAIN_VARIANT,
# HOOK_AUDIO, AUDIO_STATE_TRACE, ALTSCREEN_PACE_TRACE, LOG_RGD_PACKET_RAW.
# On success it sets OUT and APAC_OUT (absolute paths) for the caller.
#
# THE APAC DECODER ships as optional <OUTDIR>/libcarplay_apac.so, dlopen'd only
# for an enabled buffered Spatial stream, so the hook has no DT_NEEDED edge to
# it. It is C++ with a C seam, linked with GCC (not G++) so libstdc++.so.6 never
# lands in DT_NEEDED - that library is not on the head unit's image. The verify
# below is the same set apac_decoder/tools/build_qnx.sh makes (emutls=0,
# exceptions=0, rtti=0, stl=0), applied to the object that actually ships.

_BHC_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

carplay_build_hook() {
  local PROJECT_DIR OUTDIR_REL IMG
  PROJECT_DIR="$(cd "$_BHC_DIR/.." && pwd)"

  OUTDIR_REL="${OUTDIR_REL:?carplay_build_hook: OUTDIR_REL must be set}"
  # >>> protected
  local PROT_BUILD="${PROT_BUILD:-0}" PROT_SECRET_ENC=""
  local PROT_SECRET="${PROT_SECRET:-}"
  if [ "$PROT_BUILD" = "1" ] && [ -z "$PROT_SECRET" ]; then
    echo "carplay_build_hook: PROT_BUILD=1 requires PROT_SECRET"; return 1
  fi
  # The runtime gets the secret masked (protector/prot_rt.c prot_secret), never as a string.
  [ "$PROT_BUILD" = "1" ] && PROT_SECRET_ENC="$(python3 -c 'import sys; print(",".join(str(b ^ ((i*167+59) & 255)) for i, b in enumerate(sys.argv[1].encode())))' "$PROT_SECRET")"
  # <<< protected

  local QNX_TOOLCHAIN_VARIANT="${QNX_TOOLCHAIN_VARIANT:-latest}"
  case "$QNX_TOOLCHAIN_VARIANT" in
    latest|4.9|8.5) ;;
    *) echo "Invalid QNX_TOOLCHAIN_VARIANT=$QNX_TOOLCHAIN_VARIANT (latest|4.9|8.5)"; return 1 ;;
  esac
  IMG="qnx65-armv7-toolchain:$QNX_TOOLCHAIN_VARIANT"
  OUT="$PROJECT_DIR/$OUTDIR_REL/libcarplay_hook.so"
  APAC_OUT="$PROJECT_DIR/$OUTDIR_REL/libcarplay_apac.so"
  mkdir -p "$PROJECT_DIR/$OUTDIR_REL"

  local LOG_RGD_PACKET_RAW="${LOG_RGD_PACKET_RAW:-0}"
  local AUDIO_STATE_TRACE="${AUDIO_STATE_TRACE:-0}"
  local ALTSCREEN_PACE_TRACE="${ALTSCREEN_PACE_TRACE:-0}"
  # HOOK_AUDIO=0 builds the hook without the type-103 audio module (stock CarPlay
  # audio; no QSA/AirPlay audio interposers, no libcarplay_apac.so).
  local HOOK_AUDIO="${HOOK_AUDIO:-1}"
  local HOOK_WIRELESS="${HOOK_WIRELESS:-0}"
  local WIRELESS_ALT_QEMU="${WIRELESS_ALT_QEMU:-0}"
  # There is one production image: logging compiled in, WARN only, and every runtime
  # diagnostic behind carplay_verbose (log_verbose()).  The old LOG=0 variant is gone.
  [ "${LOG:-}" = 0 ] && { echo "LOG=0 is gone: the hook always logs WARN; touch carplay_verbose for diagnostics"; return 1; }
  [[ "$LOG_RGD_PACKET_RAW" == "0" || "$LOG_RGD_PACKET_RAW" == "1" ]] || { echo "Invalid LOG_RGD_PACKET_RAW"; return 1; }
  [[ "$AUDIO_STATE_TRACE" == "0" || "$AUDIO_STATE_TRACE" == "1" ]] || { echo "Invalid AUDIO_STATE_TRACE=$AUDIO_STATE_TRACE (0|1)"; return 1; }
  [[ "$ALTSCREEN_PACE_TRACE" == "0" || "$ALTSCREEN_PACE_TRACE" == "1" ]] || { echo "Invalid ALTSCREEN_PACE_TRACE=$ALTSCREEN_PACE_TRACE (0|1)"; return 1; }
  [[ "$HOOK_AUDIO" == "0" || "$HOOK_AUDIO" == "1" ]] || { echo "Invalid HOOK_AUDIO=$HOOK_AUDIO (0|1)"; return 1; }
  [[ "$HOOK_WIRELESS" == "0" || "$HOOK_WIRELESS" == "1" ]] || { echo "Invalid HOOK_WIRELESS=$HOOK_WIRELESS (0|1)"; return 1; }
  [[ "$WIRELESS_ALT_QEMU" == "0" || "$WIRELESS_ALT_QEMU" == "1" ]] || { echo "Invalid WIRELESS_ALT_QEMU (0|1)"; return 1; }
  if [ "$WIRELESS_ALT_QEMU" = "1" ]; then
      [ "$HOOK_WIRELESS" = "1" ] || { echo "WIRELESS_ALT_QEMU requires HOOK_WIRELESS=1"; return 1; }
      case "$OUTDIR_REL" in build/wireless-alt-qemu) ;; *) echo "WIRELESS_ALT_QEMU must use build/wireless-alt-qemu"; return 1 ;; esac
  fi
  # A trace build writes only its compact ledger, so the normal hook log is compiled out.
  local TRACE_ONLY=0
  [[ "$AUDIO_STATE_TRACE" == "1" || "$ALTSCREEN_PACE_TRACE" == "1" ]] && TRACE_ONLY=1
  [[ "$LOG_RGD_PACKET_RAW" == "1" && "$TRACE_ONLY" == "1" ]] && { echo "LOG_RGD_PACKET_RAW=1 needs the normal log, not a trace build"; return 1; }

  local CFLAGS_EXTRA="-D__QNX__ -mfpu=neon -mfloat-abi=softfp"
  CFLAGS_EXTRA="$CFLAGS_EXTRA -DHOOK_WIRELESS=$HOOK_WIRELESS"
  [ "$WIRELESS_ALT_QEMU" = "1" ] && CFLAGS_EXTRA="$CFLAGS_EXTRA -DMHI2Q_WIRELESS_ALT_QEMU=1"
  if [ "$HOOK_WIRELESS" = "1" ]; then
      QNX_TOOLCHAIN_VARIANT="$QNX_TOOLCHAIN_VARIANT" python3 "$PROJECT_DIR/hook/wireless/build_pairing.py" || return 1
  fi
  if [ "$HOOK_AUDIO" == "0" ]; then
      CFLAGS_EXTRA="$CFLAGS_EXTRA -DHOOK_AUDIO=0"
      echo "Type-103 audio module EXCLUDED (stock CarPlay audio, no libcarplay_apac.so)"
  else
      echo "Stereo upmix renderer: scene"
  fi
  [ "$TRACE_ONLY" == "1" ] && CFLAGS_EXTRA="$CFLAGS_EXTRA -DENABLE_LOGGING=0" && echo "Trace build: normal hook log compiled out"
  [ "$AUDIO_STATE_TRACE" == "1" ] && CFLAGS_EXTRA="$CFLAGS_EXTRA -DENABLE_STATE_TRACE=1" && echo "Compact lifecycle trace ENABLED"
  [ "$ALTSCREEN_PACE_TRACE" == "1" ] && CFLAGS_EXTRA="$CFLAGS_EXTRA -DENABLE_ALTSCREEN_PACE_TRACE=1" && echo "Stream-111 pacing trace ENABLED (no media capture)"
  [ "$LOG_RGD_PACKET_RAW" == "1" ] && CFLAGS_EXTRA="$CFLAGS_EXTRA -DRGD_TRACE_RAW_FULL=1" && echo "RGD raw packet logging ENABLED"

  if ! docker image inspect "$IMG" >/dev/null 2>&1; then
      echo "ERROR: docker image '$IMG' not found."
      echo "Build it once from https://github.com/luka-dev/qnx65-armv7-toolchain :"
      echo "  ./host-scripts/qnx-run.sh build $([ "$QNX_TOOLCHAIN_VARIANT" = latest ] || echo "$QNX_TOOLCHAIN_VARIANT")"
      return 1
  fi

  # The one definition of the APAC/hook compiler flags (also sourced by the
  # bench, so a drifted flag list can't measure a binary nobody ships).
  . "$_BHC_DIR/apac_flags.sh"

  # >>> protected
  [ "$PROT_BUILD" = "1" ] && echo "Protected build: VIN-gate objects linked in"
  # <<< protected
  echo "=== CarPlay Hook Build (Docker $IMG) -> $OUTDIR_REL ==="

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

    OUTDIR_REL='"$OUTDIR_REL"'
    OUTD=/src/$OUTDIR_REL
    mkdir -p $OUTD

    HOOK_AUDIO='"$HOOK_AUDIO"'
    HOOK_WIRELESS='"$HOOK_WIRELESS"'
    EXTRA_OBJS=""; EXTRA_CC=""
    # >>> protected
    # Protected build: compile the VIN-gate protector objects (pure C, no
    # constructor, hidden visibility -> no new dynamic exports, .init_array stays
    # compiler-only). Linked into both .so; each gets its own PROT slot.
    PROT_BUILD='"$PROT_BUILD"'
    PROT_SECRET_ENC='"$PROT_SECRET_ENC"'
    if [ "$PROT_BUILD" = "1" ]; then
        for p in prot_rt prot_vin carplay_prot; do
            $CC -O2 -fPIC -fvisibility=hidden -std=gnu99 -I/src/protector \
                -DPROT_SECRET_ENC="$PROT_SECRET_ENC" -c /src/protector/$p.c -o $OBJ/$p.o
        done
        EXTRA_OBJS="$OBJ/prot_rt.o $OBJ/prot_vin.o $OBJ/carplay_prot.o"
        EXTRA_CC="-DCARPLAY_PROT -I/src/protector"
    fi
    # <<< protected

    if [ "$HOOK_AUDIO" = "1" ]; then
    # The APAC decoder port and its C shim.  The flag set comes from
    # scripts/apac_flags.sh, which apac_decoder/bench/build_qnx_bench.sh sources
    # too - a benchmark compiled with drifted flags reports headroom for a binary
    # nobody ships, and says nothing about it.
    CXXFLAGS="'"$APAC_CXXFLAGS_NOINC"' \
              -I/src/apac_decoder/include -I/src/hook/audio"
    for f in /src/apac_decoder/src/*.cpp /src/hook/audio/apac_bridge.cpp; do
        $CXX $CXXFLAGS $EXTRA_CC -c "$f" -o "$OBJ/cxx_$(basename "$f" .cpp).o"
    done
    echo "  compiled $(ls $OBJ/cxx_*.o | wc -l | tr -d " ") APAC decoder objects"

    # Optional decoder library. One versioned C function table is its only
    # contract with libcarplay_hook.so.
    $CC -shared -fPIC -O2 $OBJ/cxx_*.o $EXTRA_OBJS -o $OUTD/libcarplay_apac.so \
        -Wl,--gc-sections -Wl,-soname,libcarplay_apac.so -lm
    fi

    cd /src/hook
    SRCS="framework/logging.c framework/state_trace.c framework/signal_guard.c framework/bus.c framework/iap2_protocol.c framework/hook_framework.c \
          framework/airplay_seams.c \
          routeguidance/rgd_tlv.c routeguidance/rgd_hook.c \
          coverart/jpeg_safety.c coverart/coverart_stream.c coverart/coverart_hook.c \
          altscreen/altscreen_av.c altscreen/altscreen_hook.c telephony/tel_hook.c vehicle/vehicle_hook.c main.c"
    AUDIO_SRCS="audio/spatial_downmix.c audio/apap_crypto.c audio/apap_lifecycle.c \
          audio/apac_codec_loader.c audio/aac_bridge.c audio/audio_hook.c audio/upmix_level.c"
    HOOK_CFLAGS="-fPIC -O2 -std=gnu99 -fvisibility=hidden -fdata-sections -ffunction-sections '"$CFLAGS_EXTRA"' $EXTRA_CC -I. -I/src/apac_decoder/include"
    DSP_OBJS=""
    WIRELESS_LIB=""
    if [ "$HOOK_WIRELESS" = "1" ]; then
        SRCS="$SRCS framework/http210_seams.c wireless/stock210_abi.c wireless/wireless_hook.c wireless/media.c wireless/advertisement.c"
        WIRELESS_LIB="/src/build/wireless-sdk/libwireless_pairing.a -lbacktrace"
    fi
    if [ "$HOOK_AUDIO" = "1" ]; then
        SRCS="$SRCS $AUDIO_SRCS"
        # The upmix DSP gets its own optimisation flags (scripts/apac_flags.sh).
        for f in '"$UPMIX_DSP_SRCS"'; do
            o=$OBJ/dsp_$(basename "$f" .c).o
            $CC $HOOK_CFLAGS '"$UPMIX_DSP_CFLAGS"' -c "$f" -o "$o"
            DSP_OBJS="$DSP_OBJS $o"
        done
    fi
    $CC -shared $HOOK_CFLAGS $SRCS $DSP_OBJS $EXTRA_OBJS $WIRELESS_LIB \
        -o $OUTD/libcarplay_hook.so -Wl,--gc-sections \
        -Wl,--version-script=/src/hook/carplay_hook.exports.map \
        -lz -lsocket -lm

    fail() { echo "REJECTED: $*"; exit 1; }
    SO=$OUTD/libcarplay_hook.so
    TRACE_ENABLED='"$AUDIO_STATE_TRACE"'
    n=$($NM -D --undefined-only $SO 2>/dev/null | grep -cE " (cp280_|cp_pair_|xpc_)" || true)
    [ "$n" = "0" ] || fail "$n unresolved private pairing/XPC symbols"

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
    if [ "$HOOK_AUDIO" = "1" ]; then
        [ "$n" = "5" ] || fail "QSA bridge exports=$n (expected open/params/setup/write/close)"
    else
        [ "$n" = "0" ] || fail "audio-less build still exports $n QSA symbols"
    fi
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
    if [ "$HOOK_AUDIO" = "0" ]; then
        # Every interposer audio_hook.c owns: the QSA bridge, the AirPlay audio
        # format/stream seams, the /info property rail and the CQNXSoundIF seams.
        grep -vE "^(snd_pcm_.*|AudioSessionGetSupportedOutputFormats|AirPlayAudioFormatToASBD|AudioStreamPrepare|AudioStreamStop|AirTunesServer_EnsureCreated|AirPlayReceiverServerPlatformCopyProperty|AirPlayReceiverSessionControl|_ZN3dio11CQNXSoundIF.*createMetadata.*)$" \
            /tmp/hook_exports_expected > /tmp/hook_exports_expected_noaudio
        mv /tmp/hook_exports_expected_noaudio /tmp/hook_exports_expected
    fi
    if [ "$HOOK_WIRELESS" = "0" ]; then
        grep -vE "^(HTTPServerCreate|HTTPMessageReadMessage|SocketWriteData|carplay_wireless_copy_pairing_identity|carplay_wireless_receiver_ready|carplay_wireless_receiver_control|AirPlayGetFeatures|TXTRecordSetValue|HTTPServerSetDispatchQueue|HTTPClientCreateWithSocket|AirPlayReceiverSessionScreen_SetSecurityInfo|AirPlayReceiverSessionScreen_ProcessFrames|SocketRecvFrom|sendto|close)$" \
            /tmp/hook_exports_expected > /tmp/hook_exports_expected_nowireless
        mv /tmp/hook_exports_expected_nowireless /tmp/hook_exports_expected
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

    if [ "$HOOK_AUDIO" = "0" ]; then
        echo "  undefined:$($NM -D --undefined-only $SO | sed "s/^ *//" | awk "\$1 == \"U\" { printf \" %s\", \$2 }" | head -c 400)"
        echo "  built $OUTDIR_REL/libcarplay_hook.so (AUDIO=excluded, emutls=0 init_array=compiler-only)"
        exit 0
    fi
    # Validate the optional decoder independently: dlopen must not reintroduce
    # the QNX emutls/constructor/runtime hazards removed from the main hook.
    APAC_SO=$OUTD/libcarplay_apac.so
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
    echo "  built $OUTDIR_REL/libcarplay_apac.so (ABI v1, emutls=0 exceptions=0 rtti=0 stl=0 libstdc++=0)"
    echo "  built $OUTDIR_REL/libcarplay_hook.so (APAC=dlopen, emutls=0 init_array=compiler-only)"
  ' || return 1

  if [ "$AUDIO_STATE_TRACE" = "1" ]; then
      strings "$OUT" | grep -q "IAP_NOW_PLAYING_START" || {
          echo "REJECTED: compact state-trace markers missing"; return 1;
      }
  fi
  if [ "$ALTSCREEN_PACE_TRACE" = "1" ]; then
      strings "$OUT" | grep -q "111 pace gen=" || {
          echo "REJECTED: stream-111 pacing markers missing"; return 1;
      }
  fi

  echo ""
  echo "Compiled: $OUT"
  ls -lh "$OUT"
  if [ "$HOOK_AUDIO" = "1" ]; then
    echo "Compiled: $APAC_OUT"
    ls -lh "$APAC_OUT"
  else
    # The output dir is per-build; drop a codec left by an earlier audio build so
    # it cannot ship (or be VIN-stamped) alongside an audio-less hook.
    rm -f "$APAC_OUT"
    echo "Audio module excluded: no $APAC_OUT"
  fi
}
