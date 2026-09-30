#!/bin/bash
#
# Build the canonical CarPlay Java patch — in Docker (no host JDK required).
#
#   ./scripts/build_java.sh
#
# Input:  java_patch/   +   ../../Tools/jxe2jar   (stock jar + OSGi libs)
# Output: build/carplay_hook.jar
#
# Compiles against MU1316-final.jar + OSGi, target 1.4 (jclfoun11 = Foundation 1.1),
# inside a pinned JDK 8 container so the build does not depend on a host JVM.
#
# NOTE: MU1316-final.jar is the author's own decompiled stock HMI jar
# (../../Tools/jxe2jar/out/). Yours may be named or located differently - point
# STOCK_JAR below and the CP line inside the container at your own stock jar.
set -e

[ "$#" -eq 0 ] || { echo "usage: ./scripts/build_java.sh"; exit 2; }

IMG=eclipse-temurin:8-jdk-jammy
SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
PROJECT_DIR="$(cd "$SCRIPT_DIR/.." && pwd)"
TOOLS_DIR="$(cd "$PROJECT_DIR/../../Tools/jxe2jar" && pwd)"

STOCK_JAR="$TOOLS_DIR/out/MU1316-final.jar"
[ -f "$STOCK_JAR" ] || { echo "ERROR: $STOCK_JAR not found"; exit 1; }
[ -d "$PROJECT_DIR/java_patch" ] || { echo "ERROR: java_patch/ not found"; exit 1; }

# Reproducible build identity is computed on the host (git lives here), passed into the container.
BUILD_ID_RAW=${CARPLAY_BUILD_ID:-$(git -C "$PROJECT_DIR" describe --always --dirty 2>/dev/null || echo unknown)}
BUILD_ID=$(printf '%s' "$BUILD_ID_RAW" | tr -cd 'A-Za-z0-9._-')

if command -v docker >/dev/null 2>&1 && docker info >/dev/null 2>&1; then
  USE_DOCKER=1
elif command -v sg >/dev/null 2>&1 && sg docker -c "docker info" >/dev/null 2>&1; then
  exec sg docker -c "$0 $*"
else
  USE_DOCKER=0
fi

if [ "$USE_DOCKER" -eq 1 ]; then
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
    CP="/tools/out/MU1316-final.jar:/tools/libs/org.osgi.framework-1.10.0.jar:/tools/libs/org.osgi.util.tracker-1.5.4.jar"

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
    # Compact generated metrics/Unicode tables (VC route text) live inside the jar.
    cp -R /src/java_resources/. "$OUT/"
    (cd "$OUT" && jar cf "$OUTJAR" .)
    rm -rf /src/build/java
  '
else
  if [ "$(uname)" = "Darwin" ]; then
    JDK_DIR="$TOOLS_DIR/jvms/zulu8.78.0.19-ca-jdk8.0.412-macosx_aarch64/zulu-8.jdk/Contents/Home"
  else
    JDK_DIR="$TOOLS_DIR/jvms/zulu8.78.0.19-ca-jdk8.0.412-linux_x64"
  fi
  [ -x "$JDK_DIR/bin/javac" ] || { echo "ERROR: Neither docker nor $JDK_DIR/bin/javac found"; exit 1; }
  echo "=== CarPlay Java Patch Build (Local JDK $JDK_DIR) ==="

  SRC="$PROJECT_DIR/java_patch"
  OUT="$PROJECT_DIR/build/java/classes"
  OUTJAR="$PROJECT_DIR/build/carplay_hook.jar"
  CP="$STOCK_JAR:$TOOLS_DIR/libs/org.osgi.framework-1.10.0.jar:$TOOLS_DIR/libs/org.osgi.util.tracker-1.5.4.jar"

  rm -rf "$PROJECT_DIR/build/java"
  mkdir -p "$OUT" "$PROJECT_DIR/build"
  SRCLIST=$(mktemp)
  find "$SRC" -name "*.java" -type f > "$SRCLIST"
  echo "Compiling $(wc -l < "$SRCLIST" | tr -d " ") files (target 1.4)..."

  GEN=$(mktemp -d)
  mkdir -p "$GEN/com/luka/carplay/core"
  sed "s/@BUILD_ID@/$BUILD_ID/g" "$SRC/com/luka/carplay/core/CarPlayApp.java" > "$GEN/com/luka/carplay/core/CarPlayApp.java"
  grep -v "/com/luka/carplay/core/CarPlayApp.java$" "$SRCLIST" > "$SRCLIST.tmp"; mv "$SRCLIST.tmp" "$SRCLIST"
  printf "%s\n" "$GEN/com/luka/carplay/core/CarPlayApp.java" >> "$SRCLIST"

  "$JDK_DIR/bin/javac" -source 1.4 -target 1.4 -cp "$CP" -sourcepath "$GEN:$SRC" -d "$OUT" -Xlint:-options @"$SRCLIST"
  # Compact generated metrics/Unicode tables (VC route text) live inside the jar.
  cp -R "$PROJECT_DIR/java_resources/." "$OUT/"
  (cd "$OUT" && "$JDK_DIR/bin/jar" cf "$OUTJAR" .)
  rm -rf "$PROJECT_DIR/build/java" "$GEN" "$SRCLIST"
fi

echo "Output: $PROJECT_DIR/build/carplay_hook.jar"
echo "Build ID: $BUILD_ID"
