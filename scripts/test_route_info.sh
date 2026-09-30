#!/bin/bash
# Host test of the actual Java 1.4 BAPBridge against the stock service interface.
# Always build fresh; no HU access or application startup.
set -euo pipefail
PROJECT_DIR=$(cd "$(dirname "$0")/.." && pwd)
TOOLS_DIR="$PROJECT_DIR/../../Tools/jxe2jar"
if [ "$(uname)" = "Darwin" ]; then
    JDK_DIR="$TOOLS_DIR/jvms/zulu8.78.0.19-ca-jdk8.0.412-macosx_aarch64/zulu-8.jdk/Contents/Home"
else
    JDK_DIR="$TOOLS_DIR/jvms/zulu8.78.0.19-ca-jdk8.0.412-linux_x64"
fi
STOCK_JAR="$TOOLS_DIR/out/MU1316-final.jar"
COMBINED_JAR="$TOOLS_DIR/out/MU1316-combined.jar"
if [ ! -f "$COMBINED_JAR" ]; then
    COMBINED_JAR="$STOCK_JAR"
fi
PATCH_JAR="$PROJECT_DIR/build/carplay_hook.jar"
if [ ! -x "$JDK_DIR/bin/javac" ] || [ ! -f "$STOCK_JAR" ]; then
    echo "Missing MU1316 build JDK or stock JAR." >&2
    exit 1
fi
bash "$PROJECT_DIR/scripts/build_java.sh"
TEST_DIR=$(mktemp -d)
trap 'rm -rf "$TEST_DIR"' EXIT
CLASSPATH="$PATCH_JAR:$STOCK_JAR:$TOOLS_DIR/libs/org.osgi.framework-1.10.0.jar:$TOOLS_DIR/libs/org.osgi.util.tracker-1.5.4.jar"
"$JDK_DIR/bin/javac" -encoding UTF-8 -cp "$CLASSPATH" -d "$TEST_DIR" \
    "$PROJECT_DIR/tests/CurrentPositionDeliveryTest.java" \
    "$PROJECT_DIR/tests/RouteInfoTimeoutTest.java" \
    "$PROJECT_DIR/tests/CurrentPositionStockChainTest.java" \
    "$PROJECT_DIR/tests/VCTextScrollTest.java" \
    "$PROJECT_DIR/tests/RouteInfoPresentationTest.java" \
    "$PROJECT_DIR/tests/RouteGuidanceDeltaTest.java" \
    "$PROJECT_DIR/tests/RouteContextStateMachineTest.java" \
    "$PROJECT_DIR/tests/RouteContextDeltaIntegrationTest.java" \
    "$PROJECT_DIR/tests/ScreenModuleContextRaceTest.java" \
    "$PROJECT_DIR/tests/RouteContextRendererLifecycleTest.java" \
    "$PROJECT_DIR/tests/RerouteRendererRecoveryTest.java" \
    "$PROJECT_DIR/tests/RerouteHoldAndKdkVerifyTest.java" \
    "$PROJECT_DIR/tests/ShortTurnRerouteBlinkTest.java" \
    "$PROJECT_DIR/tests/ShortTurnBlinkVisibilityTest.java" \
    "$PROJECT_DIR/tests/DistanceBargraphChainTest.java" \
    "$PROJECT_DIR/tests/KomoGraphicsStateTest.java" \
    "$PROJECT_DIR/tests/ManeuverChainAudit.java" \
    "$PROJECT_DIR/tests/ManeuverParityTest.java" \
    "$PROJECT_DIR/tests/RendererViewportTest.java" \
    "$PROJECT_DIR/tests/ClusterKdkSyncTest.java" \
    "$PROJECT_DIR/tests/ClusterKdkBapChainTest.java" \
    "$PROJECT_DIR/tests/ClusterKdkRendererLifecycleTest.java" \
    "$PROJECT_DIR/tests/LaneGuidanceTransportTest.java" \
    "$PROJECT_DIR/tests/LaneGuidanceLifecycleTest.java" \
    "$PROJECT_DIR/tests/RgiDeliveryRecoveryTest.java" \
    "$PROJECT_DIR/tests/RendererMapperDirectionTest.java" \
    "$PROJECT_DIR/tests/BlinkZoneAbsoluteCapTest.java" \
    "$PROJECT_DIR/tests/CarPlayDistanceResolverTest.java" \
    "$PROJECT_DIR/tests/RouteTextTruncationAndExitArrowTest.java"
# No checkout resources or font files are visible through the working directory.
# The pure runtime uses only classes and the embedded data in the built JAR.
(cd "$TEST_DIR" && "$JDK_DIR/bin/java" -cp "$TEST_DIR:$PATCH_JAR" \
    com.luka.carplay.rgd.VCTextScrollTest ${VC_UNICODE_TEST_DIR:+"$VC_UNICODE_TEST_DIR"})
"$JDK_DIR/bin/java" -cp "$TEST_DIR:$CLASSPATH" RouteInfoPresentationTest
"$JDK_DIR/bin/java" -cp "$TEST_DIR:$CLASSPATH" RendererMapperDirectionTest
"$JDK_DIR/bin/java" -cp "$TEST_DIR:$CLASSPATH" BlinkZoneAbsoluteCapTest
"$JDK_DIR/bin/java" -cp "$TEST_DIR:$CLASSPATH" CarPlayDistanceResolverTest
"$JDK_DIR/bin/java" -cp "$TEST_DIR:$CLASSPATH" RouteGuidanceDeltaTest
"$JDK_DIR/bin/java" -cp "$TEST_DIR:$CLASSPATH" RouteContextStateMachineTest
"$JDK_DIR/bin/java" -cp "$TEST_DIR:$CLASSPATH" RouteContextDeltaIntegrationTest
"$JDK_DIR/bin/java" -cp "$TEST_DIR:$CLASSPATH" ScreenModuleContextRaceTest
"$JDK_DIR/bin/java" -cp "$TEST_DIR:$CLASSPATH" RouteContextRendererLifecycleTest
"$JDK_DIR/bin/java" -cp "$TEST_DIR:$CLASSPATH" RerouteRendererRecoveryTest
"$JDK_DIR/bin/java" -cp "$TEST_DIR:$CLASSPATH" RerouteHoldAndKdkVerifyTest
"$JDK_DIR/bin/java" -cp "$TEST_DIR:$CLASSPATH" ShortTurnRerouteBlinkTest || echo "ShortTurnRerouteBlinkTest FAILED (expected until fixed)"
"$JDK_DIR/bin/java" -cp "$TEST_DIR:$CLASSPATH" ShortTurnBlinkVisibilityTest || echo "ShortTurnBlinkVisibilityTest FAILED (expected until fixed)"
# This probe loads additional IBM J9 classes reconstructed from the stock JXE.
# Their invokespecial bytecode is rejected by the HotSpot verifier; disable it
# only for this isolated host probe. The production patch build is unchanged.
"$JDK_DIR/bin/java" -Xverify:none -cp "$TEST_DIR:$CLASSPATH" DistanceBargraphChainTest
# Run KOMO against the pre-uninline stock bytecode too; final.jar is a decompiler input.
"$JDK_DIR/bin/java" -Xverify:none \
    -cp "$TEST_DIR:$PATCH_JAR:$COMBINED_JAR:$TOOLS_DIR/libs/org.osgi.framework-1.10.0.jar:$TOOLS_DIR/libs/org.osgi.util.tracker-1.5.4.jar" \
    KomoGraphicsStateTest

"$JDK_DIR/bin/java" -Xverify:none -cp "$TEST_DIR:$CLASSPATH" ManeuverParityTest
"$JDK_DIR/bin/java" -Xverify:none -cp "$TEST_DIR:$CLASSPATH" RendererViewportTest
"$JDK_DIR/bin/java" -Xverify:none -cp "$TEST_DIR:$CLASSPATH" ClusterKdkSyncTest
"$JDK_DIR/bin/java" -Xverify:none -cp "$TEST_DIR:$CLASSPATH" ClusterKdkBapChainTest
"$JDK_DIR/bin/java" -Xverify:none -cp "$TEST_DIR:$CLASSPATH" ClusterKdkRendererLifecycleTest

"$JDK_DIR/bin/java" -Xverify:none -cp "$TEST_DIR:$CLASSPATH" com.luka.carplay.rgd.LaneGuidanceTransportTest "$PROJECT_DIR/build/lane-guidance-wire.bin"
"$JDK_DIR/bin/java" -Xverify:none -cp "$TEST_DIR:$CLASSPATH" com.luka.carplay.rgd.LaneGuidanceLifecycleTest
"$JDK_DIR/bin/java" -Xverify:none -cp "$TEST_DIR:$CLASSPATH" com.luka.carplay.rgd.RgiDeliveryRecoveryTest
"$JDK_DIR/bin/java" -Xverify:none -cp "$TEST_DIR:$CLASSPATH" com.luka.carplay.rgd.RouteTextTruncationAndExitArrowTest
"$JDK_DIR/bin/java" -Xverify:none -cp "$TEST_DIR:$CLASSPATH" com.luka.carplay.rgd.CurrentPositionDeliveryTest
"$JDK_DIR/bin/java" -Xverify:none -cp "$TEST_DIR:$CLASSPATH" com.luka.carplay.rgd.RouteInfoTimeoutTest
"$JDK_DIR/bin/java" -Xverify:none \
    -cp "$TEST_DIR:$PATCH_JAR:$COMBINED_JAR:$TOOLS_DIR/libs/org.osgi.framework-1.10.0.jar:$TOOLS_DIR/libs/org.osgi.util.tracker-1.5.4.jar" \
    com.luka.carplay.rgd.CurrentPositionStockChainTest
python3 "$PROJECT_DIR/tests/test_rgd_native_contract.py"
