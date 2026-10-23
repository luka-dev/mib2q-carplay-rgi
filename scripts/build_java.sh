#!/bin/bash
#
# Build the CarPlay Java patch, one jar per firmware family.
#
#   ./scripts/build_java.sh
#
# Input:  java_patch/ (+ java_patch_cn/*.patch for the CN variant)
# Output: build/carplay_hook.jar     US/EU (MU1316 P5087, MU1329 P5092: identical stock classes)
#         build/carplay_hook_cn.jar  CN    (MU1003 K1004)
#
# The jar replaces whole stock classes, and six of them differ on CN (other superclass,
# constructors, DSI resource API).  The CN variant is java_patch/ with the CN stock delta
# applied strictly (patch -F0: a hunk that no longer fits fails the build instead of
# landing in the wrong place).  Each variant compiles against its own firmware's classes
# from java_stock/, target 1.4 (jclfoun11 = Foundation 1.1).  The installer picks one.
set -e

[ "$#" -eq 0 ] || { echo "usage: ./scripts/build_java.sh"; exit 2; }

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
PROJECT_DIR="$(cd "$SCRIPT_DIR/.." && pwd)"
. "$PROJECT_DIR/scripts/java_env.sh"; TOOLS_DIR=$JXE2JAR
JAVA_HOME="$JAVA8_HOME"
JAVAC="$JAVA_HOME/bin/javac"
JAR="$JAVA_HOME/bin/jar"

BUILD_DIR="$PROJECT_DIR/build"
echo "=== CarPlay Java Patch Build ==="
[ -x "$JAVAC" ] || { echo "ERROR: javac not found at $JAVAC"; exit 1; }
[ -d "$PROJECT_DIR/java_patch" ] || { echo "ERROR: java_patch not found"; exit 1; }
mkdir -p "$BUILD_DIR"
BUILD_ID_RAW=${CARPLAY_BUILD_ID:-$(git -C "$PROJECT_DIR" describe --always --dirty 2>/dev/null || echo unknown)}

WORK=$(mktemp -d)
trap 'rm -rf "$WORK"' EXIT HUP INT TERM

# build_variant <name> <stock jar> <output jar> [patch dir]
build_variant() {
VARIANT=$1; STOCK_JAR=$2; OUTPUT_JAR=$3; PATCH_DIR=${4:-}
[ -f "$STOCK_JAR" ] || { echo "ERROR: $STOCK_JAR not found"; exit 1; }
echo "--- $VARIANT: against $(basename "$STOCK_JAR")"
SRC_DIR="$WORK/$VARIANT-src"
OUTPUT_DIR="$WORK/$VARIANT-classes"
mkdir -p "$SRC_DIR" "$OUTPUT_DIR"
cp -R "$PROJECT_DIR/java_patch/." "$SRC_DIR/"
if [ -n "$PATCH_DIR" ]; then
    for p in "$PATCH_DIR"/*.patch; do
        patch -p1 -F0 -s -d "$SRC_DIR" < "$p" ||
            { echo "ERROR: $p no longer applies to java_patch/ - update it (java_patch_cn/README.md)"; exit 1; }
    done
fi
CLASSPATH="$STOCK_JAR:$OSGI_CP"
SOURCES_LIST="$WORK/$VARIANT-sources"
find "$SRC_DIR" -name '*.java' -type f > "$SOURCES_LIST"
echo "Compiling $(wc -l < "$SOURCES_LIST" | tr -d ' ') files (target 1.4)..."

# Compile a generated CarPlayApp copy with a real, reproducible build identity. Never edit the
# source tree: replace only the explicit source-list entry for this build.
BUILD_ID=$(printf '%s' "$BUILD_ID_RAW" | tr -cd 'A-Za-z0-9._-')
[ "$VARIANT" = us ] || BUILD_ID="$BUILD_ID-$VARIANT"
GEN_SRC_DIR="$WORK/$VARIANT-gen"
GEN_APP="$GEN_SRC_DIR/com/luka/carplay/core/CarPlayApp.java"
mkdir -p "$(dirname "$GEN_APP")"
sed "s/@BUILD_ID@/$BUILD_ID/g" "$SRC_DIR/com/luka/carplay/core/CarPlayApp.java" > "$GEN_APP"
grep -v '/com/luka/carplay/core/CarPlayApp.java$' "$SOURCES_LIST" > "$SOURCES_LIST.tmp"
mv "$SOURCES_LIST.tmp" "$SOURCES_LIST"
printf '%s\n' "$GEN_APP" >> "$SOURCES_LIST"

"$JAVAC" -encoding UTF-8 -source 1.4 -target 1.4 \
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
write_receipt
}

write_receipt() {
python3 - "$OUTPUT_JAR" "$BUILD_ID" "$PROJECT_DIR" <<'PY'
import hashlib, json, pathlib, subprocess, sys
jar, build_id, root = pathlib.Path(sys.argv[1]), sys.argv[2], sys.argv[3]
receipt = {
    "jar": str(jar), "bytes": jar.stat().st_size,
    "sha256": hashlib.sha256(jar.read_bytes()).hexdigest(), "build_id": build_id,
    "commit": subprocess.check_output(["git", "-C", root, "rev-parse", "HEAD"], text=True).strip(),
    "source_dirty": bool(subprocess.check_output([
        "git", "-C", root, "status", "--porcelain", "--", "java_patch", "java_patch_cn", "java_resources"])),
    "deployed": False,
}
jar.with_name(jar.stem + "-build.json").write_text(json.dumps(receipt, indent=2) + "\n")
PY
}

build_variant us "$PROJECT_DIR/java_stock/US_MU1316_lsd.jar" "$BUILD_DIR/carplay_hook.jar"
build_variant cn "$PROJECT_DIR/java_stock/CN_MU1003_lsd.jar" "$BUILD_DIR/carplay_hook_cn.jar" "$PROJECT_DIR/java_patch_cn"
