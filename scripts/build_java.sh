#!/bin/bash
#
# Build the canonical CarPlay Java patch — in Docker (no host JDK required).
#
#   ./scripts/build_java.sh
#
# Input:  java_patch/   +   ../../Tools/jxe2jar   (stock jar + OSGi libs)
# Output: build/carplay_hook.jar
#
# Compiles against MU1316-combined-final.jar + OSGi, target 1.4 (jclfoun11 = Foundation 1.1),
# inside a pinned JDK 8 container so the build does not depend on a host JVM.
set -e

[ "$#" -eq 0 ] || { echo "usage: ./scripts/build_java.sh"; exit 2; }

IMG=eclipse-temurin:8-jdk-jammy
SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
PROJECT_DIR="$(cd "$SCRIPT_DIR/.." && pwd)"
TOOLS_DIR="$(cd "$PROJECT_DIR/../../Tools/jxe2jar" && pwd)"

STOCK_JAR="$TOOLS_DIR/out/MU1316-combined-final.jar"
[ -f "$STOCK_JAR" ] || { echo "ERROR: $STOCK_JAR not found"; exit 1; }
[ -d "$PROJECT_DIR/java_patch" ] || { echo "ERROR: java_patch/ not found"; exit 1; }

# Reproducible build identity is computed on the host (git lives here), passed into the container.
BUILD_ID_RAW=${CARPLAY_BUILD_ID:-$(git -C "$PROJECT_DIR" describe --always --dirty 2>/dev/null || echo unknown)}
BUILD_ID=$(printf '%s' "$BUILD_ID_RAW" | tr -cd 'A-Za-z0-9._-')

echo "=== CarPlay Java Patch Build (Docker $IMG) ==="

docker run --rm \
  -v "$PROJECT_DIR":/src \
  -v "$TOOLS_DIR":/tools:ro \
  -e BUILD_ID="$BUILD_ID" \
  "$IMG" bash -c '
  set -e
  SRC=/src/java_patch
  OUT=/src/build/java/classes
  OUTJAR=/src/build/carplay_hook.jar
  CP="/tools/out/MU1316-combined-final.jar:/tools/libs/org.osgi.framework-1.10.0.jar:/tools/libs/org.osgi.util.tracker-1.5.4.jar"

  rm -rf /src/build/java; mkdir -p "$OUT" /src/build
  SRCLIST=$(mktemp)
  find "$SRC" -name "*.java" -type f > "$SRCLIST"
  echo "Compiling $(wc -l < "$SRCLIST" | tr -d " ") files (target 1.4)..."

  # Generate a CarPlayApp copy with the real BUILD_ID; never edit the source tree.
  GEN=$(mktemp -d)
  mkdir -p "$GEN/com/luka/carplay/core"
  sed "s/@BUILD_ID@/$BUILD_ID/g" "$SRC/com/luka/carplay/core/CarPlayApp.java" > "$GEN/com/luka/carplay/core/CarPlayApp.java"
  grep -v "/com/luka/carplay/core/CarPlayApp.java$" "$SRCLIST" > "$SRCLIST.tmp"; mv "$SRCLIST.tmp" "$SRCLIST"
  printf "%s\n" "$GEN/com/luka/carplay/core/CarPlayApp.java" >> "$SRCLIST"

  javac -source 1.4 -target 1.4 -cp "$CP" -sourcepath "$GEN:$SRC" -d "$OUT" -Xlint:-options @"$SRCLIST"
  (cd "$OUT" && jar cf "$OUTJAR" .)
  rm -rf /src/build/java
'

echo "Output: $PROJECT_DIR/build/carplay_hook.jar"
echo "Build ID: $BUILD_ID"
