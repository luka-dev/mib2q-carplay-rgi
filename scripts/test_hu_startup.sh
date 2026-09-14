#!/bin/bash
set -euo pipefail
PROJECT_DIR=$(cd "$(dirname "$0")/.." && pwd)
TOOLS_DIR="$PROJECT_DIR/../../Tools/jxe2jar"
TEST_JDK="$TOOLS_DIR/jvms/zulu8.78.0.19-ca-jdk8.0.412-macosx_aarch64/zulu-8.jdk/Contents/Home"
TEST_DIR=$(mktemp -d)
trap 'rm -rf "$TEST_DIR"' EXIT
STOCK_JAR="$TOOLS_DIR/out/MU1316-final.jar"
REFERENCE="$TOOLS_DIR/out/MU1316-vf/de/audi/app/terminalmode/device/TMDeviceControl.java"
ASM_CP="$TOOLS_DIR/tools/uninline/lib/asm-9.7.jar:$TOOLS_DIR/tools/uninline/lib/asm-tree-9.7.jar:$TOOLS_DIR/tools/uninline/lib/asm-analysis-9.7.jar"
# ASM is a host-only verifier; production is entirely compiled from Java source.
bash "$PROJECT_DIR/scripts/build_java.sh"
mkdir -p "$TEST_DIR/reference-classes"
python3 "$PROJECT_DIR/tests/recover_tmdevice_reference.py" \
    "$REFERENCE" "$TEST_DIR/TMDeviceControl.java"
"$TEST_JDK/bin/javac" -source 1.4 -target 1.4 -Xlint:-options -cp "$STOCK_JAR" \
    -d "$TEST_DIR/reference-classes" "$TEST_DIR/TMDeviceControl.java"
"$TEST_JDK/bin/jar" cf "$TEST_DIR/reference.jar" -C "$TEST_DIR/reference-classes" .
"$TEST_JDK/bin/javac" -cp "$ASM_CP" -d "$TEST_DIR" "$PROJECT_DIR/tests/TMDeviceControlReplacementTest.java"
"$TEST_JDK/bin/java" -cp "$TEST_DIR:$ASM_CP" TMDeviceControlReplacementTest \
    "$TOOLS_DIR/out/MU1316-combined.jar" "$PROJECT_DIR/build/carplay_hook.jar" "$TEST_DIR/reference.jar"
rg --files "$PROJECT_DIR/tests/stubs/hu-startup" -g '*.java' > "$TEST_DIR/sources"
printf '%s\n' "$PROJECT_DIR/java_patch/com/luka/carplay/startup/MenuIdentityState.java" \
    "$PROJECT_DIR/java_patch/com/luka/carplay/startup/HuMenuIdentity.java" \
    "$PROJECT_DIR/java_patch/com/luka/carplay/cursor/HuStartup.java" \
    "$PROJECT_DIR/java_patch/com/luka/carplay/cursor/CursorWire.java" \
    "$PROJECT_DIR/tests/HuStartupTest.java" >> "$TEST_DIR/sources"
"$TEST_JDK/bin/javac" -source 1.4 -target 1.4 -Xlint:-options -d "$TEST_DIR" @"$TEST_DIR/sources"
"$TEST_JDK/bin/java" -cp "$TEST_DIR" com.luka.carplay.cursor.HuStartupTest
# Exercise the real HuCursor + NativeCursorOverlay classes, beyond pure-state stubs.
mkdir -p "$TEST_DIR/owner"
OWNER_CP="$PROJECT_DIR/build/carplay_hook.jar:$TOOLS_DIR/out/MU1316-combined.jar"
"$TEST_JDK/bin/javac" -cp "$OWNER_CP" -d "$TEST_DIR/owner" \
    "$PROJECT_DIR/tests/stubs/cursor-owner/com/luka/carplay/core/CarPlayApp.java" "$PROJECT_DIR/tests/HuCursorLifecycleTest.java"
"$TEST_JDK/bin/java" -cp "$TEST_DIR/owner:$OWNER_CP" com.luka.carplay.cursor.HuCursorLifecycleTest
cc -std=gnu99 -Wall -Wextra -Werror -pthread -fsanitize=address,undefined \
    "$PROJECT_DIR/tests/hu_video_observer_test.c" -o "$TEST_DIR/hu_video_observer_test"
"$TEST_DIR/hu_video_observer_test"
