#!/usr/bin/env bash
#
# Host tests for the hook's audio path.  No Docker, no head unit, no capture.
#
#   ./scripts/test_hook_audio.sh
#
# spatial_downmix_test    the 5.1.2 fold and the 5.1 channel permutation
# stereo_upmix_test       deterministic PCM2->PCM6 ambience renderer, resets and bounds
# buffered_pcm_switch_test geometry-selected AAC/APAC render policy with stale SETUP indices
# apap_lifecycle_test stopped APAP ownership, ordered resume and exact FLUSH ranges
# apac_type103_test       the whole type-103 render path end to end: APAP framing, decrypt,
#                         the TLV split, APAC decode and the reorder, checked against the PCM
#                         Apple's own decoder produced for the same six packets
#
# The APAC decoder is C++ and the hook is C, so the two are compiled separately and linked with
# the C++ driver here.  On the unit it is the other way round - scripts/build_hook.sh links with
# GCC and asserts libstdc++ never appears - because there the .so must not need a C++ runtime.
set -euo pipefail

HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT="$(cd "$HERE/.." && pwd)"
OUT="$ROOT/build/tests"
mkdir -p "$OUT"

CC="${CC:-cc}"
CXX="${CXX:-c++}"
CFLAGS="-std=gnu99 -O2 -g -Wall -Wextra -Werror -I$ROOT/hook -I$ROOT/hook/audio"
CXXFLAGS="-std=c++11 -O2 -g -Wall -Wextra -Werror -fno-exceptions -fno-rtti \
          -I$ROOT/apac_decoder/include -I$ROOT/hook/audio"

cd "$ROOT"

echo "=== spatial_downmix_test ==="
$CC $CFLAGS tests/spatial_downmix_test.c hook/audio/spatial_downmix.c -o "$OUT/spatial_downmix_test"
"$OUT/spatial_downmix_test"
echo "OK  downmix matrix, saturation, in-place, reorder"

echo "=== rt_upmix_guard_test ==="
$CC $CFLAGS -Ihook tests/rt_upmix_guard_test.c -o "$OUT/rtguard" && "$OUT/rtguard"

echo "=== stereo_upmix_test ==="
$CC $CFLAGS -DSPA_STEREO_UPMIX_TESTING tests/stereo_upmix_test.c hook/audio/stereo_upmix.c \
    hook/audio/scene_engine_math.c hook/audio/scene_clusters.c hook/audio/scene_engine.c hook/audio/scene_mask.c \
    hook/audio/scene_mask_worker.c -lm -lpthread -o "$OUT/stereo_upmix_test"
"$OUT/stereo_upmix_test"

echo "=== buffered_pcm_switch_test ==="
$CC $CFLAGS tests/buffered_pcm_switch_test.c hook/audio/spatial_downmix.c \
    hook/audio/stereo_upmix.c \
    hook/audio/scene_engine_math.c hook/audio/scene_clusters.c hook/audio/scene_engine.c hook/audio/scene_mask.c -lm -o "$OUT/buffered_pcm_switch_test"
"$OUT/buffered_pcm_switch_test"

echo "=== aac_fifo_test ==="
$CC $CFLAGS tests/aac_fifo_test.c -o "$OUT/aac_fifo_test"
"$OUT/aac_fifo_test"

echo "=== feedback_clock_test ==="
$CC $CFLAGS tests/feedback_clock_test.c -o "$OUT/feedback_clock_test"
"$OUT/feedback_clock_test"

echo "=== apap_lifecycle_test ==="
$CC $CFLAGS tests/apap_lifecycle_test.c hook/audio/apap_lifecycle.c \
    -o "$OUT/apap_lifecycle_test"
"$OUT/apap_lifecycle_test"

echo "=== soundcheck_test ==="
$CC $CFLAGS tests/soundcheck_test.c hook/audio/soundcheck.c -lm \
    -o "$OUT/soundcheck_test"
"$OUT/soundcheck_test"

echo "=== coverart_safety_test ==="
$CC $CFLAGS tests/coverart_safety_test.c hook/coverart/jpeg_safety.c \
    -o "$OUT/coverart_safety_test"
"$OUT/coverart_safety_test"

echo "=== apac_type103_test ==="
rm -rf "$OUT/obj"; mkdir -p "$OUT/obj"
for f in "$ROOT"/apac_decoder/src/*.cpp "$ROOT"/hook/audio/apac_bridge.cpp; do
    $CXX $CXXFLAGS -c "$f" -o "$OUT/obj/$(basename "$f" .cpp).o"
done
$CC $CFLAGS -c tests/apac_type103_test.c -o "$OUT/obj/apac_type103_test.o"
$CC $CFLAGS -c hook/audio/apap_crypto.c -o "$OUT/obj/apap_crypto.o"
$CC $CFLAGS -c hook/audio/spatial_downmix.c -o "$OUT/obj/spatial_downmix.o"
$CXX "$OUT"/obj/*.o -o "$OUT/apac_type103_test" -lm
"$OUT/apac_type103_test" "$ROOT/apac_decoder/tests/fixtures/frames.bin"
