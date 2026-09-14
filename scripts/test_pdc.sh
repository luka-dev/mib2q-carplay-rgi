#!/bin/bash
# Real MU1316 resource policy, reactive properties and commands; fake physical HMI.
set -euo pipefail
PROJECT_DIR=$(cd "$(dirname "$0")/.." && pwd)
TOOLS_DIR="$PROJECT_DIR/../../Tools/jxe2jar"
JDK="$TOOLS_DIR/jvms/zulu8.78.0.19-ca-jdk8.0.412-macosx_aarch64/zulu-8.jdk/Contents/Home"
STOCK="$TOOLS_DIR/out/MU1316-combined.jar"
LIBS="$TOOLS_DIR/libs/org.osgi.framework-1.10.0.jar:$TOOLS_DIR/libs/org.osgi.util.tracker-1.5.4.jar"
PATCH="$PROJECT_DIR/build/carplay_hook.jar"
ASM="$TOOLS_DIR/tools/uninline/lib/asm-9.7.jar:$TOOLS_DIR/tools/uninline/lib/asm-tree-9.7.jar"
TEST_DIR=$(mktemp -d)
trap 'rm -rf "$TEST_DIR"' EXIT
mkdir -p "$TEST_DIR/stubs" "$TEST_DIR/test"
"$JDK/bin/javac" -d "$TEST_DIR/stubs" \
    "$PROJECT_DIR/tests/stubs/pdc/com/luka/carplay/core/CarPlayApp.java" \
    "$PROJECT_DIR/tests/stubs/app-lifecycle/com/luka/carplay/framework/Log.java" \
    "$PROJECT_DIR/tests/stubs/pdc/de/audi/atip/hmi/view/Screen.java" \
    "$PROJECT_DIR/tests/stubs/pdc/de/esolutions/hmi/widgets/audi/base/AbstractScreenWidget.java"
CP="$TEST_DIR/stubs:$PATCH:$STOCK:$LIBS:$ASM"
"$JDK/bin/javac" -cp "$CP" -d "$TEST_DIR/test" "$PROJECT_DIR/tests/PdcResourcePolicyTest.java"
"$JDK/bin/java" -cp "$TEST_DIR/test:$CP" PdcResourcePolicyTest
# Same scenario with the original tracker must reproduce the reported failure.
if "$JDK/bin/java" -cp "$TEST_DIR/test:$TEST_DIR/stubs:$STOCK:$PATCH:$LIBS:$ASM" \
    PdcResourcePolicyTest > "$TEST_DIR/stock.log" 2>&1; then
    echo "ERROR: unpatched stock tracker unexpectedly passed"; exit 1
fi
if ! grep -q 'pure OPS 108 toggled Main Wizard' "$TEST_DIR/stock.log"; then
    cat "$TEST_DIR/stock.log"; exit 1
fi
echo "PDC stock regression: reproduced Main Wizard takeover"
