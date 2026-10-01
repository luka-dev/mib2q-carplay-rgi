#!/usr/bin/env bash
set -euo pipefail

usage() {
    echo "usage: $0 --stock-jar PATH [--compile-jar PATH] [--firmware-source DIR]" >&2
    exit 2
}

STOCK=
COMPILE=
SOURCE=
while [ "$#" -gt 0 ]; do
    case "$1" in
        --stock-jar) [ "$#" -ge 2 ] || usage; STOCK=$2; shift 2 ;;
        --compile-jar) [ "$#" -ge 2 ] || usage; COMPILE=$2; shift 2 ;;
        --firmware-source) [ "$#" -ge 2 ] || usage; SOURCE=$2; shift 2 ;;
        *) usage ;;
    esac
done
[ -n "$STOCK" ] && [ -f "$STOCK" ] || usage
[ -n "$COMPILE" ] || COMPILE=$STOCK
[ -f "$COMPILE" ] || usage

ROOT=$(cd "$(dirname "$0")/.." && pwd)
STOCK=$(cd "$(dirname "$STOCK")" && pwd)/$(basename "$STOCK")
COMPILE=$(cd "$(dirname "$COMPILE")" && pwd)/$(basename "$COMPILE")

python3 "$ROOT/scripts/check_firmware.py" --stock-jar "$STOCK" --compile-jar "$COMPILE"

MOUNTS=(-v "$ROOT":/src -v "$STOCK":/firmware/stock.jar:ro -v "$COMPILE":/firmware/compile.jar:ro)
ENV=(-e STOCK_JAR=/firmware/stock.jar -e COMPILE_JAR=/firmware/compile.jar)
if [ -n "$SOURCE" ]; then
    SOURCE=$(cd "$SOURCE" && pwd)
    MOUNTS+=(-v "$SOURCE":/firmware/source:ro)
    ENV+=(-e FIRMWARE_SOURCE_DIR=/firmware/source)
fi

docker run --rm "${MOUNTS[@]}" "${ENV[@]}" eclipse-temurin:8-jdk-jammy \
    sh /src/android_auto/java/build_in_container.sh

mkdir -p "$ROOT/build/android_auto"
cp "$ROOT/android_auto/java/build/mib2q_android_auto_cluster.jar" \
   "$ROOT/build/android_auto/mib2q_android_auto_cluster.jar"
echo "Android Auto Java output: $ROOT/build/android_auto/mib2q_android_auto_cluster.jar"
