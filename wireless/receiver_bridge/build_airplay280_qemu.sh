#!/bin/sh
# Build the from-source receiver for QEMU or as an uninstalled on-unit candidate.
set -eu

SCRIPT_DIR=$(CDPATH= cd "$(dirname "$0")" && pwd)
AUDI=$(CDPATH= cd "$SCRIPT_DIR/../../../.." && pwd)
CC=arm-unknown-nto-qnx6.5.0eabi-gcc
if [ "${1:-}" = --inside-docker ]; then
    MODE=${2:-qemu}
else
    MODE=${1:-qemu}
fi
case "$MODE" in
    qemu|on_unit) ;;
    *) echo "usage: sh $0 [qemu|on_unit]" >&2; exit 2 ;;
esac
OUTPUT=/src/Patches/mhi2-carplay/wireless/receiver_bridge/out/airplay280_$MODE

if [ "${1:-}" != --inside-docker ]; then
    python3 "$SCRIPT_DIR/stage_airplay280_qemu.py" \
        "$SCRIPT_DIR/out/airplay280_$MODE/source"
    exec docker run --rm --platform=linux/amd64 \
        -e MHI2Q_QEMU_INFO_CFDIAG -e MHI2Q_QEMU_INFO_FAILDIAG \
        -e MHI2Q_QEMU_CFRELEASE_CALLER -e MHI2Q_QEMU_CFRELEASE_ASSERT \
        -e MHI2Q_QEMU_ASSERT_STACK -e MHI2Q_QEMU_NTCP_TRACE \
        -v "$AUDI:/src" \
        -w "$OUTPUT/source" qnx65-armv7-toolchain:latest \
        sh /src/Patches/mhi2-carplay/wireless/receiver_bridge/build_airplay280_qemu.sh --inside-docker "$MODE"
fi

rm -rf "$OUTPUT/obj"
mkdir -p "$OUTPUT/obj"
SDK=vendor/carplay-sdk
A=$SDK/AccessorySDK
INCS="-Ivendor/shims -Ivendor/shims/OMX -I$A -I$A/Support -I$A/Sources -I$A/EasyConfig \
-I$A/External/Curve25519 -I$A/External/Ed25519 -I$A/External/GladmanAES \
-I$A/External/Small25519 -I$A/External/SRP -I$A/External/LibTomMath -I$A/CoreUtils \
-I$SDK/Support -I$SDK/Sources -I$SDK/Transport -I$SDK/Common -Iinclude"
DEFS="-fPIC -std=gnu99 -D__STDC_LIMIT_MACROS=1 -D_GNU_SOURCE \
-DCFCOMPAT_NOTIFICATIONS_ENABLED=0 -DCFL_BINARY_PLISTS=1 -DAIRPLAY_THREADED_MAIN=1 \
-DCFLITE_ENABLED=1 -DDEBUG_CF_OBJECTS_ENABLED=1 -DDEBUG_EXPORT_ERROR_STRINGS=1 \
-DTARGET_HAS_MD5_UTILS=1 -DAES_UTILS_HAS_GLADMAN_GCM=0 -DAES_UTILS_USE_GLADMAN_AES=1 \
-DTARGET_HAS_SHA_UTILS=1 -DTARGET_NO_OPENSSL=1 -DUSE_VIA_ACE_IF_PRESENT=0 -DTOMMATH=1 \
-DAtomicUtils_USE_BUILTINS=1 -DAIRPLAY_HTTP_SERVER_LEGACY -DHIDUTILS_HID_RAW=0 \
-DDEBUG=1 -DDEV_LOGGING=1"

if [ "$MODE" = qemu ]; then
    sed '/MFiServerPlatformQNX/d; s#^#'"$A"'/#' scripts/coreutils_sources.txt > "$OUTPUT/sources.txt"
else
    sed "s#^#$A/#" scripts/coreutils_sources.txt > "$OUTPUT/sources.txt"
fi
{ cat "$OUTPUT/sources.txt"
  sed "s#^#$SDK/#" scripts/engine_sources.txt
  printf '%s\n' src/hid_2q.c src/bonjour_browser_stub.c src/g_message_stub.c \
      src/qnx_random.c \
      src/screenstream.c
  if [ "$MODE" = qemu ]; then
      printf '%s\n' src/audio_null.c src/mfi_sw.c
  else
      printf '%s\n' src/audio.c
  fi
  printf '%s\n' src/dev_logging.c
} > "$OUTPUT/sources.all.txt"
mv "$OUTPUT/sources.all.txt" "$OUTPUT/sources.txt"

set -- $DEFS '-DAIRPLAY_DEVICE_NAME="Media-Hub v2.0"'
if [ "$MODE" = qemu ]; then
    if [ "${MHI2Q_QEMU_INFO_CFDIAG:-0}" = 1 ] &&
       [ "${MHI2Q_QEMU_INFO_FAILDIAG:-0}" = 1 ]; then
        echo "choose one QEMU /info diagnostic mode" >&2
        exit 2
    fi
    set -- "$@" '-DKEYCHAINS_PATH="/dev/shmem/airplay_keychains"'
    if [ "${MHI2Q_QEMU_INFO_CFDIAG:-0}" = 1 ]; then
        set -- "$@" -DMHI2Q_QEMU_INFO_CFDIAG
    fi
    if [ "${MHI2Q_QEMU_INFO_FAILDIAG:-0}" = 1 ]; then
        set -- "$@" -DMHI2Q_QEMU_INFO_FAILDIAG
    fi
    if [ "${MHI2Q_QEMU_CFRELEASE_CALLER:-0}" = 1 ]; then
        set -- "$@" -DMHI2Q_QEMU_CFRELEASE_CALLER
    fi
    if [ "${MHI2Q_QEMU_CFRELEASE_ASSERT:-0}" = 1 ]; then
        set -- "$@" -DMHI2Q_QEMU_CFRELEASE_ASSERT
    fi
    if [ "${MHI2Q_QEMU_ASSERT_STACK:-0}" = 1 ]; then
        set -- "$@" -DMHI2Q_QEMU_ASSERT_STACK -fno-omit-frame-pointer
    fi
    if [ "${MHI2Q_QEMU_NTCP_TRACE:-0}" = 1 ]; then
        set -- "$@" -DMHI2Q_QEMU_NTCP_TRACE
    fi
else
    set -- "$@" '-DKEYCHAINS_PATH="/mnt/persist/mhi2q-carplay/keychains"'
fi

: > "$OUTPUT/compile.log"
count=0
while IFS= read -r file; do
    obj="$OUTPUT/obj/$(printf '%s' "$file" | tr / _).o"
    # The project has one quoted string define. Keep it as its own argument.
    if ! $CC "$@" $INCS \
        -c "$file" -o "$obj" >> "$OUTPUT/compile.log" 2>&1; then
        printf 'FAILED: %s\n' "$file" | tee -a "$OUTPUT/compile.log"
        exit 1
    fi
    count=$((count + 1))
done < "$OUTPUT/sources.txt"
printf 'Compiled %s files\n' "$count"

: > "$OUTPUT/empty_soname.c"
$CC -shared -fPIC -Wl,-soname,libscreen.so.1 \
    -o "$OUTPUT/obj/screen_soname.so" "$OUTPUT/empty_soname.c" -lc
QT=$QNX_TARGET/armle-v7
# Link the same SDK libraries as the original build. QEMU substitutions affect
# audio/MFi source, not the exported dio ABI.
$CC -shared -fPIC -Wl,-soname,libairplay.so \
    -o "$OUTPUT/libairplay.$MODE.so" "$OUTPUT"/obj/*.o \
    "$QT/lib/libsocket.so.3" "$QT/lib/libasound.so.2" \
    "$QT/usr/lib/libbacktrace.so.1" \
    libs/libdns_sd.so.1 libs/libOmxCore.so libs/libOmxBase.so \
    libs/libomxctx.so.1 "$OUTPUT/obj/screen_soname.so" -lc -lm \
    >> "$OUTPUT/compile.log" 2>&1
if [ "$MODE" = qemu ]; then
    if [ "${MHI2Q_QEMU_INFO_FAILDIAG:-0}" = 1 ]; then
        strings "$OUTPUT/libairplay.$MODE.so" | grep -Fq '[info-faildiag] serialization failed'
    elif [ "${MHI2Q_QEMU_INFO_CFDIAG:-0}" = 1 ]; then
        strings "$OUTPUT/libairplay.$MODE.so" | grep -Fq '[info-cfdiag]'
    fi
    if [ "${MHI2Q_QEMU_CFRELEASE_CALLER:-0}" = 1 ]; then
        strings "$OUTPUT/libairplay.$MODE.so" | grep -Fq '[cfrelease-qemu]'
    fi
fi
sha256sum "$OUTPUT/libairplay.$MODE.so"
