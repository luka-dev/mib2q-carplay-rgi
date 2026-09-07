#!/usr/bin/env bash
set -euo pipefail

HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT="$(cd "$HERE/../../.." && pwd)"
OUT="$ROOT/build/stereo-upmix-bench/host/stereo_upmix_bench"

mkdir -p "$(dirname "$OUT")"
cc -O2 -g -std=gnu99 -Wall -Wextra -Werror \
   -fdata-sections -ffunction-sections \
   -I"$ROOT/hook/audio" \
   "$HERE/stereo_upmix_bench.c" "$ROOT/hook/audio/stereo_upmix.c" \
   "$ROOT/hook/audio/hssr_dsp.c" "$ROOT/hook/audio/hssr_neon.c" \
   "$ROOT/hook/audio/hssr_control_worker.c" -lpthread -lm \
   -o "$OUT"

echo "$OUT"
