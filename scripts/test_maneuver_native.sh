#!/bin/bash
# Headless CPU regression test linked to the actual macOS renderer sources.
set -euo pipefail
PROJECT_DIR=$(cd "$(dirname "$0")/.." && pwd)
TEST_DIR=$(mktemp -d)
trap 'rm -rf "$TEST_DIR"' EXIT
cd "$PROJECT_DIR"
cc -O1 -g -std=c99 -Wall -Wextra -Icommon -fsanitize=address,undefined \
    -fno-omit-frame-pointer tests/lane_guidance_test.c -o "$TEST_DIR/lane_guidance_test"
"$TEST_DIR/lane_guidance_test"
SCENE_OBJECTS=()
for source in scene geometry layout lane_panel; do
    c++ -O1 -g -std=c++11 -fno-exceptions -fno-rtti -DPLATFORM_MACOS -Icommon -I/opt/homebrew/include \
        -fsanitize=address,undefined -fno-omit-frame-pointer \
        -c "maneuver_render/scene/$source.cpp" -o "$TEST_DIR/$source.o"
    SCENE_OBJECTS+=("$TEST_DIR/$source.o")
done
cc -O1 -g -std=c99 -Wall -Wextra -DPLATFORM_MACOS -Icommon -I/opt/homebrew/include \
    -fsanitize=address,undefined -fno-omit-frame-pointer \
    tests/maneuver_parity_test.c maneuver_render/route_path.c \
    maneuver_render/render.c maneuver_render/server.c maneuver_render/platform_macos.c \
    "${SCENE_OBJECTS[@]}" \
    -L/opt/homebrew/lib -lglfw -framework OpenGL -lm -o "$TEST_DIR/maneuver_parity_test"
"$TEST_DIR/maneuver_parity_test"
cc -O1 -g -std=c99 -Wall -Wextra -Icommon -fsanitize=address,undefined \
    -fno-omit-frame-pointer tests/renderer_peer_loss_test.c -o "$TEST_DIR/renderer_peer_loss_test"
"$TEST_DIR/renderer_peer_loss_test"
