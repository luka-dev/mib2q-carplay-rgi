#!/bin/bash
# Host-only investigation of real MU1316 camera/display contracts.
# Fresh Java patch compilation stays in a temporary directory; no HU or release writes.
set -euo pipefail
PROJECT_DIR=$(cd "$(dirname "$0")/.." && pwd)
. "$PROJECT_DIR/scripts/java_env.sh"; TOOLS_DIR=$JXE2JAR; java_need_jxe2jar test_partial360_preflight
JDK="$JAVA8_HOME"
LIBS="$OSGI_CP"
ASM="$ASM_CP"
TEST_DIR=$(mktemp -d)
trap 'rm -rf "$TEST_DIR"' EXIT
mkdir -p "$TEST_DIR/patch" "$TEST_DIR/stubs" "$TEST_DIR/test"
python3 - "$PROJECT_DIR/java_patch" "$TEST_DIR/sources" <<'PY'
import pathlib, sys
pathlib.Path(sys.argv[2]).write_text(''.join('"' + str(p) + '"\n'
    for p in sorted(pathlib.Path(sys.argv[1]).rglob('*.java'))))
PY
"$JDK/bin/javac" -source 1.4 -target 1.4 -Xlint:-options \
    -cp "$TOOLS_DIR/out/MU1316-final.jar:$LIBS" \
    -sourcepath "$PROJECT_DIR/java_patch" -d "$TEST_DIR/patch" @"$TEST_DIR/sources"
"$JDK/bin/javac" -d "$TEST_DIR/stubs" \
    "$PROJECT_DIR/tests/stubs/pdc/com/luka/carplay/core/CarPlayApp.java" \
    "$PROJECT_DIR/tests/stubs/app-lifecycle/com/luka/carplay/framework/Log.java" \
    "$PROJECT_DIR/tests/stubs/pdc/de/audi/atip/hmi/view/Screen.java" \
    "$PROJECT_DIR/tests/stubs/pdc/de/esolutions/hmi/widgets/audi/base/AbstractScreenWidget.java"
CP="$TEST_DIR/stubs:$TEST_DIR/patch:$TOOLS_DIR/out/MU1316-combined.jar:$LIBS:$ASM"
"$JDK/bin/javac" -cp "$CP" -d "$TEST_DIR/test" \
    "$PROJECT_DIR/tests/PdcResourcePolicyTest.java" \
    "$PROJECT_DIR/tests/Partial360PreflightTest.java"
for STOCK in MU1316-combined MU1316-final; do
    echo "Partial 360 contracts: $STOCK"
    "$JDK/bin/java" -cp "$TEST_DIR/test:$TEST_DIR/stubs:$TEST_DIR/patch:$TOOLS_DIR/out/$STOCK.jar:$LIBS:$ASM" \
        Partial360PreflightTest
done
"$JDK/bin/java" -cp "$TEST_DIR/test:$CP" PdcResourcePolicyTest
if "$JDK/bin/java" -cp "$TEST_DIR/test:$TEST_DIR/stubs:$TOOLS_DIR/out/MU1316-combined.jar:$TEST_DIR/patch:$LIBS:$ASM" \
    PdcResourcePolicyTest > "$TEST_DIR/stock.log" 2>&1; then
    echo "ERROR: unpatched tracker unexpectedly passed"; exit 1
fi
if ! grep -q 'pure OPS 108 toggled Main Wizard' "$TEST_DIR/stock.log"; then
    cat "$TEST_DIR/stock.log"; exit 1
fi
echo "PDC regression control: stock takeover reproduced; current guard passes"
