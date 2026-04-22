#!/bin/bash
#
# Build script for CarPlay Java patch
# Compiles against original lsd.jar + OSGi jars for BAP/DSI class access
# Target: Java 1.2 for MU1316 compatibility
#
set -e

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
TOOLS_DIR="$SCRIPT_DIR/../jxe2jar"

# JDK
JAVA_HOME="$TOOLS_DIR/jvms/zulu8.78.0.19-ca-jdk8.0.412-macosx_aarch64/zulu-8.jdk/Contents/Home"
JAVAC="$JAVA_HOME/bin/javac"
JAR="$JAVA_HOME/bin/jar"

# Dependencies
LSD_JAR="$TOOLS_DIR/out/lsd.jar"
OSGI_LIBS="$TOOLS_DIR/libs"
OSGI_CP="$OSGI_LIBS/org.osgi.framework-1.10.0.jar:$OSGI_LIBS/org.osgi.util.tracker-1.5.4.jar"

SRC_DIR="$SCRIPT_DIR/java_patch"
BUILD_DIR="$SCRIPT_DIR/build"
OUTPUT_DIR="$BUILD_DIR/java/classes"
OUTPUT_JAR="$BUILD_DIR/carplay_hook.jar"

echo "=== CarPlay Hook Java Builder ==="

# Check for JDK
if [ ! -x "$JAVAC" ]; then
    echo "ERROR: javac not found at $JAVAC"
    exit 1
fi

# Build classpath
CLASSPATH="$LSD_JAR:$OSGI_CP"
if [ ! -f "$LSD_JAR" ]; then
    echo "ERROR: lsd.jar not found"
    exit 1
fi

# Clean output
rm -rf "$OUTPUT_DIR"
mkdir -p "$OUTPUT_DIR" "$BUILD_DIR"

# Generate BUILD_ID (date + short git hash)
BUILD_ID="$(date +%Y-%m-%d)-$(git -C "$SCRIPT_DIR" rev-parse --short HEAD 2>/dev/null || echo 'nogit')"
echo "Build ID: $BUILD_ID"

# Inject BUILD_ID into CarPlayHook.java (restore on exit)
HOOK_FILE="$SRC_DIR/com/luka/carplay/CarPlayHook.java"
if [ -f "$HOOK_FILE" ]; then
    sed -i.bak "s/@BUILD_ID@/$BUILD_ID/g" "$HOOK_FILE"
    trap 'mv "$HOOK_FILE.bak" "$HOOK_FILE"' EXIT
fi

# Find Java files (use @file to handle $ in filenames)
SOURCES_LIST=$(mktemp)
find "$SRC_DIR" -name '*.java' -type f ! -path '*/out/*' > "$SOURCES_LIST"
FILE_COUNT=$(wc -l < "$SOURCES_LIST" | tr -d ' ')
echo "Compiling $FILE_COUNT files (target 1.2)..."

# Compile
"$JAVAC" -source 1.2 -target 1.2 \
    -cp "$CLASSPATH" \
    -sourcepath "$SRC_DIR" \
    -d "$OUTPUT_DIR" \
    -Xlint:-options \
    @"$SOURCES_LIST"

rm -f "$SOURCES_LIST"

# Create JAR
cd "$OUTPUT_DIR"
"$JAR" cf "$OUTPUT_JAR" .
cd "$SCRIPT_DIR"

echo ""
echo "Output: $OUTPUT_JAR"
echo "Classes:"
find "$OUTPUT_DIR" -name '*.class' | sed "s|$OUTPUT_DIR/||" | sort
