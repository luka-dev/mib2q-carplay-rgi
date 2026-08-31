#!/usr/bin/env bash
#
# Stage i: cross-build the APAC decoder port for QNX 6.5 / armle-v7 and enforce
# the traps that make an LD_PRELOADed .so on that platform work at all.
#
#   apac_decoder/tools/build_qnx.sh                 # GCC 4.9.4, the baseline
#   apac_decoder/tools/build_qnx.sh --variant 8.5   # GCC 8.5.0
#   apac_decoder/tools/build_qnx.sh --both
#   apac_decoder/tools/build_qnx.sh --qemu          # also RUN the self-contained
#                                                   # tests on QNX under QEMU
#
# Uses the project's existing toolchain images (Tools/qnx-65-sdp-docker).  The
# decoder is not a .so of its own on the unit - it links into libcarplay_hook.so
# - but linking one here is what lets the symbol assertions below run against a
# real link, not just against object files.
#
# THE ONE SOURCE CHANGE THIS NEEDED
#   QNX 6.5's <math.h> puts every C99 function behind `#if _HAS_C9X`, which the
#   headers set only for `_C99`, i.e. for C compiled as C99.  In C++ mode the
#   declarations are simply absent, so `log2f` (apac::CoreDecoderConfig, the
#   channel-remap width) did not compile.  The SYMBOL is there - `nm -D
#   libm.so.2` finds log2f, log2, exp2f, pow, powf, ceilf - so it is a missing
#   declaration, not a missing function, and src/config.cpp now declares it
#   under `#if defined(__QNX__)`.  Defining _HAS_C9X from here instead looked
#   tidier and is wrong: it also switches on QNX's own C++ <math.h> overloads,
#   which then redefine GCC <cmath>'s in any translation unit including both -
#   tests/test_mdct.cpp does.
#
# WHY -D_NO_CPP_INLINES
#   QNX 6.5's <math.h> also defines its own C++ overloads of sqrt/sin/cos/tan/...
#   in namespace std (guarded by `#if defined(__cplusplus) && !defined(
#   _NO_CPP_INLINES)`).  GCC's own <cmath> defines the same overloads, so any
#   translation unit that includes <math.h> before <cmath> - tests/dsp_fixture.h
#   then tests/test_mdct.cpp - fails to compile with dozens of "redefinition of
#   constexpr float std::sqrt(float)".  _NO_CPP_INLINES tells the QNX header to
#   stand back and let the compiler's <cmath> own them; the C declarations it
#   still provides are what the port itself uses.  The effect on the port is
#   that sin/cos reach libm as `sin`/`cos` instead of QNX's inline `_Sin(x,0)`
#   /`_Sin(x,1)` - the same libm code either way, and the QEMU run below proves
#   it by keeping every bit-exact fixture test passing on the target.
set -euo pipefail

HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
APAC="$(cd "$HERE/.." && pwd)"
ROOT="$(cd "$APAC/.." && pwd)"
LIBM_OVERRIDE="${LIBM_OVERRIDE:-}"
OUT="$APAC/.build-qnx"

VARIANTS=(4.9)
QEMU=0
while [ $# -gt 0 ]; do
    case "$1" in
        --variant) VARIANTS=("$2"); shift ;;
        --both)    VARIANTS=(4.9 8.5) ;;
        --qemu)    QEMU=1 ;;
        --libm)    LIBM_OVERRIDE="$2"; shift ;;
        *) echo "usage: build_qnx.sh [--variant 4.9|8.5] [--both] [--qemu] [--libm FILE]" >&2; exit 2 ;;
    esac
    shift
done

CXXFLAGS='-std=c++11 -O2 -g -Wall -Wextra -Werror -fno-exceptions -fno-rtti -D_NO_CPP_INLINES -Iinclude'

mkdir -p "$OUT"

for V in "${VARIANTS[@]}"; do
    IMG="qnx65-armv7-toolchain:$V"
    docker image inspect "$IMG" >/dev/null 2>&1 || {
        echo "ERROR: $IMG not built.  See Tools/qnx-65-sdp-docker/README.md" >&2; exit 1; }
    echo "=== APAC decoder, QNX 6.5 armle-v7, GCC $V ==="
    docker run --rm --platform=linux/amd64 -v "$ROOT":/src -w /src/apac_decoder "$IMG" bash -c '
      set -e
      export PATH=/opt/qnx650/host/linux/x86/usr/bin:$PATH
      export QNX_HOST=/opt/qnx650/host/linux/x86 QNX_TARGET=/opt/qnx650/target/qnx6
      CXX=arm-unknown-nto-qnx6.5.0eabi-g++
      NM=arm-unknown-nto-qnx6.5.0eabi-nm
      O=.build-qnx/'"$V"'
      rm -rf "$O"; mkdir -p "$O"
      for f in src/*.cpp; do
          $CXX '"$CXXFLAGS"' -fPIC -c "$f" -o "$O/$(basename "$f" .cpp).o"
      done
      $CXX -shared -fPIC -o "$O/libapac_decoder.so" "$O"/*.o -lm
      echo "  linked $O/libapac_decoder.so  ($(stat -c %s "$O/libapac_decoder.so") bytes)"

      fail() { echo "REJECTED: $*"; exit 1; }

      # 1. emutls.  __thread in an LD_PRELOADed .so is a hard crash on QNX 6.5.
      n=$($NM "$O/libapac_decoder.so" 2>/dev/null | grep -ci emutls || true)
      [ "$n" = 0 ] || fail "$n emutls symbols"

      # 2. no exceptions and no RTTI reached the link.
      n=$($NM "$O/libapac_decoder.so" 2>/dev/null | grep -cE "__cxa_throw|__cxa_allocate_exception|__gxx_personality|_ZTI|_ZTS|_ZTV" || true)
      [ "$n" = 0 ] || fail "$n exception/RTTI symbols"

      # 3. nothing from the C++ runtime or the STL: no operator new, no
      #    std::anything, no static-init guards (which would need __cxa_guard).
      n=$($NM -D --undefined-only "$O/libapac_decoder.so" 2>/dev/null \
            | grep -cE "_Znw|_Znа|_Zna|_ZdlPv|_ZdaPv|__cxa_guard|_ZSt|_ZNSt" || true)
      [ "$n" = 0 ] || fail "$n C++ runtime/STL symbols"

      # 4. the only libraries it may need are libc and libm.  Print what is left
      #    so a new dependency shows up in the log instead of at load time.
      echo "  undefined:$($NM -D --undefined-only "$O/libapac_decoder.so" | sed "s/^ *//" | awk "\$1 == \"U\" { printf \" %s\", \$2 }")"
      echo "  emutls=0  exceptions=0  rtti=0  stl=0"
    '
done

if [ "$QEMU" = 1 ]; then
    V="${VARIANTS[0]}"
    O="$OUT/$V"
    echo "=== cross-building the tests for armle-v7 (GCC $V) ==="
    docker run --rm --platform=linux/amd64 -v "$ROOT":/src -w /src/apac_decoder \
        "qnx65-armv7-toolchain:$V" bash -c '
          set -e
          export PATH=/opt/qnx650/host/linux/x86/usr/bin:$PATH
          export QNX_HOST=/opt/qnx650/host/linux/x86 QNX_TARGET=/opt/qnx650/target/qnx6
          O=.build-qnx/'"$V"'
          for t in tests/test_*.cpp; do
              n=$(basename "$t" .cpp)
              # -fno-threadsafe-statics: a function-local static with a dynamic
              # initializer (tests/test_frames.cpp) otherwise needs
              # __cxa_guard_acquire, which lives in libstdc++ and which the port
              # is not allowed to want.  The tests are single-threaded, and the
              # port itself is checked for __cxa_guard by the assertions above.
              arm-unknown-nto-qnx6.5.0eabi-g++ '"$CXXFLAGS"' -fno-threadsafe-statics -c "$t" -o "$O/$n.to"
              # Link with gcc, not g++: the port needs no C++ runtime and linking
              # with the C++ driver would drag libstdc++.so.6 onto the target.
              arm-unknown-nto-qnx6.5.0eabi-gcc "$O/$n.to" $(ls "$O"/*.o) -o "$O/$n" -lm
              n2=$(arm-unknown-nto-qnx6.5.0eabi-readelf -d "$O/$n" | grep -c libstdc || true)
              [ "$n2" = 0 ] || { echo "REJECTED: $n links libstdc++"; exit 1; }
          done
          echo "  $(ls tests/test_*.cpp | wc -l) test binaries, none needing libstdc++"
        '

    # One boot runs the whole suite: a boot costs about a minute, twelve cost
    # twelve.  The fixture each test wants is read out of the test's own source
    # rather than guessed from its name (test_frames wants frames.bin), and it
    # is handed over as an absolute /proc/boot path - the IFS is flat and the
    # cwd is not the repo.
    #
    # test_lrvq is the one test this cannot run: it opens six fixtures through
    # a hardcoded "tests/fixtures/%s" and the IFS has no subdirectories, so
    # there is nowhere to put them.  It is a host-only check; the frame mode it
    # covers is unreachable on this content anyway (see PORT_STATUS).
    #
    # test_bwe names four fixtures but takes a fixture DIRECTORY in argv[1], so
    # it does run: all four are staged and it is handed /proc/boot, which is
    # where the flat IFS puts them.  DIRARG lists the tests that work that way.
    SKIP="test_lrvq"
    DIRARG="test_bwe"
    FILES=(); RUNS=(); skipped=""
    for t in "$APAC"/tests/test_*.cpp; do
        n=$(basename "$t" .cpp)
        case " $SKIP " in *" $n "*) skipped="$skipped $n"; continue ;; esac
        FILES+=(--file "$O/$n")
        # Every distinct fixture the source names.  More than one and we cannot
        # pass them all through argv[1] unless the test takes a directory.
        fx=$(grep -oE 'tests/fixtures/[A-Za-z0-9_]+\.[A-Za-z0-9]+' "$t" | sort -u || true)
        nfx=$(printf '%s\n' "$fx" | grep -c . || true)
        case " $DIRARG " in
            *" $n "*)
                for f in $fx; do FILES+=(--file "$APAC/$f"); done
                RUNS+=(--run "$n /proc/boot")
                continue ;;
        esac
        case "$nfx" in
            0) RUNS+=(--run "$n") ;;
            1) FILES+=(--file "$APAC/$fx")
               RUNS+=(--run "$n /proc/boot/$(basename "$fx")") ;;
            *) echo "  $n names several fixtures; add it to SKIP or DIRARG"; exit 1 ;;
        esac
    done
    [ -z "$skipped" ] || echo "  not run on the target:$skipped"

    echo "=== the APAC test suite on QNX 6.5 armle-v7 under QEMU (GCC $V) ==="
    LOG="$OUT/qemu-suite.log"
    set +e
    "$HERE/qnx_qemu_run.sh" --variant "$V" ${LIBM_OVERRIDE:+--libm "$LIBM_OVERRIDE"} \
        "${FILES[@]}" "${RUNS[@]}" 2>&1 | tee "$LOG"
    set -e

    # Two tests miss their tolerance on the target and neither is a port bug;
    # both are measured and written up in PORT_STATUS.md, stage i.  Anything
    # else that fails is new and fails this script.
    #
    #   test_tns   TNS::Apply, 1.67e-6 of full scale against a 1.53e-6 tolerance.
    #              Pure FMA contraction: arm64 fuses a*b+c to one rounding, the
    #              target is VFPv3-d16 with no FMA at all.  Rebuilding the HOST
    #              test with -ffp-contract=off reproduces the target's bits
    #              exactly (3f04a9ea at index 179), so nothing here is ARM- or
    #              QNX-specific and nothing is wrong with the port.
    #   test_lpc   get_LSFWeightingFactors, 9.3e-10 absolute on a 1.9e-3 peak
    #              against a 7.7e-10 tolerance.  NOT contraction - the host with
    #              -ffp-contract=off passes.  It is QNX libm cos/acos differing
    #              from glibc/arm64 by about an ULP.
    #
    # Both are far under an S16 LSB (3.05e-5 of full scale): 1/18 of one and
    # 1/32000 of one.
    EXPECTED="test_tns test_lpc"
    bad=0
    for t in "$APAC"/tests/test_*.cpp; do
        n=$(basename "$t" .cpp)
        case " $SKIP " in *" $n "*) continue ;; esac
        if grep -q "Process [0-9]* ($n) exited status=0\." "$LOG"; then continue; fi
        case " $EXPECTED " in
            *" $n "*) echo "  $n: known ARMv7 tolerance delta, see PORT_STATUS stage i" ;;
            *) echo "  $n: FAILED on the target"; bad=1 ;;
        esac
    done
    [ "$bad" = 0 ] || { echo "REJECTED: a test that used to pass on the target does not"; exit 1; }
    echo "  every test either passed on QNX/armle-v7 or is a recorded tolerance delta"
fi
