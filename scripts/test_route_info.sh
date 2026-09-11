#!/bin/bash
# Host test of the actual Java 1.4 BAPBridge against the stock service interface.
# Always build fresh; no HU access or application startup.
set -euo pipefail
PROJECT_DIR=$(cd "$(dirname "$0")/.." && pwd)
TOOLS_DIR="$PROJECT_DIR/../../Tools/jxe2jar"
JDK_DIR="$TOOLS_DIR/jvms/zulu8.78.0.19-ca-jdk8.0.412-macosx_aarch64/zulu-8.jdk/Contents/Home"
STOCK_JAR="$TOOLS_DIR/out/MU1316-combined-final.jar"
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
    "$PROJECT_DIR/tests/RouteInfoPresentationTest.java" \
    "$PROJECT_DIR/tests/RendererMapperDirectionTest.java"
"$JDK_DIR/bin/java" -cp "$TEST_DIR:$CLASSPATH" RouteInfoPresentationTest
"$JDK_DIR/bin/java" -cp "$TEST_DIR:$CLASSPATH" RendererMapperDirectionTest
