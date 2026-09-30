#!/usr/bin/env bash
# Standalone Track B graph only; the repository mount is read-only.
set -euo pipefail
HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT="$(cd "$HERE/../../.." && pwd)"
[[ $# -ge 1 && $# -le 2 ]] || { echo "usage: $0 NEW_OUTPUT_DIRECTORY [DOCKER_IMAGE]" >&2; exit 2; }
OUT="$1"
IMAGE="${2:-qnx65-armv7-toolchain:latest}"
[[ ! -e "$OUT" ]] || { echo "preserve previous build: $OUT" >&2; exit 1; }
docker image inspect "$IMAGE" >/dev/null
mkdir -p "$OUT"
OUT="$(cd "$OUT" && pwd)"
. "$ROOT/scripts/apac_flags.sh"
# Save the resolved image, compiler and source identity; tags alone can drift.
docker image inspect "$IMAGE" --format '{{.Id}}' > "$OUT/docker-image-id.txt"
shasum -a 256 "$HERE/unified_runtime.h" "$HERE/unified_runtime.c" "$HERE/unified_runtime_cli.c" "$HERE/unified_audio_stream.h" "$HERE/unified_audio_stream.c" "$HERE/unified_audio_cli.c" > "$OUT/source-sha256.txt"
docker run --rm --platform=linux/amd64 -v "$ROOT":/src:ro -v "$OUT":/out "$IMAGE" bash -c '
set -euo pipefail
export PATH=/opt/qnx650/host/linux/x86/usr/bin:$PATH
export QNX_HOST=/opt/qnx650/host/linux/x86
export QNX_TARGET=/opt/qnx650/target/qnx6
CC=arm-unknown-nto-qnx6.5.0eabi-gcc
$CC --version > /out/compiler-version.txt
FLAGS="'"$APAC_CFLAGS_NOINC $UPMIX_DSP_CFLAGS"' -Werror -mfpu=neon -mfloat-abi=softfp -fstack-usage"
echo "$FLAGS" > /out/flags.txt
$CC $FLAGS -c /src/tools/stereo-upmix-render/unified_runtime_v2/unified_runtime.c -o /out/unified_runtime.o
$CC $FLAGS -c /src/tools/stereo-upmix-render/unified_runtime_v2/unified_runtime_cli.c -o /out/unified_runtime_cli.o
$CC /out/unified_runtime.o /out/unified_runtime_cli.o -lm -o /out/upmix_unified_runtime
$CC $FLAGS -c /src/tools/stereo-upmix-render/unified_runtime_v2/unified_audio_stream.c -o /out/unified_audio_stream.o
$CC $FLAGS -c /src/tools/stereo-upmix-render/unified_runtime_v2/unified_audio_cli.c -o /out/unified_audio_cli.o
$CC /out/unified_runtime.o /out/unified_audio_stream.o /out/unified_audio_cli.o -lm -o /out/upmix_unified_audio_stream
arm-unknown-nto-qnx6.5.0eabi-readelf -h -A /out/upmix_unified_runtime > /out/elf.txt
arm-unknown-nto-qnx6.5.0eabi-objdump -d /out/unified_runtime.o > /out/disassembly.txt
'
shasum -a 256 "$OUT/upmix_unified_runtime" "$OUT/upmix_unified_audio_stream" > "$OUT/executable-sha256.txt"
echo "built standalone runtime: $OUT/upmix_unified_runtime"
