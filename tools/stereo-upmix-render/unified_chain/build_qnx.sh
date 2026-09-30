#!/usr/bin/env bash
# Existing reviewed NN-free copies are read-only. Never writes hook/audio.
set -euo pipefail
HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT="$(cd "$HERE/../../.." && pwd)"
[[ $# -ge 2 && $# -le 3 ]] || { echo "usage: $0 COPIED_RENDERER_DIR NEW_OUTPUT [IMAGE]" >&2; exit 2; }
COPIES="$(cd "$1" && pwd)";OUT="$2";IMAGE="${3:-qnx65-armv7-toolchain:latest}"
[[ ! -e "$OUT" ]] || { echo "preserve existing build" >&2;exit 1; }
for f in manifest.json stereo_upmix.c hssr_dsp.c hssr_neon.c hssr_mask.c;do [[ -f "$COPIES/$f" ]];done
docker image inspect "$IMAGE" >/dev/null
mkdir -p "$OUT";OUT="$(cd "$OUT" && pwd)"
. "$ROOT/scripts/apac_flags.sh"
cp "$COPIES/manifest.json" "$OUT/copied-renderer-manifest.json"
docker image inspect "$IMAGE" --format '{{.Id}}' > "$OUT/docker-image-id.txt"
shasum -a 256 "$HERE/unified_chain_cli.c" "$COPIES/"{stereo_upmix,hssr_dsp,hssr_neon,hssr_mask}.c "$ROOT/tools/stereo-upmix-render/unified_runtime_v2/"{unified_runtime,unified_audio_stream}.{h,c} "$ROOT/hook/audio/soundcheck.c" > "$OUT/source-sha256.txt"
docker run --rm --platform=linux/amd64 -v "$ROOT":/src:ro -v "$COPIES":/copies:ro -v "$OUT":/out "$IMAGE" bash -c '
set -euo pipefail
export PATH=/opt/qnx650/host/linux/x86/usr/bin:$PATH
export QNX_HOST=/opt/qnx650/host/linux/x86
export QNX_TARGET=/opt/qnx650/target/qnx6
CC=arm-unknown-nto-qnx6.5.0eabi-gcc
$CC --version > /out/compiler-version.txt
FLAGS="'"$APAC_CFLAGS_NOINC $UPMIX_DSP_CFLAGS"' -Werror -mfpu=neon -mfloat-abi=softfp -fstack-usage -DSPA_HSSR_TEST_MASK -DSPA_HSSR_TEST_DIRECT_NO_BED -I/src/hook/audio -I/src/tools/stereo-upmix-render/unified_runtime_v2"
echo "$FLAGS" > /out/flags.txt
$CC $FLAGS -c /src/tools/stereo-upmix-render/unified_chain/unified_chain_cli.c -o /out/chain.o
$CC $FLAGS -c /src/tools/stereo-upmix-render/unified_runtime_v2/unified_runtime.c -o /out/runtime.o
$CC $FLAGS -c /src/tools/stereo-upmix-render/unified_runtime_v2/unified_audio_stream.c -o /out/stream.o
for f in stereo_upmix hssr_dsp hssr_neon hssr_mask;do
    $CC $FLAGS -Wno-unused-function -Wno-unused-parameter -c /copies/$f.c -o /out/$f.o
done
$CC $FLAGS -c /src/hook/audio/soundcheck.c -o /out/soundcheck.o
$CC /out/chain.o /out/runtime.o /out/stream.o /out/stereo_upmix.o /out/hssr_dsp.o /out/hssr_neon.o /out/hssr_mask.o /out/soundcheck.o -pthread -lm -o /out/upmix_unified_chain
arm-unknown-nto-qnx6.5.0eabi-readelf -h -A -d /out/upmix_unified_chain > /out/elf.txt
'
shasum -a 256 "$OUT/upmix_unified_chain" > "$OUT/executable-sha256.txt"
echo "built standalone QNX causal chain: $OUT/upmix_unified_chain"
