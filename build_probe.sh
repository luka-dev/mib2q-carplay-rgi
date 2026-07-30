#!/bin/bash
# MINIMAL Smartphone-tab probe jar: patched TerminalModeBapCombi + SmartphoneTabProbe ONLY.
# No hook, no bus, no renderers. Compiles against combined-final + OSGi. Target 1.4.
set -e
SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
TOOLS_DIR="$SCRIPT_DIR/../jxe2jar"
JAVA_HOME="$TOOLS_DIR/jvms/zulu8.78.0.19-ca-jdk8.0.412-macosx_aarch64/zulu-8.jdk/Contents/Home"
JAVAC="$JAVA_HOME/bin/javac"
JAR="$JAVA_HOME/bin/jar"
STOCK_JAR="$TOOLS_DIR/out/MU1316-combined-final.jar"
OSGI_LIBS="$TOOLS_DIR/libs"
OSGI_CP="$OSGI_LIBS/org.osgi.framework-1.10.0.jar:$OSGI_LIBS/org.osgi.util.tracker-1.5.4.jar"

SRC_DIR="$SCRIPT_DIR/java_patch_probe"
BUILD_DIR="$SRC_DIR/build"
OUTPUT_DIR="$BUILD_DIR/classes"
OUTPUT_JAR="$BUILD_DIR/carplay_tabprobe.jar"

echo "=== Smartphone-tab PROBE builder ==="
[ -x "$JAVAC" ] || { echo "ERROR: javac not found at $JAVAC"; exit 1; }
[ -f "$STOCK_JAR" ] || { echo "ERROR: $STOCK_JAR not found"; exit 1; }

CLASSPATH="$STOCK_JAR:$OSGI_CP"
rm -rf "$OUTPUT_DIR"; mkdir -p "$OUTPUT_DIR" "$BUILD_DIR"

SOURCES_LIST=$(mktemp)
find "$SRC_DIR" -name '*.java' -type f > "$SOURCES_LIST"
echo "Compiling $(wc -l < "$SOURCES_LIST" | tr -d ' ') files (target 1.4)..."

"$JAVAC" -source 1.4 -target 1.4 \
    -cp "$CLASSPATH" \
    -sourcepath "$SRC_DIR" \
    -d "$OUTPUT_DIR" \
    -Xlint:-options \
    @"$SOURCES_LIST"
rm -f "$SOURCES_LIST"

cd "$OUTPUT_DIR"; "$JAR" cf "$OUTPUT_JAR" .; cd "$SCRIPT_DIR"
echo "=== wrote $OUTPUT_JAR ==="
find "$OUTPUT_DIR" -name '*.class' | sed "s|$OUTPUT_DIR/||" | sort
