#!/usr/bin/env bash
set -euo pipefail

usage() {
    echo "usage: $0 --stock-jar PATH [--compile-jar PATH] [--profile ID] [--firmware-source DIR]" >&2
    exit 2
}

STOCK=
COMPILE=
SOURCE=
PROFILE=
while [ "$#" -gt 0 ]; do
    case "$1" in
        --stock-jar) [ "$#" -ge 2 ] || usage; STOCK=$2; shift 2 ;;
        --compile-jar) [ "$#" -ge 2 ] || usage; COMPILE=$2; shift 2 ;;
        --profile) [ "$#" -ge 2 ] || usage; PROFILE=$2; shift 2 ;;
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

CHECK_ARGS=(--stock-jar "$STOCK" --compile-jar "$COMPILE")
if [ -n "$PROFILE" ]; then
    case "$PROFILE" in [!A-Za-z0-9]*|*[!A-Za-z0-9_.-]*) echo "ERROR: invalid firmware profile id" >&2; exit 1;; esac
    PROFILE_PATH="$ROOT/firmware/profiles/$PROFILE.json"
    [ -f "$PROFILE_PATH" ] || { echo "ERROR: unknown firmware profile $PROFILE" >&2; exit 1; }
    CHECK_ARGS+=(--profile "$PROFILE_PATH")
fi
REPORT=$(python3 "$ROOT/scripts/check_firmware.py" "${CHECK_ARGS[@]}" --json)
PROFILE=$(printf '%s' "$REPORT" | python3 -c 'import json,sys; print(json.load(sys.stdin)["profile_id"])')
RESULT=$(printf '%s' "$REPORT" | python3 -c 'import json,sys; print(json.load(sys.stdin)["result"])')
[ "$RESULT" = validated-profile ] || [ "$RESULT" = java-build-compatible ] || [ "$RESULT" = java-port-ready ] || {
    printf '%s\n' "$REPORT"; echo "ERROR: firmware Java inputs are not compatible" >&2; exit 1;
}
echo "Firmware profile: $PROFILE ($RESULT)"

MOUNTS=(-v "$ROOT":/src -v "$STOCK":/firmware/stock.jar:ro -v "$COMPILE":/firmware/compile.jar:ro)
ENV=(-e STOCK_JAR=/firmware/stock.jar -e COMPILE_JAR=/firmware/compile.jar -e FIRMWARE_PROFILE_ID="$PROFILE")
if [ -n "$SOURCE" ]; then
    SOURCE=$(cd "$SOURCE" && pwd)
    MOUNTS+=(-v "$SOURCE":/firmware/source:ro)
    ENV+=(-e FIRMWARE_SOURCE_DIR=/firmware/source)
fi

docker run --rm "${MOUNTS[@]}" "${ENV[@]}" eclipse-temurin:8-jdk-jammy \
    sh /src/android_auto/java/build_in_container.sh

mkdir -p "$ROOT/build/android_auto/profiles/$PROFILE"
cp "$ROOT/android_auto/java/build/mib2q_android_auto_cluster.jar" \
   "$ROOT/build/android_auto/profiles/$PROFILE/mib2q_android_auto_cluster.jar"
if [ "$PROFILE" = mhi2q_us_aug22_p3639_mu0918 ]; then
    cp "$ROOT/android_auto/java/build/mib2q_android_auto_cluster.jar" \
       "$ROOT/build/android_auto/mib2q_android_auto_cluster.jar"
fi
echo "Android Auto Java output: $ROOT/build/android_auto/profiles/$PROFILE/mib2q_android_auto_cluster.jar"
