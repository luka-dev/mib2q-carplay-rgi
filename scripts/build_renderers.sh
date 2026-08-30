#!/bin/bash
# Build the QNX/ARMv7 cluster maneuver renderer in Docker.
#
# Uses the self-contained image `qnx65-armv7-toolchain` (Tools/qnx-65-sdp-docker,
# GCC 4.9.4).
#
#   ./scripts/build_renderers.sh            # build maneuver_render
#   ./scripts/build_renderers.sh grid       # maneuver_render with -DCR_DEBUG_GRID
#
# Outputs: build/maneuver_render  (written via the mount).
#
# BSP note: libscreen / libEGL / libGLESv2 export their client API only on the unit
# (generic SDP lacks them).  We synthesize IMPORT STUBS (symbol names, empty bodies,
# correct SONAME) so the link resolves; at runtime the binary binds to the unit's real
# libs.  `--allow-shlib-undefined` defers each BSP lib's own transitive deps to the unit.
set -e

[ "$#" -le 1 ] || { echo "usage: ./scripts/build_renderers.sh [grid]"; exit 2; }
[ "$#" -eq 0 ] || [ "$1" = "grid" ] || { echo "usage: ./scripts/build_renderers.sh [grid]"; exit 2; }

IMG=qnx65-armv7-toolchain:latest
SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
PROJECT_DIR="$(cd "$SCRIPT_DIR/.." && pwd)"
GRID=""
[ "$1" = "grid" ] && GRID="-DCR_DEBUG_GRID"

if ! docker image inspect "$IMG" >/dev/null 2>&1; then
    echo "ERROR: docker image '$IMG' not found."
    echo "Build it once:  docker build --platform=linux/amd64 -t $IMG $PROJECT_DIR/../../Tools/qnx-65-sdp-docker"
    exit 1
fi

echo "=== Cluster Renderer Build (Docker $IMG) ==="

docker run --rm --platform=linux/amd64 -v "$PROJECT_DIR":/src "$IMG" bash -c '
  set -e
  export PATH=/opt/qnx650/host/linux/x86/usr/bin:$PATH
  export QNX_HOST=/opt/qnx650/host/linux/x86 QNX_TARGET=/opt/qnx650/target/qnx6
  CC=arm-unknown-nto-qnx6.5.0eabi-gcc
  ABI_INCLUDE=/src/toolchain/qnx65-abi/include
  GRID="'"$GRID"'"

  # gen_stub <soname> <symbol-regex> <src-files...>  -> /tmp/<soname>
  gen_stub(){ local so="$1" rx="$2"; shift 2
    grep -rhoE "$rx" "$@" 2>/dev/null | sort -u | sed "s/.*/int &(){return 0;}/" > /tmp/st_$so.c
    $CC -shared -fPIC -Wl,-soname,"$so" /tmp/st_$so.c -o /tmp/"$so"
  }

  echo "--- maneuver_render ---"
  cd /src/maneuver_render
  MR_SRCS="main.c render.c maneuver.c route_path.c server.c platform_qnx.c ../common/cluster_surface.c"
  gen_stub libscreen.so.1  "\bscreen_[a-z_]+"      $MR_SRCS
  gen_stub libEGL.so.1     "\begl[A-Z][A-Za-z0-9]+" $MR_SRCS
  gen_stub libGLESv2.so.1  "\bgl[A-Z][A-Za-z0-9]+"  $MR_SRCS
  $CC -O2 -std=gnu99 -Wall -D__QNX__ -DPLATFORM_QNX -fdata-sections -ffunction-sections $GRID \
      -I. -I../common -I"$ABI_INCLUDE" $MR_SRCS \
      -o /src/build/maneuver_render \
      -Wl,--gc-sections -Wl,--allow-shlib-undefined \
      -L/tmp -l:libscreen.so.1 -l:libEGL.so.1 -l:libGLESv2.so.1 -lsocket -lm
  echo "  built build/maneuver_render"

  echo "--- verify (ARM ELF, no emutls) ---"
  for b in maneuver_render; do
    B=/src/build/$b
    m=$(arm-unknown-nto-qnx6.5.0eabi-readelf -h "$B" | awk -F: "/Machine/{print \$2}" | tr -d " ")
    e=$(arm-unknown-nto-qnx6.5.0eabi-nm "$B" 2>/dev/null | grep -ci emutls || true)
    echo "  $b: machine=$m emutls=$e"
  done
'

echo ""
echo "Done:"
ls -lh "$PROJECT_DIR/build/maneuver_render"
