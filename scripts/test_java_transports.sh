#!/bin/bash
# Compile shipping Java 1.4 sources, then exercise the local transports.
set -euo pipefail
PROJECT_DIR=$(cd "$(dirname "$0")/.." && pwd)
. "$PROJECT_DIR/scripts/java_env.sh"; TEST_JDK=$JAVA8_HOME
bash "$PROJECT_DIR/scripts/build_java.sh"
TEST_DIR=$(mktemp -d)
trap 'rm -rf "$TEST_DIR"' EXIT
"$TEST_JDK/bin/javac" -encoding UTF-8 -cp "$PROJECT_DIR/build/carplay_hook.jar" -d "$TEST_DIR" \
    "$PROJECT_DIR/tests/CarplayBusTransportTest.java" \
    "$PROJECT_DIR/tests/RendererServerTransportTest.java" \
    "$PROJECT_DIR/tests/RendererRerouteTransportTest.java" \
    "$PROJECT_DIR/tests/TouchpadControllerTest.java"
"$TEST_JDK/bin/java" -cp "$TEST_DIR:$PROJECT_DIR/build/carplay_hook.jar" TouchpadControllerTest
"$TEST_JDK/bin/java" -cp "$TEST_DIR:$PROJECT_DIR/build/carplay_hook.jar" com.luka.carplay.bus.CarplayBusTransportTest
"$TEST_JDK/bin/java" -cp "$TEST_DIR:$PROJECT_DIR/build/carplay_hook.jar" com.luka.carplay.rgd.RendererServerTransportTest
"$TEST_JDK/bin/javac" -encoding UTF-8 -cp "$PROJECT_DIR/build/carplay_hook.jar:$PROJECT_DIR/java_stock/US_MU1316_lsd.jar" \
    -d "$TEST_DIR" "$PROJECT_DIR/tests/VehicleDataEncodingTest.java"
"$TEST_JDK/bin/java" -cp "$TEST_DIR:$PROJECT_DIR/build/carplay_hook.jar:$PROJECT_DIR/java_stock/US_MU1316_lsd.jar" \
    com.luka.carplay.core.VehicleDataEncodingTest
"$TEST_JDK/bin/javac" -encoding UTF-8 -cp "$PROJECT_DIR/build/carplay_hook.jar:$PROJECT_DIR/java_stock/US_MU1316_lsd.jar" \
    -d "$TEST_DIR" "$PROJECT_DIR/tests/UpmixSettingTest.java"
"$TEST_JDK/bin/java" -cp "$TEST_DIR:$PROJECT_DIR/build/carplay_hook.jar:$PROJECT_DIR/java_stock/US_MU1316_lsd.jar" \
    com.luka.carplay.core.UpmixSettingTest
# Cockpit map modes: stored mode, cluster context per mode, pin/ownership, steering-wheel shortcut.
"$TEST_JDK/bin/javac" -encoding UTF-8 -nowarn -cp "$PROJECT_DIR/build/carplay_hook.jar:$PROJECT_DIR/java_stock/US_MU1316_lsd.jar:$OSGI_CP" \
    -d "$TEST_DIR" "$PROJECT_DIR/tests/CockpitMapModeTest.java"
for variant in us eu cn; do
    case "$variant" in
        us) patch=carplay_hook.jar; stock=US_MU1316_lsd.jar ;;
        eu) patch=carplay_hook.jar; stock=EU_MU1329_lsd.jar ;;
        cn) patch=carplay_hook_cn.jar; stock=CN_MU1003_lsd.jar ;;
    esac
    "$TEST_JDK/bin/java" -cp "$TEST_DIR:$PROJECT_DIR/build/$patch:$PROJECT_DIR/java_stock/$stock:$OSGI_CP" CockpitMapModeTest
done
for variant in us eu cn; do
    case "$variant" in
        us) patch=carplay_hook.jar; stock=US_MU1316_lsd.jar ;;
        eu) patch=carplay_hook.jar; stock=EU_MU1329_lsd.jar ;;
        cn) patch=carplay_hook_cn.jar; stock=CN_MU1003_lsd.jar ;;
    esac
    echo "Renderer reroute transport: $variant"
    "$TEST_JDK/bin/java" -cp "$TEST_DIR:$PROJECT_DIR/build/$patch:$PROJECT_DIR/java_stock/$stock" \
        com.luka.carplay.rgd.RendererRerouteTransportTest
done

# AltScreen startup scenarios and the context-switch race.
STOCK_JAR="$PROJECT_DIR/java_stock/US_MU1316_lsd.jar"
mkdir -p "$TEST_DIR/nav-init"
"$TEST_JDK/bin/javac" -encoding UTF-8 \
    -cp "$PROJECT_DIR/build/carplay_hook.jar:$STOCK_JAR" \
    -d "$TEST_DIR/nav-init" \
    "$PROJECT_DIR/tests/AltScreenStartupTest.java" \
    "$PROJECT_DIR/tests/AltScreenContextRaceTest.java"
for scenario in cold warm stop bounce-loss; do
    "$TEST_JDK/bin/java" \
        -cp "$TEST_DIR/nav-init:$PROJECT_DIR/build/carplay_hook.jar:$STOCK_JAR" \
        com.luka.carplay.core.AltScreenStartupTest "$scenario"
done
for variant in us eu cn; do
    case "$variant" in
        us) patch=carplay_hook.jar; stock=US_MU1316_lsd.jar ;;
        eu) patch=carplay_hook.jar; stock=EU_MU1329_lsd.jar ;;
        cn) patch=carplay_hook_cn.jar; stock=CN_MU1003_lsd.jar ;;
    esac
    echo "Context regressions: $variant"
    for scenario in stop change peer readiness reconnect direct target-error rate-error stock-error bounce-error drift; do
        "$TEST_JDK/bin/java" \
            -cp "$TEST_DIR/nav-init:$PROJECT_DIR/build/$patch:$PROJECT_DIR/java_stock/$stock" \
            com.luka.carplay.core.AltScreenContextRaceTest "$scenario"
    done
done
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
    "$PROJECT_DIR/tests/stubs/app-lifecycle/com/luka/carplay/core/UpmixSetting.java" \
    "$PROJECT_DIR/tests/stubs/app-lifecycle/com/luka/carplay/core/CockpitMapSetting.java" \
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
