#!/bin/bash
# Headless CPU regression test linked to the actual macOS renderer sources.
set -euo pipefail
PROJECT_DIR=$(cd "$(dirname "$0")/.." && pwd)
TEST_DIR=$(mktemp -d)
trap 'rm -rf "$TEST_DIR"' EXIT
cd "$PROJECT_DIR"
cc -O1 -g -std=c99 -Wall -Wextra -fsanitize=address,undefined \
    -fno-omit-frame-pointer tests/lane_safety_test.c -o "$TEST_DIR/lane_safety_test"
"$TEST_DIR/lane_safety_test"
cc -O1 -g -std=c99 -Wall -Wextra -DPLATFORM_MACOS -I/opt/homebrew/include \
    -fsanitize=address,undefined -fno-omit-frame-pointer \
    tests/maneuver_parity_test.c maneuver_render/route_path.c \
    maneuver_render/render.c maneuver_render/server.c maneuver_render/platform_macos.c \
    -L/opt/homebrew/lib -lglfw -framework OpenGL -lm -o "$TEST_DIR/maneuver_parity_test"
"$TEST_DIR/maneuver_parity_test"
cc -O1 -g -std=c99 -Wall -Wextra -fsanitize=address,undefined \
    -fno-omit-frame-pointer tests/renderer_peer_loss_test.c -o "$TEST_DIR/renderer_peer_loss_test"
"$TEST_DIR/renderer_peer_loss_test"
