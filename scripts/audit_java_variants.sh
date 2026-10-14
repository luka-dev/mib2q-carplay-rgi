#!/bin/bash
# Each Java variant must link cleanly against every firmware it is installed on:
#   build/carplay_hook.jar     vs java_stock/US_MU1316_lsd.jar, EU_MU1329_lsd.jar
#   build/carplay_hook_cn.jar  vs java_stock/CN_MU1003_lsd.jar
# (tests/JavaStockLinkageAudit.java: patch->stock and stock->patch member links).
# Keep this matrix in step with java_jar() in the installer (E2P region US/ER/JP -> US jar,
# CN -> CN jar).  JP MU1017 has no java_stock jar: its replaced stock classes are
# byte-identical to US.
set -euo pipefail
PROJECT_DIR=$(cd "$(dirname "$0")/.." && pwd)
. "$PROJECT_DIR/scripts/java_env.sh"; TOOLS_DIR=$JXE2JAR; java_need_jxe2jar audit_java_variants
JDK="$JAVA8_HOME"
ASM="$ASM_CP"
T=$(mktemp -d); trap 'rm -rf "$T"' EXIT
"$JDK/bin/javac" -cp "$ASM" -d "$T" "$PROJECT_DIR/tests/JavaStockLinkageAudit.java"
audit() {   # audit <jar> <stock>
    "$JDK/bin/java" -Xmx2g -cp "$T:$ASM" JavaStockLinkageAudit "$PROJECT_DIR/build/$1" "$PROJECT_DIR/java_stock/$2" \
        "$TOOLS_DIR/libs/jcl/MHI2Q_US_AUG22_P5087_MU1316/jcl.jar" \
        "$TOOLS_DIR/libs/org.osgi.framework-1.10.0.jar" "$TOOLS_DIR/libs/org.osgi.util.tracker-1.5.4.jar" \
        > "$T/out" 2>&1 || { echo "FAIL $1 vs $2"; grep -E '^(MISSING|ACCESS)' "$T/out" | head -20; exit 1; }
}
audit carplay_hook.jar US_MU1316_lsd.jar
audit carplay_hook.jar EU_MU1329_lsd.jar
audit carplay_hook_cn.jar CN_MU1003_lsd.jar
echo "Java variants link cleanly: US+EU jar vs MU1316/MU1329, CN jar vs MU1003 PASS"
