#!/bin/bash
# Offline export; no sockets, no HU access. Stock JXE reconstruction has known
# corrupt string literals: its complete wire output is evidence, not a CAN capture.
set -euo pipefail
PROJECT_DIR=$(cd "$(dirname "$0")/.." && pwd)
TOOLS_DIR="$PROJECT_DIR/../../Tools/jxe2jar"
JDK_DIR="$TOOLS_DIR/jvms/zulu8.78.0.19-ca-jdk8.0.412-macosx_aarch64/zulu-8.jdk/Contents/Home"
OUT_DIR=${1:-"$PROJECT_DIR/output/maneuver-audit"}
mkdir -p "$OUT_DIR"
bash "$PROJECT_DIR/scripts/build_java.sh"
TEST_DIR=$(mktemp -d)
trap 'rm -rf "$TEST_DIR"' EXIT
CLASSPATH="$PROJECT_DIR/build/carplay_hook.jar:$TOOLS_DIR/out/MU1316-final.jar:$TOOLS_DIR/libs/org.osgi.framework-1.10.0.jar:$TOOLS_DIR/libs/org.osgi.util.tracker-1.5.4.jar"
"$JDK_DIR/bin/javac" -encoding UTF-8 -cp "$CLASSPATH" -d "$TEST_DIR" "$PROJECT_DIR/tests/ManeuverChainAudit.java" "$PROJECT_DIR/tests/RampDescriptorAudit.java" "$PROJECT_DIR/tests/ManeuverIconSelectionAudit.java"
"$JDK_DIR/bin/java" -Xverify:none -cp "$TEST_DIR:$CLASSPATH" ManeuverChainAudit "$OUT_DIR/java_mapping.csv"
"$JDK_DIR/bin/java" -Xverify:none -cp "$TEST_DIR:$CLASSPATH" RampDescriptorAudit "$OUT_DIR/ramp_mapping.csv"
"$JDK_DIR/bin/java" -Xverify:none -cp "$TEST_DIR:$CLASSPATH" ManeuverIconSelectionAudit "$OUT_DIR/selection_examples.csv"
