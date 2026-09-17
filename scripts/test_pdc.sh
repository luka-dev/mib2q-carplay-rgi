#!/bin/bash
# Real MU1316 resource policy, reactive properties and commands; fake physical HMI.
set -euo pipefail
PROJECT_DIR=$(cd "$(dirname "$0")/.." && pwd)
TOOLS_DIR="$PROJECT_DIR/../../Tools/jxe2jar"
JDK="$TOOLS_DIR/jvms/zulu8.78.0.19-ca-jdk8.0.412-macosx_aarch64/zulu-8.jdk/Contents/Home"
# combined retains executable stock accessors. final's decompiler-only
# AccessInline pass breaks private accesses in stock anonymous classes;
# audit_java_stock.sh separately verifies linkage against both inventories.
STOCK="$TOOLS_DIR/out/MU1316-combined.jar"
LIBS="$TOOLS_DIR/libs/org.osgi.framework-1.10.0.jar:$TOOLS_DIR/libs/org.osgi.util.tracker-1.5.4.jar"
PATCH="${PDC_PATCH_JAR:-$PROJECT_DIR/build/carplay_hook.jar}"
ASM="$TOOLS_DIR/tools/uninline/lib/asm-9.7.jar:$TOOLS_DIR/tools/uninline/lib/asm-tree-9.7.jar"
TEST_DIR=$(mktemp -d)
trap 'rm -rf "$TEST_DIR"' EXIT
mkdir -p "$TEST_DIR/stubs" "$TEST_DIR/test"
"$JDK/bin/javac" -cp "$STOCK:$LIBS" -d "$TEST_DIR/stubs" \
    "$PROJECT_DIR/tests/stubs/pdc/com/luka/carplay/core/CarPlayApp.java" \
    "$PROJECT_DIR/tests/stubs/app-lifecycle/com/luka/carplay/framework/Log.java" \
    "$PROJECT_DIR/tests/stubs/pdc/de/audi/atip/hmi/view/Screen.java" \
    "$PROJECT_DIR/tests/stubs/pdc/de/esolutions/hmi/widgets/audi/base/AbstractScreenWidget.java"
CP="$TEST_DIR/stubs:$PATCH:$STOCK:$LIBS:$ASM"
"$JDK/bin/javac" -cp "$CP" -d "$TEST_DIR/test" "$PROJECT_DIR/tests/PdcResourcePolicyTest.java" \
    "$PROJECT_DIR/tests/PdcExternalEventsTest.java" "$PROJECT_DIR/tests/OpsAudioDrawerTest.java" \
    "$PROJECT_DIR/tests/OpsStatusLineTest.java"
"$JDK/bin/java" -cp "$TEST_DIR/test:$CP" PdcResourcePolicyTest
"$JDK/bin/java" -cp "$TEST_DIR/test:$CP" PdcExternalEventsTest
"$JDK/bin/java" -cp "$TEST_DIR/test:$CP" OpsAudioDrawerTest
"$JDK/bin/java" -cp "$TEST_DIR/test:$CP" OpsStatusLineTest
# The isolated PDC tests fake CarPlayApp.active. Also run the real lifecycle
# worker with only external modules/transport substituted, so disconnect does
# not depend on a later HMI callback to release the presentation policy.
mkdir -p "$TEST_DIR/lifecycle"
"$JDK/bin/javac" -encoding UTF-8 -cp "$CP" -d "$TEST_DIR/lifecycle" \
    "$PROJECT_DIR/java_patch/com/luka/carplay/core/CarPlayApp.java" \
    "$PROJECT_DIR/java_patch/com/luka/carplay/core/Module.java" \
    "$PROJECT_DIR/tests/stubs/app-lifecycle/com/luka/carplay/core/LifecycleFixtures.java" \
    "$PROJECT_DIR/tests/stubs/app-lifecycle/com/luka/carplay/bus/CarplayBus.java" \
    "$PROJECT_DIR/tests/stubs/app-lifecycle/com/luka/carplay/framework/Log.java"
"$JDK/bin/javac" -cp "$TEST_DIR/lifecycle:$TEST_DIR/test:$CP" -d "$TEST_DIR/test" \
    "$PROJECT_DIR/tests/CarPlayPdcLifecycleTest.java"
"$JDK/bin/java" -cp "$TEST_DIR/lifecycle:$TEST_DIR/test:$CP" CarPlayPdcLifecycleTest
# Same scenario with the original tracker must reproduce the reported failure.
if "$JDK/bin/java" -cp "$TEST_DIR/test:$TEST_DIR/stubs:$STOCK:$PATCH:$LIBS:$ASM" \
    PdcResourcePolicyTest > "$TEST_DIR/stock.log" 2>&1; then
    echo "ERROR: unpatched stock tracker unexpectedly passed"; exit 1
fi
if ! grep -q 'pure OPS 108 toggled Main Wizard' "$TEST_DIR/stock.log"; then
    cat "$TEST_DIR/stock.log"; exit 1
fi
echo "PDC stock regression: reproduced Main Wizard takeover"

# Separate negative control for the screenshot's actual APS drawer source.
if "$JDK/bin/java" -cp "$TEST_DIR/test:$TEST_DIR/stubs:$STOCK:$PATCH:$LIBS:$ASM" \
    OpsAudioDrawerTest > "$TEST_DIR/stock-drawer.log" 2>&1; then
    echo "ERROR: unpatched stock APS drawer unexpectedly passed"; exit 1
fi
if ! grep -q 'APS drawer still selected over CarPlay + side OPS' "$TEST_DIR/stock-drawer.log"; then
    cat "$TEST_DIR/stock-drawer.log"; exit 1
fi
echo "PDC stock regression: reproduced APS warning content 6001 over CarPlay"

if "$JDK/bin/java" -cp "$TEST_DIR/test:$TEST_DIR/stubs:$STOCK:$PATCH:$LIBS:$ASM" \
    OpsStatusLineTest > "$TEST_DIR/stock-statusline.log" 2>&1; then
    echo "ERROR: unpatched stock footer unexpectedly passed"; exit 1
fi
if ! grep -q 'MMI status line 62 remained over CarPlay' "$TEST_DIR/stock-statusline.log"; then
    cat "$TEST_DIR/stock-statusline.log"; exit 1
fi
echo "PDC stock regression: reproduced MMI status line 62 over CarPlay"
