#!/bin/bash
# Host test of the actual Java 1.4 BAPBridge against the stock service interface.
# Always build fresh; no HU access or application startup.
set -euo pipefail
PROJECT_DIR=$(cd "$(dirname "$0")/.." && pwd)
. "$PROJECT_DIR/scripts/java_env.sh"; TOOLS_DIR=$JXE2JAR
JDK_DIR="$JAVA8_HOME"
STOCK_JAR="$PROJECT_DIR/java_stock/US_MU1316_lsd.jar"
PATCH_JAR="$PROJECT_DIR/build/carplay_hook.jar"
if [ ! -x "$JDK_DIR/bin/javac" ] || [ ! -f "$STOCK_JAR" ]; then
    echo "Missing MU1316 build JDK or stock JAR." >&2
    exit 1
fi
bash "$PROJECT_DIR/scripts/build_java.sh"
TEST_DIR=$(mktemp -d)
trap 'rm -rf "$TEST_DIR"' EXIT
CLASSPATH="$PATCH_JAR:$STOCK_JAR:$OSGI_CP"
"$JDK_DIR/bin/javac" -encoding UTF-8 -cp "$CLASSPATH" -d "$TEST_DIR" \
    "$PROJECT_DIR/tests/CurrentPositionDeliveryTest.java" \
    "$PROJECT_DIR/tests/RouteInfoTimeoutTest.java" \
    "$PROJECT_DIR/tests/CurrentPositionStockChainTest.java" \
    "$PROJECT_DIR/tests/VCTextScrollTest.java" \
    "$PROJECT_DIR/tests/RouteInfoPresentationTest.java" \
    "$PROJECT_DIR/tests/RouteGuidanceDeltaTest.java" \
    "$PROJECT_DIR/tests/DistanceBargraphChainTest.java" \
    "$PROJECT_DIR/tests/KomoGraphicsStateTest.java" \
    "$PROJECT_DIR/tests/ManeuverChainAudit.java" \
    "$PROJECT_DIR/tests/ManeuverParityTest.java" \
    "$PROJECT_DIR/tests/RendererViewportTest.java" \
    "$PROJECT_DIR/tests/ClusterKdkSyncTest.java" \
    "$PROJECT_DIR/tests/ClusterKdkBapChainTest.java" \
    "$PROJECT_DIR/tests/ClusterKdkRendererLifecycleTest.java" \
    "$PROJECT_DIR/tests/BapShutdownFailureTest.java" \
    "$PROJECT_DIR/tests/ManeuverFieldArrivalTest.java" \
    "$PROJECT_DIR/tests/LaneGuidanceTransportTest.java" \
    "$PROJECT_DIR/tests/LaneGuidanceLifecycleTest.java" \
    "$PROJECT_DIR/tests/RgiDeliveryRecoveryTest.java" \
    "$PROJECT_DIR/tests/RendererMapperDirectionTest.java"
# No checkout resources or font files are visible through the working directory.
# The pure runtime uses only classes and the embedded data in the built JAR.
(cd "$TEST_DIR" && "$JDK_DIR/bin/java" -cp "$TEST_DIR:$PATCH_JAR" \
    com.luka.carplay.rgd.VCTextScrollTest ${VC_UNICODE_TEST_DIR:+"$VC_UNICODE_TEST_DIR"})
"$JDK_DIR/bin/java" -cp "$TEST_DIR:$CLASSPATH" RouteInfoPresentationTest
"$JDK_DIR/bin/java" -cp "$TEST_DIR:$CLASSPATH" RendererMapperDirectionTest
"$JDK_DIR/bin/java" -cp "$TEST_DIR:$CLASSPATH" RouteGuidanceDeltaTest
# This probe loads additional IBM J9 classes reconstructed from the stock JXE.
# Their invokespecial bytecode is rejected by the HotSpot verifier; disable it
# only for this isolated host probe. The production patch build is unchanged.
"$JDK_DIR/bin/java" -Xverify:none -cp "$TEST_DIR:$CLASSPATH" DistanceBargraphChainTest
# Run KOMO against the pre-uninline stock bytecode too; final.jar is a decompiler input.
"$JDK_DIR/bin/java" -Xverify:none \
    -cp "$TEST_DIR:$PATCH_JAR:$PROJECT_DIR/java_stock/US_MU1316_lsd.jar:$OSGI_CP" \
    KomoGraphicsStateTest

"$JDK_DIR/bin/java" -Xverify:none -cp "$TEST_DIR:$CLASSPATH" ManeuverParityTest
"$JDK_DIR/bin/java" -Xverify:none -cp "$TEST_DIR:$CLASSPATH" RendererViewportTest
"$JDK_DIR/bin/java" -Xverify:none -cp "$TEST_DIR:$CLASSPATH" ClusterKdkSyncTest
"$JDK_DIR/bin/java" -Xverify:none -cp "$TEST_DIR:$CLASSPATH" ClusterKdkBapChainTest
"$JDK_DIR/bin/java" -Xverify:none -cp "$TEST_DIR:$CLASSPATH" ClusterKdkRendererLifecycleTest
for variant in us eu cn; do
    case "$variant" in
        us) patch=carplay_hook.jar; stock=US_MU1316_lsd.jar ;;
        eu) patch=carplay_hook.jar; stock=EU_MU1329_lsd.jar ;;
        cn) patch=carplay_hook_cn.jar; stock=CN_MU1003_lsd.jar ;;
    esac
    echo "BAP shutdown regressions: $variant"
    "$JDK_DIR/bin/java" -cp "$TEST_DIR:$PROJECT_DIR/build/$patch:$PROJECT_DIR/java_stock/$stock" \
        com.luka.carplay.rgd.BapShutdownFailureTest
    echo "Maneuver field arrival permutations: $variant"
    "$JDK_DIR/bin/java" -cp "$TEST_DIR:$PROJECT_DIR/build/$patch:$PROJECT_DIR/java_stock/$stock" \
        com.luka.carplay.rgd.ManeuverFieldArrivalTest
done

"$JDK_DIR/bin/java" -Xverify:none -cp "$TEST_DIR:$CLASSPATH" com.luka.carplay.rgd.LaneGuidanceTransportTest "$PROJECT_DIR/build/lane-guidance-wire.bin"
"$JDK_DIR/bin/java" -Xverify:none -cp "$TEST_DIR:$CLASSPATH" com.luka.carplay.rgd.LaneGuidanceLifecycleTest
"$JDK_DIR/bin/java" -Xverify:none -cp "$TEST_DIR:$CLASSPATH" com.luka.carplay.rgd.RgiDeliveryRecoveryTest
"$JDK_DIR/bin/java" -Xverify:none -cp "$TEST_DIR:$CLASSPATH" com.luka.carplay.rgd.CurrentPositionDeliveryTest
"$JDK_DIR/bin/java" -Xverify:none -cp "$TEST_DIR:$CLASSPATH" com.luka.carplay.rgd.RouteInfoTimeoutTest
"$JDK_DIR/bin/java" -Xverify:none \
    -cp "$TEST_DIR:$PATCH_JAR:$PROJECT_DIR/java_stock/US_MU1316_lsd.jar:$OSGI_CP" \
    com.luka.carplay.rgd.CurrentPositionStockChainTest
python3 "$PROJECT_DIR/tests/test_rgd_native_contract.py"

# Native hook -> bus -> Java -> HUD/renderer, with the JAR built above.
SKIP_JAVA_BUILD=1 bash "$PROJECT_DIR/scripts/test_lane_guidance_e2e.sh"
