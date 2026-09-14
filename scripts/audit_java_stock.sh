#!/bin/bash
set -euo pipefail
PROJECT_DIR=$(cd "$(dirname "$0")/.." && pwd)
TOOLS_DIR="$PROJECT_DIR/../../Tools/jxe2jar"
JDK="$TOOLS_DIR/jvms/zulu8.78.0.19-ca-jdk8.0.412-macosx_aarch64/zulu-8.jdk/Contents/Home"
ASM="$TOOLS_DIR/tools/uninline/lib/asm-9.7.jar:$TOOLS_DIR/tools/uninline/lib/asm-tree-9.7.jar"
bash "$PROJECT_DIR/scripts/build_java.sh"
python3 "$PROJECT_DIR/tests/audit_java_sources.py" "$PROJECT_DIR" \
    "$TOOLS_DIR/out/MU1316-vf" "$PROJECT_DIR/build/java-stock-audit"
TMP=$(mktemp -d)
trap 'rm -rf "$TMP"' EXIT
"$JDK/bin/javac" -cp "$ASM" -d "$TMP" "$PROJECT_DIR/tests/JavaStockLinkageAudit.java"
# The final JAR is for decompilation: AccessInline rewrites synthetic accessors.
# Also check the pre-uninline JAR, whose callers retain the actual accessor ABI.
for STOCK in MU1316-final.jar MU1316-combined.jar; do
    echo "Auditing $STOCK with the MU1316 J9 class library"
    "$JDK/bin/java" -Xmx1g -cp "$TMP:$ASM" JavaStockLinkageAudit \
        "$PROJECT_DIR/build/carplay_hook.jar" "$TOOLS_DIR/out/$STOCK" \
        "$TOOLS_DIR/libs/jcl/MHI2Q_US_AUG22_P5087_MU1316/jcl.jar" \
        "$TOOLS_DIR/libs/org.osgi.framework-1.10.0.jar" "$TOOLS_DIR/libs/org.osgi.util.tracker-1.5.4.jar"
done
