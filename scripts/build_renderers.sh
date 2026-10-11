#!/bin/bash
# Build the QNX/ARMv7 cluster maneuver renderer in Docker.
#
# Uses the self-contained image `qnx65-armv7-toolchain`
# (https://github.com/luka-dev/qnx65-armv7-toolchain, GCC 8.5).
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

IMG=${QNX_TOOLCHAIN_IMAGE:-qnx65-armv7-toolchain:latest}
SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
PROJECT_DIR="$(cd "$SCRIPT_DIR/.." && pwd)"
# under Git for Windows hand Docker a Windows path (MSYS maps /tmp to the user's
# temp directory, which Docker would read as its own /tmp) and stop MSYS rewriting the
# container paths in docker's arguments.
case "$(uname -s)" in
    MINGW*|MSYS*|CYGWIN*) PROJECT_DIR="$(cygpath -m "$PROJECT_DIR")"; export MSYS_NO_PATHCONV=1 ;;
esac
GRID=""
[ "$1" = "grid" ] && GRID="-DCR_DEBUG_GRID"

if ! docker image inspect "$IMG" >/dev/null 2>&1; then
    echo "ERROR: docker image '$IMG' not found."
    echo "Build it once from https://github.com/luka-dev/qnx65-armv7-toolchain :  ./host-scripts/qnx-run.sh build"
    exit 1
fi

echo "=== Cluster Renderer Build (Docker $IMG) ==="

docker run --rm --platform=linux/amd64 -v "$PROJECT_DIR":/host "$IMG" bash -c '
  set -e
  export PATH=/opt/qnx650/host/linux/x86/usr/bin:$PATH
  export QNX_HOST=/opt/qnx650/host/linux/x86 QNX_TARGET=/opt/qnx650/target/qnx6
  # build inside the container at the same /src paths.  A Docker Desktop (Windows)
  # bind mount reports 64-bit inode numbers that the 32-bit QNX binutils reject (EOVERFLOW).
  mkdir -p /src && cp -a /host/common /host/maneuver_render /host/toolchain /src/
  CC=arm-unknown-nto-qnx6.5.0eabi-gcc
  CXX=arm-unknown-nto-qnx6.5.0eabi-g++
  AR=arm-unknown-nto-qnx6.5.0eabi-ar
  mkdir -p /src/build/maneuver-scene-qnx
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
  SCENE_OBJECTS=""
  for source in scene/scene.cpp scene/geometry.cpp scene/layout.cpp scene/lane_panel.cpp; do
    object=/src/build/maneuver-scene-qnx/$(basename "$source" .cpp).o
    $CXX -O2 -std=c++11 -Wall -Wextra -fno-exceptions -fno-rtti \
        -D__QNX__ -DPLATFORM_QNX -fdata-sections -ffunction-sections $GRID \
        -I. -I../common -I"$ABI_INCLUDE" -c "$source" -o "$object"
    SCENE_OBJECTS="$SCENE_OBJECTS $object"
  done
  rm -f /src/build/libmaneuver_scene.a
  $AR rcs /src/build/libmaneuver_scene.a $SCENE_OBJECTS
  $CC -O2 -std=gnu99 -Wall -D__QNX__ -DPLATFORM_QNX -fdata-sections -ffunction-sections $GRID \
      -I. -I../common -I"$ABI_INCLUDE" $MR_SRCS $SCENE_OBJECTS \
      -o /src/build/maneuver_render \
      -Wl,--gc-sections -Wl,--allow-shlib-undefined \
      -L/tmp -l:libscreen.so.1 -l:libEGL.so.1 -l:libGLESv2.so.1 -lsocket -lm
  if arm-unknown-nto-qnx6.5.0eabi-nm -u $SCENE_OBJECTS | grep -E "(__cxa|_ZSt|_ZTI|_ZTV|_Zn[aw]|_Zd[al]|gxx_personality)"; then
    echo "Unexpected C++ runtime dependency in scene engine"; exit 1
  fi
  echo "  built build/maneuver_render + build/libmaneuver_scene.a (C ABI, no C++ runtime)"

  echo "--- verify (ARM ELF, no emutls) ---"
  for b in maneuver_render; do
    B=/src/build/$b
    m=$(arm-unknown-nto-qnx6.5.0eabi-readelf -h "$B" | awk -F: "/Machine/{print \$2}" | tr -d " ")
    e=$(arm-unknown-nto-qnx6.5.0eabi-nm "$B" 2>/dev/null | grep -ci emutls || true)
    echo "  $b: machine=$m emutls=$e"
    [ "$m" = ARM ] && [ "$e" = 0 ] || exit 1
  done
  mkdir -p /host/build && cp /src/build/maneuver_render /src/build/libmaneuver_scene.a /host/build/
'

echo ""
echo "Done:"
ls -lh "$PROJECT_DIR/build/maneuver_render"
