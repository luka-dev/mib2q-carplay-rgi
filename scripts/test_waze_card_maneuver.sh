#!/bin/bash
# Waze card class -> HUD arrow (WazeCardManeuver).  The shipping class is
# compiled at the shipping level (1.4) with ManeuverMapper, its only
# dependency; the test checks the HUD picture set and parses the
# card_rec_class_t enum in altscreen_render/card_rec.h.  No stock JARs.
set -euo pipefail
PROJECT_DIR=$(cd "$(dirname "$0")/.." && pwd)
. "$PROJECT_DIR/scripts/java_env.sh"
RGD="$PROJECT_DIR/java_patch/com/luka/carplay/rgd"
TEST_DIR=$(mktemp -d)
trap 'rm -rf "$TEST_DIR"' EXIT
mkdir "$TEST_DIR/patch" "$TEST_DIR/test"
"$JAVA8_HOME/bin/javac" -source 1.4 -target 1.4 -Xlint:-options -encoding UTF-8 \
    -cp "$TEST_DIR/patch" -d "$TEST_DIR/patch" \
    "$RGD/WazeCardManeuver.java" "$RGD/ManeuverMapper.java"
"$JAVA8_HOME/bin/javac" -encoding UTF-8 -cp "$TEST_DIR/patch" -d "$TEST_DIR/test" \
    "$PROJECT_DIR/tests/WazeCardManeuverTest.java"
"$JAVA8_HOME/bin/java" -cp "$TEST_DIR/test:$TEST_DIR/patch" WazeCardManeuverTest \
    "$PROJECT_DIR/altscreen_render/card_rec.h"
