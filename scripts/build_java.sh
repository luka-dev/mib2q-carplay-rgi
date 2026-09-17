#!/bin/bash
#
# Build the canonical CarPlay Java patch.
#
#   ./scripts/build_java.sh
#
# Input:  java_patch/
# Output: build/carplay_hook.jar
#
# Compiles against MU1316-final.jar + OSGi, target 1.4 (jclfoun11 = Foundation 1.1).
set -e

[ "$#" -eq 0 ] || { echo "usage: ./scripts/build_java.sh"; exit 2; }

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
PROJECT_DIR="$(cd "$SCRIPT_DIR/.." && pwd)"
TOOLS_DIR="$PROJECT_DIR/../../Tools/jxe2jar"
JAVA_HOME="$TOOLS_DIR/jvms/zulu8.78.0.19-ca-jdk8.0.412-macosx_aarch64/zulu-8.jdk/Contents/Home"
JAVAC="$JAVA_HOME/bin/javac"
JAR="$JAVA_HOME/bin/jar"
STOCK_JAR="$TOOLS_DIR/out/MU1316-final.jar"
OSGI_LIBS="$TOOLS_DIR/libs"
OSGI_CP="$OSGI_LIBS/org.osgi.framework-1.10.0.jar:$OSGI_LIBS/org.osgi.util.tracker-1.5.4.jar"

SRC_DIR="$PROJECT_DIR/java_patch"
BUILD_DIR="$PROJECT_DIR/build"
OUTPUT_ROOT="$BUILD_DIR/java"
OUTPUT_DIR="$OUTPUT_ROOT/classes"
OUTPUT_JAR="$BUILD_DIR/carplay_hook.jar"

echo "=== CarPlay Java Patch Build ==="
[ -x "$JAVAC" ] || { echo "ERROR: javac not found at $JAVAC"; exit 1; }
[ -f "$STOCK_JAR" ] || { echo "ERROR: $STOCK_JAR not found"; exit 1; }
[ -d "$SRC_DIR" ] || { echo "ERROR: $SRC_DIR not found"; exit 1; }

CLASSPATH="$STOCK_JAR:$OSGI_CP"
rm -rf "$OUTPUT_ROOT"; mkdir -p "$OUTPUT_DIR" "$BUILD_DIR"

SOURCES_LIST=$(mktemp)
GEN_SRC_DIR=""
cleanup() {
    rm -f "$SOURCES_LIST" "$SOURCES_LIST.tmp"
    [ -z "$GEN_SRC_DIR" ] || rm -rf "$GEN_SRC_DIR"
    rm -rf "$OUTPUT_ROOT"
}
trap cleanup EXIT HUP INT TERM
find "$SRC_DIR" -name '*.java' -type f > "$SOURCES_LIST"
echo "Compiling $(wc -l < "$SOURCES_LIST" | tr -d ' ') files (target 1.4)..."

# Compile a generated CarPlayApp copy with a real, reproducible build identity. Never edit the
# source tree: replace only the explicit source-list entry for this build.
BUILD_ID_RAW=${CARPLAY_BUILD_ID:-$(git -C "$PROJECT_DIR" describe --always --dirty 2>/dev/null || echo unknown)}
BUILD_ID=$(printf '%s' "$BUILD_ID_RAW" | tr -cd 'A-Za-z0-9._-')
GEN_SRC_DIR=$(mktemp -d)
GEN_APP="$GEN_SRC_DIR/com/luka/carplay/core/CarPlayApp.java"
mkdir -p "$(dirname "$GEN_APP")"
sed "s/@BUILD_ID@/$BUILD_ID/g" "$SRC_DIR/com/luka/carplay/core/CarPlayApp.java" > "$GEN_APP"
grep -v '/com/luka/carplay/core/CarPlayApp.java$' "$SOURCES_LIST" > "$SOURCES_LIST.tmp"
mv "$SOURCES_LIST.tmp" "$SOURCES_LIST"
printf '%s\n' "$GEN_APP" >> "$SOURCES_LIST"

"$JAVAC" -source 1.4 -target 1.4 \
    -cp "$CLASSPATH" \
    -sourcepath "$GEN_SRC_DIR:$SRC_DIR" \
    -d "$OUTPUT_DIR" \
    -Xlint:-options \
    @"$SOURCES_LIST"
# Compact generated metrics/Unicode tables live inside the hook JAR. Ordinary
# builds never read firmware fonts or invoke the offline generator.
cp -R "$PROJECT_DIR/java_resources/." "$OUTPUT_DIR/"
cd "$OUTPUT_DIR"; "$JAR" cf "$OUTPUT_JAR" .; cd "$PROJECT_DIR"
echo "Output: $OUTPUT_JAR"
echo "Build ID: $BUILD_ID"
