#!/bin/bash
# Compile shipping Java 1.4 sources, then exercise input and local transports.
set -euo pipefail
PROJECT_DIR=$(cd "$(dirname "$0")/.." && pwd)
TEST_JDK="$PROJECT_DIR/../../Tools/jxe2jar/jvms/zulu8.78.0.19-ca-jdk8.0.412-macosx_aarch64/zulu-8.jdk/Contents/Home"
bash "$PROJECT_DIR/scripts/build_java.sh"
TEST_DIR=$(mktemp -d)
trap 'rm -rf "$TEST_DIR"' EXIT
"$TEST_JDK/bin/javac" -encoding UTF-8 -cp "$PROJECT_DIR/build/carplay_hook.jar" -d "$TEST_DIR" \
    "$PROJECT_DIR/tests/CarplayBusTransportTest.java" \
    "$PROJECT_DIR/tests/RendererServerTransportTest.java" \
    "$PROJECT_DIR/tests/TouchpadControllerTest.java"
"$TEST_JDK/bin/java" -cp "$TEST_DIR:$PROJECT_DIR/build/carplay_hook.jar" TouchpadControllerTest
"$TEST_JDK/bin/java" -cp "$TEST_DIR:$PROJECT_DIR/build/carplay_hook.jar" com.luka.carplay.bus.CarplayBusTransportTest
"$TEST_JDK/bin/java" -cp "$TEST_DIR:$PROJECT_DIR/build/carplay_hook.jar" com.luka.carplay.rgd.RendererServerTransportTest
# Isolate the native-HMI callback, while running the actual control-server source.
mkdir -p "$TEST_DIR/altctl"
"$TEST_JDK/bin/javac" -encoding UTF-8 -cp "$PROJECT_DIR/build/carplay_hook.jar" -d "$TEST_DIR/altctl" \
    "$PROJECT_DIR/java_patch/com/luka/carplay/core/AltscreenControlServer.java" \
    "$PROJECT_DIR/tests/stubs/com/luka/carplay/core/AltScreenModule.java" \
    "$PROJECT_DIR/tests/AltscreenControlTransportTest.java"
"$TEST_JDK/bin/java" -cp "$TEST_DIR/altctl:$PROJECT_DIR/build/carplay_hook.jar" com.luka.carplay.core.AltscreenControlTransportTest

# Exercise the actual lifecycle worker against controllable external modules.
mkdir -p "$TEST_DIR/lifecycle"
"$TEST_JDK/bin/javac" -encoding UTF-8 -d "$TEST_DIR/lifecycle" \
    "$PROJECT_DIR/java_patch/com/luka/carplay/core/CarPlayApp.java" \
    "$PROJECT_DIR/java_patch/com/luka/carplay/core/Module.java" \
    "$PROJECT_DIR/tests/CarPlayAppLifecycleTest.java" \
    "$PROJECT_DIR/tests/stubs/app-lifecycle/com/luka/carplay/core/LifecycleFixtures.java" \
    "$PROJECT_DIR/tests/stubs/app-lifecycle/com/luka/carplay/bus/CarplayBus.java" \
    "$PROJECT_DIR/tests/stubs/app-lifecycle/com/luka/carplay/framework/Log.java" \
    "$PROJECT_DIR/tests/stubs/app-lifecycle/de/audi/app/terminalmode/IContext.java" \
    "$PROJECT_DIR/tests/stubs/app-lifecycle/de/audi/atip/base/IFrameworkAccess.java"
for scenario in publication during-start replug failure bounce; do
    "$TEST_JDK/bin/java" -cp "$TEST_DIR/lifecycle" com.luka.carplay.core.CarPlayAppLifecycleTest "$scenario"
done

# Parking resource policy also uses the freshly built shipping JAR.
bash "$PROJECT_DIR/scripts/test_pdc.sh"
