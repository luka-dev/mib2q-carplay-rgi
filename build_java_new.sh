#!/bin/bash
#
# Build the redesigned CarPlay Java patch (java_patch_new/).
# Compiles against the full accurate decompile MU1316-combined-final.jar + OSGi.
# Target Java 1.2 (MU1316 lsd runtime).
#
set -e

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
TOOLS_DIR="$SCRIPT_DIR/../jxe2jar"

JAVA_HOME="$TOOLS_DIR/jvms/zulu8.78.0.19-ca-jdk8.0.412-macosx_aarch64/zulu-8.jdk/Contents/Home"
JAVAC="$JAVA_HOME/bin/javac"
JAR="$JAVA_HOME/bin/jar"

STOCK_JAR="$TOOLS_DIR/out/MU1316-combined-final.jar"
OSGI_LIBS="$TOOLS_DIR/libs"
OSGI_CP="$OSGI_LIBS/org.osgi.framework-1.10.0.jar:$OSGI_LIBS/org.osgi.util.tracker-1.5.4.jar"

SRC_DIR="$SCRIPT_DIR/java_patch_new"
BUILD_DIR="$SCRIPT_DIR/build"
OUTPUT_DIR="$BUILD_DIR/java_new/classes"
OUTPUT_JAR="$BUILD_DIR/carplay_hook_new.jar"

echo "=== CarPlay Hook Java Builder (new / combined-final) ==="
[ -x "$JAVAC" ] || { echo "ERROR: javac not found at $JAVAC"; exit 1; }
[ -f "$STOCK_JAR" ] || { echo "ERROR: $STOCK_JAR not found"; exit 1; }

CLASSPATH="$STOCK_JAR:$OSGI_CP"
rm -rf "$OUTPUT_DIR"; mkdir -p "$OUTPUT_DIR" "$BUILD_DIR"

SOURCES_LIST=$(mktemp)
find "$SRC_DIR" -name '*.java' -type f > "$SOURCES_LIST"
echo "Compiling $(wc -l < "$SOURCES_LIST" | tr -d ' ') files (target 1.2)..."

"$JAVAC" -source 1.2 -target 1.2 \
    -cp "$CLASSPATH" \
    -sourcepath "$SRC_DIR" \
    -d "$OUTPUT_DIR" \
    -Xlint:-options \
    @"$SOURCES_LIST"
rm -f "$SOURCES_LIST"

cd "$OUTPUT_DIR"; "$JAR" cf "$OUTPUT_JAR" .; cd "$SCRIPT_DIR"
echo "Output: $OUTPUT_JAR"
find "$OUTPUT_DIR" -name '*.class' | sed "s|$OUTPUT_DIR/||" | sort
