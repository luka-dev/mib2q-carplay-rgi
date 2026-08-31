#!/usr/bin/env bash
# Cross-build apac_bench for QNX 6.5 / armle-v7 - the head unit's own toolchain,
# the head unit's own flags.
#
#   apac_decoder/bench/build_qnx_bench.sh [--variant 4.9|8.5]
#       -> apac_decoder/.build-bench/qnx-<variant>/apac_bench
#
# The flags are the ones scripts/build_hook.sh ships the decoder with, not a
# benchmark's own, and they are not copied here: scripts/apac_flags.sh is the
# single definition and build_hook.sh sources the same file.  A copy that drifts
# does not announce itself - the benchmark would keep reporting headroom for a
# binary that is no longer the one being shipped.  Formerly the same -O2, the
# -D_NO_CPP_INLINES, and the link is done with gcc rather than g++ so nothing
# drags libstdc++ in.  A benchmark built differently from the thing that ships
# measures a different program.
#
# tools/build_qnx.sh is the authority on all of that and this deliberately does
# not touch it; it only adds the two hook translation units and this driver.
set -euo pipefail

HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
APAC="$(cd "$HERE/.." && pwd)"
ROOT="$(cd "$APAC/.." && pwd)"

V=4.9
[ "${1:-}" = "--variant" ] && { V="$2"; shift 2; }
IMG="qnx65-armv7-toolchain:$V"
docker image inspect "$IMG" >/dev/null 2>&1 || { echo "ERROR: $IMG not built" >&2; exit 1; }

docker run --rm --platform=linux/amd64 -v "$ROOT":/src -w /src "$IMG" bash -c '
  set -e
  export PATH=/opt/qnx650/host/linux/x86/usr/bin:$PATH
  export QNX_HOST=/opt/qnx650/host/linux/x86 QNX_TARGET=/opt/qnx650/target/qnx6
  CC=arm-unknown-nto-qnx6.5.0eabi-gcc
  CXX=arm-unknown-nto-qnx6.5.0eabi-g++
  NM=arm-unknown-nto-qnx6.5.0eabi-nm
  O=apac_decoder/.build-bench/qnx-'"$V"'
  rm -rf "$O"; mkdir -p "$O"

  . /src/scripts/apac_flags.sh
  CXXFLAGS="$APAC_CXXFLAGS_NOINC -Wno-error -Iapac_decoder/include -Ihook/audio"
  CFLAGS="$APAC_CFLAGS_NOINC -D__QNX__ -Ihook/audio"

  for f in apac_decoder/src/*.cpp; do
      $CXX $CXXFLAGS -c "$f" -o "$O/$(basename "$f" .cpp).o"
  done
  $CXX $CXXFLAGS -c hook/audio/apac_bridge.cpp -o "$O/apac_bridge.o"
  $CXX $CXXFLAGS -c apac_decoder/bench/apac_bench.cpp -o "$O/apac_bench.o"
  $CC  $CFLAGS  -c hook/audio/apap_crypto.c     -o "$O/apap_crypto.o"
  $CC  $CFLAGS  -c hook/audio/spatial_downmix.c -o "$O/spatial_downmix.o"

  # Linked with gcc: the shipping .so needs no C++ runtime and neither may this.
  $CC "$O"/*.o -o "$O/apac_bench" -lm
  n=$(arm-unknown-nto-qnx6.5.0eabi-readelf -d "$O/apac_bench" | grep -c libstdc || true)
  [ "$n" = 0 ] || { echo "REJECTED: the benchmark links libstdc++"; exit 1; }

  # THE ALLOCATION QUESTION, answered on the objects that ship rather than on
  # the benchmark: the decoder must not allocate.  apac_bridge.o may - it
  # malloc()s the one 1.4 MB decoder at stream start - and the benchmark
  # obviously does, so both are excluded.
  echo "  decoder objects referencing an allocator:"
  bad=0
  for f in $(ls "$O"/*.o | grep -v -e apac_bench.o -e apac_bridge.o -e apap_crypto.o -e spatial_downmix.o); do
      u=$($NM --undefined-only "$f" | grep -oE "malloc|calloc|realloc|free|memalign|posix_memalign|sbrk|mmap|_Znw|_Zna" | sort -u | tr "\n" " " || true)
      [ -z "$u" ] || { echo "    $(basename $f): $u"; bad=1; }
  done
  [ "$bad" = 0 ] && echo "    none - the decoder allocates nothing on the target build"
  echo "  built $O/apac_bench ($(stat -c %s "$O/apac_bench") bytes)"
'
