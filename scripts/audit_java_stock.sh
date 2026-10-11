#!/bin/bash
# Preserve upstream's source inventory and both decompiled/executable inventories.
set -euo pipefail
ROOT=$(cd "$(dirname "$0")/.." && pwd)
TOOLS=${CARPLAY_TOOLS_DIR:-$ROOT/../../Tools/jxe2jar}
OUTPUT=${JAVA_OUTPUT:-$ROOT/build}
if [ -z "${STOCK_JAR:-}" ]; then
    REFERENCE=${JAVA_STOCK_SOURCES:-$TOOLS/out/MU1316-vf}
    test -d "$REFERENCE" || { echo "Missing stock source tree: $REFERENCE" >&2; exit 1; }
    bash "$ROOT/scripts/java/docker.sh" build "$@"
    python3 "$ROOT/tests/audit_java_sources.py" "$ROOT" "$REFERENCE" "$OUTPUT/java-stock-audit"
    for stock in MU1316-final.jar MU1316-combined.jar; do
        echo "Auditing $stock with the MU1316 J9 class library"
        STOCK_JAR="$TOOLS/out/$stock" \
            STOCK_BOOT_JAR="${STOCK_BOOT_JAR:-$TOOLS/libs/jcl/MHI2Q_US_AUG22_P5087_MU1316/jcl.jar}" \
            JAVA_SKIP_BUILD=1 bash "$ROOT/scripts/java/docker.sh" test linkage
        cp "$OUTPUT/stock-linkage.txt" "$OUTPUT/java-stock-audit/$stock-linkage.txt"
    done
else
    # A supplied combined firmware JAR is a separate inventory. Never silently
    # compare it to the maintainer's MU1316 sources or second JAR.
    if [ -n "${JAVA_STOCK_SOURCES:-}" ]; then
        test -d "$JAVA_STOCK_SOURCES" || { echo "Missing stock source tree: $JAVA_STOCK_SOURCES" >&2; exit 1; }
        python3 "$ROOT/tests/audit_java_sources.py" "$ROOT" "$JAVA_STOCK_SOURCES" "$OUTPUT/java-stock-audit"
    else
        echo 'Source inventory omitted: set JAVA_STOCK_SOURCES to this firmware source tree.'
    fi
    exec bash "$ROOT/scripts/java/docker.sh" test linkage "$@"
fi
