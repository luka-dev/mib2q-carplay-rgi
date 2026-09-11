#!/bin/bash
# Compile shipping Java 1.4 sources, then exercise local transports off the HU ports.
set -euo pipefail
PROJECT_DIR=$(cd "$(dirname "$0")/.." && pwd)
TEST_JDK="$PROJECT_DIR/../../Tools/jxe2jar/jvms/zulu8.78.0.19-ca-jdk8.0.412-macosx_aarch64/zulu-8.jdk/Contents/Home"
bash "$PROJECT_DIR/scripts/build_java.sh"
TEST_DIR=$(mktemp -d)
trap 'rm -rf "$TEST_DIR"' EXIT
"$TEST_JDK/bin/javac" -encoding UTF-8 -cp "$PROJECT_DIR/build/carplay_hook.jar" -d "$TEST_DIR" \
    "$PROJECT_DIR/tests/CarplayBusTransportTest.java" \
    "$PROJECT_DIR/tests/RendererServerTransportTest.java"
"$TEST_JDK/bin/java" -cp "$TEST_DIR:$PROJECT_DIR/build/carplay_hook.jar" com.luka.carplay.bus.CarplayBusTransportTest
"$TEST_JDK/bin/java" -cp "$TEST_DIR:$PROJECT_DIR/build/carplay_hook.jar" com.luka.carplay.rgd.RendererServerTransportTest
# Isolate the native-HMI callback, while running the actual control-server source.
mkdir -p "$TEST_DIR/altctl"
"$TEST_JDK/bin/javac" -encoding UTF-8 -cp "$PROJECT_DIR/build/carplay_hook.jar" -d "$TEST_DIR/altctl" \
    "$PROJECT_DIR/java_patch/com/luka/carplay/core/AltscreenControlServer.java" \
    "$PROJECT_DIR/tests/stubs/com/luka/carplay/core/AltScreenModule.java" \
    "$PROJECT_DIR/tests/AltscreenControlTransportTest.java"
"$TEST_JDK/bin/java" -cp "$TEST_DIR/altctl:$PROJECT_DIR/build/carplay_hook.jar" com.luka.carplay.core.AltscreenControlTransportTest
