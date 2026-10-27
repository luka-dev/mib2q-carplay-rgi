#!/usr/bin/env bash
# Build apac_bench for THIS machine (arm64 macOS).  Same sources the head unit
# gets, a different compiler and a very different core - see STATUS.md,
# stage k, for what that number is and is not worth.
#
#   apac_decoder/bench/build_host.sh          -> apac_decoder/.build/apac_bench
set -euo pipefail

HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
APAC="$(cd "$HERE/.." && pwd)"
ROOT="$(cd "$APAC/.." && pwd)"
OUT="$APAC/.build-bench"
mkdir -p "$OUT"

CXX=${CXX:-c++}
CC=${CC:-cc}
CXXFLAGS="-std=c++11 -O2 -g -Wall -Wextra -fno-exceptions -fno-rtti
          -I$APAC/include -I$ROOT/hook/audio"
CFLAGS="-O2 -g -std=gnu99 -Wall -Wextra -I$ROOT/hook/audio"

$CC $CFLAGS -c "$ROOT/hook/audio/apap_crypto.c"    -o "$OUT/bench_apap_crypto.o"
$CC $CFLAGS -c "$ROOT/hook/audio/spatial_downmix.c" -o "$OUT/bench_spatial_downmix.o"
$CXX $CXXFLAGS -c "$ROOT/hook/audio/apac_bridge.cpp" -o "$OUT/bench_apac_bridge.o"
$CXX $CXXFLAGS -c "$HERE/apac_bench.cpp"             -o "$OUT/bench_main.o"

# The decoder itself, with the same flags the Makefile uses.
make -C "$APAC" -j8 all >/dev/null

$CXX -o "$OUT/apac_bench" "$OUT/bench_main.o" "$OUT/bench_apac_bridge.o" \
     "$OUT/bench_apap_crypto.o" "$OUT/bench_spatial_downmix.o" "$APAC"/.build/*.o -lm
echo "built $OUT/apac_bench"
