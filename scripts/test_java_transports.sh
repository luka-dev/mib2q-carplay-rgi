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

# Reroute bug hunt (non-fatal): real RendererServer vs a fake renderer peer through
# CLEAR/MANEUVER/PROGRESS bursts, late/out-of-order ACKs and a stalled writer.
# Prints "REPRODUCED <case>" or "PASS" and never aborts the rest of this script.
mkdir -p "$TEST_DIR/reroute-transport"
if "$TEST_JDK/bin/javac" -encoding UTF-8 -cp "$PROJECT_DIR/build/carplay_hook.jar" \
        -d "$TEST_DIR/reroute-transport" "$PROJECT_DIR/tests/RendererRerouteTransportTest.java"; then
    "$TEST_JDK/bin/java" -cp "$TEST_DIR/reroute-transport:$PROJECT_DIR/build/carplay_hook.jar" \
        com.luka.carplay.rgd.RendererRerouteTransportTest \
        || echo "RendererRerouteTransportTest: FAILED (non-fatal)"
else
    echo "RendererRerouteTransportTest: BUILD FAILED (non-fatal)"
fi

# NAVSD INITIALIZING/READY must pass straight through the RGI gate
# (the altscreen INITIALIZING takeover and AltScreenStartupTest do not apply here).
STOCK_JAR="$PROJECT_DIR/../../Tools/jxe2jar/out/MU1316-final.jar"
mkdir -p "$TEST_DIR/nav-init"
"$TEST_JDK/bin/javac" -encoding UTF-8 \
    -cp "$PROJECT_DIR/build/carplay_hook.jar:$STOCK_JAR" \
    -d "$TEST_DIR/nav-init" \
    "$PROJECT_DIR/tests/GatedCombiServiceInitStateTest.java"
"$TEST_JDK/bin/java" \
    -cp "$TEST_DIR/nav-init:$PROJECT_DIR/build/carplay_hook.jar:$STOCK_JAR" \
    com.luka.carplay.rgd.GatedCombiServiceInitStateTest

# Exercise the actual lifecycle worker against controllable external modules.
mkdir -p "$TEST_DIR/lifecycle"
"$TEST_JDK/bin/javac" -encoding UTF-8 -d "$TEST_DIR/lifecycle" \
    "$PROJECT_DIR/java_patch/com/luka/carplay/core/CarPlayApp.java" \
    "$PROJECT_DIR/java_patch/com/luka/carplay/core/Module.java" \
    "$PROJECT_DIR/tests/CarPlayAppLifecycleTest.java" \
    "$PROJECT_DIR/tests/stubs/app-lifecycle/com/luka/carplay/core/LifecycleFixtures.java" \
    "$PROJECT_DIR/tests/stubs/app-lifecycle/com/luka/carplay/bus/CarplayBus.java" \
    "$PROJECT_DIR/tests/stubs/app-lifecycle/com/luka/carplay/framework/Log.java" \
    "$PROJECT_DIR/tests/stubs/app-lifecycle/com/luka/carplay/pdc/PdcSmallStageGuard.java" \
    "$PROJECT_DIR/tests/stubs/app-lifecycle/de/audi/app/terminalmode/IContext.java" \
    "$PROJECT_DIR/tests/stubs/app-lifecycle/de/audi/atip/base/IFrameworkAccess.java"
for scenario in publication during-start replug failure bounce; do
    "$TEST_JDK/bin/java" -cp "$TEST_DIR/lifecycle" com.luka.carplay.core.CarPlayAppLifecycleTest "$scenario"
done
# Parking resource policy also uses the freshly built shipping JAR.
bash "$PROJECT_DIR/scripts/test_pdc.sh"

