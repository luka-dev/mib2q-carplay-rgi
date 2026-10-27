#!/bin/bash
# Real native renderer command loop; only GL/platform and command-source leaves are simulated.
# A failed sequence or failed build is fatal, just like the other regression suites.
set -euo pipefail
PROJECT_DIR=$(cd "$(dirname "$0")/.." && pwd)
TEST_DIR=$(mktemp -d)
trap 'rm -rf "$TEST_DIR"' EXIT
cc -std=gnu99 -O1 -Wall -Werror -Wno-unused-function -Wno-unused-variable \
    -I"$PROJECT_DIR/maneuver_render" -I"$PROJECT_DIR/common" \
    "$PROJECT_DIR/tests/maneuver_reroute_clear_test.c" -lpthread -lm \
    -o "$TEST_DIR/maneuver_reroute_clear"
"$TEST_DIR/maneuver_reroute_clear"
