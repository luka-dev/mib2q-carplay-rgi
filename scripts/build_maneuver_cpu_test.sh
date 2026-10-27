#!/bin/bash
# Build the existing lane/scene CPU regression for the actual QNX runtime.
# No Screen/EGL window or renderer socket; safe to run alongside CarPlay.
# Output: build/maneuver_qnx_cpu_test (exits after 10 s if target code hangs).
set -euo pipefail
PROJECT_DIR=$(cd "$(dirname "$0")/.." && pwd)
docker run --rm --platform=linux/amd64 -v "$PROJECT_DIR":/src \
    qnx65-armv7-toolchain:latest bash -c '
set -e
export PATH=/opt/qnx650/host/linux/x86/usr/bin:$PATH
export QNX_HOST=/opt/qnx650/host/linux/x86 QNX_TARGET=/opt/qnx650/target/qnx6
CC=arm-unknown-nto-qnx6.5.0eabi-gcc
CXX=arm-unknown-nto-qnx6.5.0eabi-g++
OUT=/src/build/maneuver-qnx-cpu-test
mkdir -p "$OUT"
FLAGS="-O2 -D__QNX__ -DPLATFORM_QNX -ffunction-sections -fdata-sections -I/src/maneuver_render -I/src/toolchain/qnx65-abi/include"
for name in scene geometry layout lane_panel; do
    $CXX $FLAGS -std=c++11 -fno-exceptions -fno-rtti \
        -c /src/maneuver_render/scene/$name.cpp -o "$OUT/$name.o"
done
for name in render route_path maneuver; do
    $CC $FLAGS -std=gnu99 -c /src/maneuver_render/$name.c -o "$OUT/$name.o"
done
$CXX $FLAGS -std=c++11 -fno-exceptions -fno-rtti \
    -c /src/tools/maneuver-preview/test_lane_panel.cpp -o "$OUT/test.o"
# CPU-only reachability: unused GPU functions and their imports are discarded.
$CC "$OUT/test.o" "$OUT/scene.o" "$OUT/geometry.o" "$OUT/layout.o" \
    "$OUT/lane_panel.o" "$OUT/render.o" "$OUT/route_path.o" "$OUT/maneuver.o" \
    -Wl,--gc-sections -lm -o /src/build/maneuver_qnx_cpu_test
'
echo "Built build/maneuver_qnx_cpu_test; run on QNX to validate target math and framing."
