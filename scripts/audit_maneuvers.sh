#!/bin/bash
# Offline export; no sockets, no HU access. Stock JXE reconstruction has known
# corrupt string literals: its complete wire output is evidence, not a CAN capture.
set -euo pipefail
PROJECT_DIR=$(cd "$(dirname "$0")/.." && pwd)
. "$PROJECT_DIR/scripts/java_env.sh"; TOOLS_DIR=$JXE2JAR; java_need_jxe2jar audit_maneuvers
JDK_DIR="$JAVA8_HOME"
OUT_DIR=${1:-"$PROJECT_DIR/output/maneuver-audit"}
mkdir -p "$OUT_DIR"
bash "$PROJECT_DIR/scripts/build_java.sh"
TEST_DIR=$(mktemp -d)
trap 'rm -rf "$TEST_DIR"' EXIT
CLASSPATH="$PROJECT_DIR/build/carplay_hook.jar:$TOOLS_DIR/out/MU1316-final.jar:$OSGI_CP"
"$JDK_DIR/bin/javac" -encoding UTF-8 -cp "$CLASSPATH" -d "$TEST_DIR" "$PROJECT_DIR/tests/ManeuverChainAudit.java" "$PROJECT_DIR/tests/RampDescriptorAudit.java" "$PROJECT_DIR/tests/ManeuverIconSelectionAudit.java"
"$JDK_DIR/bin/java" -Xverify:none -cp "$TEST_DIR:$CLASSPATH" ManeuverChainAudit "$OUT_DIR/java_mapping.csv"
"$JDK_DIR/bin/java" -Xverify:none -cp "$TEST_DIR:$CLASSPATH" RampDescriptorAudit "$OUT_DIR/ramp_mapping.csv"
"$JDK_DIR/bin/java" -Xverify:none -cp "$TEST_DIR:$CLASSPATH" ManeuverIconSelectionAudit "$OUT_DIR/selection_examples.csv"
