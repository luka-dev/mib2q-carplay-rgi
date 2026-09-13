#!/bin/bash
# Observation probe pinned to dda63e4: success reproduces known defects.
# Not part of the acceptance suite; fixed behavior should change expectations.
set -euo pipefail
PROJECT_DIR=$(cd "$(dirname "$0")/../.." && pwd)
AUDIT_JDK="$PROJECT_DIR/../../Tools/jxe2jar/jvms/zulu8.78.0.19-ca-jdk8.0.412-macosx_aarch64/zulu-8.jdk/Contents/Home"
AUDIT_CP="$PROJECT_DIR/build/carplay_hook.jar:$PROJECT_DIR/../../Tools/jxe2jar/out/MU1316-combined-final.jar:$PROJECT_DIR/../../Tools/jxe2jar/libs/org.osgi.framework-1.10.0.jar:$PROJECT_DIR/../../Tools/jxe2jar/libs/org.osgi.util.tracker-1.5.4.jar"
bash "$PROJECT_DIR/scripts/build_java.sh"
AUDIT_CLASSES=$(mktemp -d)
trap 'rm -rf "$AUDIT_CLASSES"' EXIT
"$AUDIT_JDK/bin/javac" -encoding UTF-8 -cp "$AUDIT_CP" -d "$AUDIT_CLASSES" \
    "$PROJECT_DIR/output/java-rgi-audit-2026-09-13/JavaRgiAuditProbe.java"
# Host-only: reconstructed IBM J9 stock bytecode requires disabled verification.
"$AUDIT_JDK/bin/java" -Xverify:none -cp "$AUDIT_CLASSES:$AUDIT_CP" JavaRgiAuditProbe
