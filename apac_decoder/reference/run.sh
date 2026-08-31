#!/usr/bin/env bash
#
# One command from the repo root:
#
#   ./apac_decoder/reference/run.sh
#
# Builds the arm64 container image if it is missing, decodes the captured
# CarPlay APAC 5.1 stream with Pioneer's copy of Apple's decoder running
# NATIVELY on this Apple Silicon machine, decodes the same packets with macOS
# AudioToolbox, and prints a per-channel comparison.
#
#   ./apac_decoder/reference/run.sh --count 64      # only the first 64 packets
#   ./apac_decoder/reference/run.sh --f32           # float32 out, no Apple compare
#   ./apac_decoder/reference/run.sh --probe         # just load the library
#   ./apac_decoder/reference/run.sh --rebuild       # force an image rebuild
#
# The per-function oracle (apac_fn_ref.cpp) rides in the same image:
#
#   ./apac_decoder/reference/run.sh fn probe        # resolve every symbol
#   ./apac_decoder/reference/run.sh fn sfb          # apac::GetSFBOffsetTable
#   ./apac_decoder/reference/run.sh fn cookie CK    # the C cookie parsers
#   ./apac_decoder/reference/run.sh fn gc CK        # GlobalConfig::Deserialize
#   ./apac_decoder/reference/run.sh fn bs < cases   # TBitstreamReader, per call
#
# Paths given to `fn` are host paths and are bind-mounted read-only.
#
# Everything it writes lives in apac_decoder/reference/.build/ except
# build/apple_apac_oracle.
set -euo pipefail

HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT="$(cd "$HERE/../.." && pwd)"
WORK="$HERE/.build"
IMAGE="${APAC_IMAGE:-apac-ref}"

ROOTFS="${APAC_ROOTFS:-/Users/luka/Desktop/AUDI_2/Firmwares/Other/Pioneer_SPHERA_OTA/extracted_01030010/rootfs}"
CAPTURE="${APAC_CAPTURE:-$ROOT/captures/apac_5_1_live_20260831_0205}"
COOKIE="$CAPTURE/spatial_magiccookie_5_60100d08f9d19abb.bin"
PACKETS="$CAPTURE/apac_packets"

FN=0
if [ "${1:-}" = "fn" ]; then FN=1; shift; fi

REBUILD=0
PROBE=0
FLOAT=0
COUNT=""
EXTRA=()
while [ "$FN" = 0 ] && [ $# -gt 0 ]; do
    case "$1" in
        --rebuild) REBUILD=1 ;;
        --probe)   PROBE=1 ;;
        --f32)     FLOAT=1; EXTRA+=(--f32) ;;
        --count)   COUNT="$2"; EXTRA+=(--count "$2"); shift ;;
        *)         EXTRA+=("$1") ;;
    esac
    shift
done

# ---------------------------------------------------------------- staging ----
# Only the DT_NEEDED closure of libSPF_CP_PlugInWrapper.so is copied.  Several
# entries under the extracted rootfs are 10-12 byte broken symlink stubs left by
# the macOS extraction (lib/libc.so.6, ld-linux-aarch64.so.1, libasound.so.2
# ...); those are deliberately ignored and the container's own glibc / system
# libraries are used instead.
CLOSURE_LOCAL="libGparts.so libNDeviceLibAV.so libNEventBaseLib.so libNPCommonLib.so
               libNStandardLib.so libNStorageLib.so libNSystemInfoLib.so
               libSPF_CP_PlugInWrapper.so"
CLOSURE_USR="libdns_sd.so libcrypto.so.3 libssl.so.3"

stage_libs() {
    mkdir -p "$WORK/libs"
    local f missing=0
    for f in $CLOSURE_LOCAL; do
        [ -s "$WORK/libs/$f" ] || missing=1
    done
    for f in $CLOSURE_USR; do
        [ -s "$WORK/libs/$f" ] || missing=1
    done
    [ "$missing" = 0 ] && return
    echo "staging Pioneer libraries from $ROOTFS"
    for f in $CLOSURE_LOCAL; do
        cp "$ROOTFS/usr/local/lib/$f" "$WORK/libs/$f"
    done
    for f in $CLOSURE_USR; do
        cp "$ROOTFS/usr/lib/$f" "$WORK/libs/$f"
    done
}

stage_libs

# ------------------------------------------------------------------ image ----
if [ "$REBUILD" = 1 ] || ! docker image inspect "$IMAGE" >/dev/null 2>&1; then
    echo "building $IMAGE (linux/arm64)"
    docker build --platform linux/arm64 -t "$IMAGE" -f "$HERE/Dockerfile" "$HERE"
fi

# Per-function oracle.  Any host path in the argument list is bind-mounted at
# the same path inside the container so the arguments need no rewriting.
run_fn() {
    local mounts=() a
    for a in "$@"; do
        if [ -e "$a" ]; then
            mounts+=(-v "$(cd "$(dirname "$a")" && pwd)/$(basename "$a"):$a:ro")
        fi
    done
    docker run --rm -i --platform linux/arm64 \
        -v "$HERE:/src:ro" "${mounts[@]+"${mounts[@]}"}" \
        "$IMAGE" bash -c \
        'g++ -std=c++11 -O2 -g -Wall -o /tmp/apac_fn_ref /src/apac_fn_ref.cpp -ldl && exec /tmp/apac_fn_ref "$@"' \
        _ "$@"
}

if [ "$FN" = 1 ]; then
    run_fn "$@"
    exit $?
fi

run_ref() {
    docker run --rm --platform linux/arm64 \
        -v "$HERE:/src:ro" -v "$CAPTURE:/data:ro" -v "$WORK:/out" \
        "$IMAGE" bash -c \
        'g++ -std=c++11 -O2 -g -Wall -o /tmp/apac_ref /src/apac_ref.cpp -ldl && exec /tmp/apac_ref "$@"' \
        _ "$@"
}

if [ "$PROBE" = 1 ]; then
    run_ref --probe
    exit $?
fi

# ---------------------------------------------------------------- packets ----
if [ ! -d "$PACKETS" ] || [ -z "$(ls -A "$PACKETS" 2>/dev/null)" ]; then
    echo "regenerating APAC packets from the raw APAP capture"
    # The session key never leaves this shell: pioneer_apap_decrypt.py reads it
    # from shk.bin directly and nothing here echoes it.
    python3 "$ROOT/tools/pioneer_apap_decrypt.py" \
        --key-file "$CAPTURE/shk.bin" \
        --output-dir "$WORK/decrypted" \
        --keep-going \
        "$CAPTURE/spatial_apap_5.trunc.bin" >/dev/null
    mkdir -p "$WORK/apac_packets"
    python3 - "$WORK/decrypted/manifest.tsv" "$WORK/apac_packets" <<'PY'
import csv, json, os, sys
manifest, outdir = sys.argv[1], sys.argv[2]
with open(manifest) as fh:
    for row in csv.DictReader(fh, delimiter='\t'):
        blob = json.loads(row['extension_json']).get('trailing_hex', '')
        with open(os.path.join(outdir, 'p%05d.bin' % int(row['index'])), 'wb') as out:
            out.write(bytes.fromhex(blob))
PY
    PACKETS="$WORK/apac_packets"
fi

# Inside the container the capture is at /data and .build at /out.
case "$PACKETS" in
    "$WORK"/*) CTR_PREFIX="/out/apac_packets" ;;
    *)         CTR_PREFIX="/data/apac_packets" ;;
esac

# One shared, deterministically ordered packet list for both decoders.  Three
# frames in this capture carry an empty APAC payload; Apple's converter rejects
# a zero-length packet, so they are excluded from both sides rather than
# handled differently by each.
find "$PACKETS" -name '*.bin' ! -size 0 | sort > "$WORK/host_list.txt"
sed "s|.*/|$CTR_PREFIX/|" "$WORK/host_list.txt" > "$WORK/ctr_list.txt"
echo "packets: $(wc -l < "$WORK/host_list.txt" | tr -d ' ') non-empty of $(ls "$PACKETS" | wc -l | tr -d ' ')"

# ---------------------------------------------------------------- pioneer ----
PIONEER_OUT="pioneer.pcm"
[ "$FLOAT" = 1 ] && PIONEER_OUT="pioneer.f32"
echo
echo "=== Pioneer / Apple APAC decoder, native AArch64 in Docker ==="
run_ref --list /out/ctr_list.txt "${EXTRA[@]}" \
    "/data/$(basename "$COOKIE")" "/out/$PIONEER_OUT"

[ "$FLOAT" = 1 ] && exit 0

# ------------------------------------------------------------------ apple ----
ORACLE="$ROOT/build/apple_apac_oracle"
if [ ! -x "$ORACLE" ] && [ "$(uname -s)" = "Darwin" ]; then
    echo
    echo "building the macOS AudioToolbox oracle"
    mkdir -p "$ROOT/build"
    clang -std=c11 -O2 "$ROOT/tools/apple_apac_oracle.c" \
        -framework AudioToolbox -framework CoreFoundation -o "$ORACLE"
fi
if [ ! -x "$ORACLE" ]; then
    echo "no macOS oracle available; skipping the comparison"
    exit 0
fi

if [ -n "$COUNT" ]; then
    head -n "$COUNT" "$WORK/host_list.txt" > "$WORK/host_list_n.txt"
else
    cp "$WORK/host_list.txt" "$WORK/host_list_n.txt"
fi

echo
echo "=== Apple AudioToolbox (macOS) ==="
# shellcheck disable=SC2046
"$ORACLE" "$COOKIE" "$WORK/apple.pcm" $(cat "$WORK/host_list_n.txt") \
    | tail -n 1

echo
echo "=== comparison ==="
python3 "$HERE/compare_pcm.py" "$WORK/apple.pcm" "$WORK/$PIONEER_OUT" 6
