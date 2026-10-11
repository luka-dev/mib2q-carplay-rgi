#!/bin/bash
# Runs inside Java 8 Docker; all host launchers use this compiler procedure.
set -euo pipefail
export LC_ALL=C TZ=UTC
: "${SOURCE_ROOT:?}" "${STOCK_JAR:?}" "${CARPLAY_DEPENDENCIES:?}" "${JAVA_OUTPUT:?}"
BUILD_ID=${CARPLAY_BUILD_ID:-unknown}
[[ "$BUILD_ID" =~ ^[A-Za-z0-9._-]+$ ]] || { echo 'Invalid build ID' >&2; exit 1; }
[[ $(javac -version 2>&1) == javac\ 1.8.* ]] || { echo 'Java 8 is required' >&2; exit 1; }
work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT
mkdir -p "$work/classes" "$work/tools" "$work/generated/com/luka/carplay/core" "$JAVA_OUTPUT"
# Keep javac's small-file traffic on Linux storage rather than the host bind mount.
cp -R "$SOURCE_ROOT/java_patch" "$work/src"
cp "$STOCK_JAR" "$work/stock.jar"
boot="$work/stock.jar"
if [ -n "${BOOT_JAR:-}" ] && [ "$BOOT_JAR" != "$STOCK_JAR" ]; then
    cp "$BOOT_JAR" "$work/boot.jar"; boot="$work/boot.jar"
fi
jar tf "$boot" > "$work/boot-entries"
bootargs=(-bootclasspath "$boot")
if ! grep -qx 'java/lang/Object.class' "$work/boot-entries"; then
    if [ -n "${BOOT_JAR:-}" ]; then
        echo 'Stock Java library missing. Supply STOCK_BOOT_JAR (the car JCL), or a stock JAR containing it.' >&2; exit 1
    fi
    # As the original build: JDK 8's classes. The car library only adds a stricter API check.
    echo 'Boot classes: JDK 8 (no car Java library supplied; STOCK_BOOT_JAR enables that check)'
    bootargs=()
fi
app=com/luka/carplay/core/CarPlayApp.java
grep -q '@BUILD_ID@' "$work/src/$app" || { echo 'Missing BUILD_ID token' >&2; exit 1; }
sed "s/@BUILD_ID@/$BUILD_ID/g" "$work/src/$app" > "$work/generated/$app"
find "$work/src" -type f -name '*.java' ! -path "*/$app" -print | LC_ALL=C sort > "$work/source-paths"
printf '%s\n' "$work/generated/$app" >> "$work/source-paths"
while IFS= read -r path; do printf '"%s"\n' "$path"; done < "$work/source-paths" > "$work/sources.txt"
framework=$CARPLAY_DEPENDENCIES/org.osgi.framework-1.10.0.jar
tracker=$CARPLAY_DEPENDENCIES/org.osgi.util.tracker-1.5.4.jar
echo "Compiling $(wc -l < "$work/sources.txt") Java files (build $BUILD_ID)..."
javac -encoding UTF-8 -source 1.4 -target 1.4 "${bootargs[@]}" \
    -cp "$work/stock.jar:$framework:$tracker" -sourcepath "$work/generated:$work/src" \
    -d "$work/classes" -Xlint:-options @"$work/sources.txt"
resources="$SOURCE_ROOT/java_resources"
[ -f "$resources/com/luka/carplay/rgd/vc-text.bin" ] || { echo 'Missing vc-text.bin' >&2; exit 1; }
while IFS= read -r -d '' resource; do
    relative=${resource#"$resources/"}
    [[ "$relative" != *.class ]] && [ ! -e "$work/classes/$relative" ] || {
        echo "Resource collides with compiled output: $relative" >&2; exit 1;
    }
done < <(find "$resources" -type f -print0)
cp -R "$resources/." "$work/classes/"
javac -d "$work/tools" "$SOURCE_ROOT/scripts/java/PackageJar.java"
java -cp "$work/tools" PackageJar "$work/classes" "$work/carplay_hook.jar"
cp "$work/carplay_hook.jar" "$JAVA_OUTPUT/carplay_hook.jar"
javac -version > "$JAVA_OUTPUT/javac-version.txt" 2>&1
