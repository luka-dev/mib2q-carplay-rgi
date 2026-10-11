#!/bin/bash
# Host launcher: upstream defaults on macOS/Linux, explicit inputs on Windows.
set -euo pipefail
ROOT=$(cd "$(dirname "$0")/../.." && pwd)
TOOLS=${CARPLAY_TOOLS_DIR:-$ROOT/../../Tools/jxe2jar}
mode=${1:-build}; shift || true
default_stock=0
if [ -z "${STOCK_JAR:-}" ]; then STOCK_JAR="$TOOLS/out/MU1316-final.jar"; default_stock=1; fi
CARPLAY_DEPENDENCIES=${CARPLAY_DEPENDENCIES:-$TOOLS/libs}
JAVA_OUTPUT=${JAVA_OUTPUT:-$ROOT/build}
CARPLAY_BUILD_ID=${CARPLAY_BUILD_ID:-$(git -C "$ROOT" describe --always --dirty 2>/dev/null || echo unknown)}
CARPLAY_BUILD_ID=$(printf '%s' "$CARPLAY_BUILD_ID" | tr -cd 'A-Za-z0-9._-')
if [ "$default_stock" = 1 ]; then
    # The car's class library is optional for building; use it when it is present.
    default_boot=$TOOLS/libs/jcl/MHI2Q_US_AUG22_P5087_MU1316/jcl.jar
    if [ -z "${STOCK_BOOT_JAR:-}" ] && [ -f "$default_boot" ]; then STOCK_BOOT_JAR=$default_boot; fi
    STOCK_RUNTIME_JAR=${STOCK_RUNTIME_JAR:-$TOOLS/out/MU1316-combined.jar}
fi
host_path() {
    case "$(uname -s)" in
        MINGW*|MSYS*|CYGWIN*) cygpath -am "$1" ;;
        *) (cd "$(dirname "$1")" && printf '%s/%s\n' "$PWD" "$(basename "$1")") ;;
    esac
}
mount_file() {
    local input=$1 target=$2 envname=$3
    [ -f "$input" ] || { echo "Missing $envname input: $input" >&2; exit 1; }
    local dest="$target/$(basename "$input")"
    args+=(--mount "type=bind,source=$(host_path "$input"),target=$dest,readonly" -e "$envname=$dest")
}
mkdir -p "$JAVA_OUTPUT"
args=(run --rm --network none --env TZ=UTC --env LC_ALL=C
    --mount "type=bind,source=$(host_path "$ROOT"),target=/src,readonly"
    --mount "type=bind,source=$(host_path "$CARPLAY_DEPENDENCIES"),target=/deps,readonly"
    --mount "type=bind,source=$(host_path "$JAVA_OUTPUT"),target=/out"
    -e SOURCE_ROOT=/src -e CARPLAY_DEPENDENCIES=/deps -e JAVA_OUTPUT=/out -e "CARPLAY_BUILD_ID=$CARPLAY_BUILD_ID")
case "$(uname -s)" in
    MINGW*|MSYS*|CYGWIN*) export MSYS_NO_PATHCONV=1 ;;
    *) args+=(--user "$(id -u):$(id -g)") ;;
esac
mount_file "$STOCK_JAR" /inputs/stock STOCK_JAR
if [ -n "${STOCK_BOOT_JAR:-}" ]; then mount_file "$STOCK_BOOT_JAR" /inputs/boot BOOT_JAR; fi
if [ "$mode" = test ]; then
    if [ -n "${STOCK_RUNTIME_JAR:-}" ]; then mount_file "$STOCK_RUNTIME_JAR" /inputs/runtime STOCK_RUNTIME_JAR; fi
    case "${1:-all}" in
        all|transports|pdc|linkage)
            mount_file "${ASM_JAR:-$TOOLS/tools/uninline/lib/asm-9.7.jar}" /inputs/asm ASM_JAR
            mount_file "${ASM_TREE_JAR:-$TOOLS/tools/uninline/lib/asm-tree-9.7.jar}" /inputs/asm-tree ASM_TREE_JAR ;;
    esac
    if [ -n "${RGD_CONTRACT_FRAMES:-}" ]; then args+=(--mount "type=bind,source=$(host_path "$RGD_CONTRACT_FRAMES"),target=/frames,readonly" -e RGD_CONTRACT_FRAMES=/frames); fi
    if [ -n "${VC_UNICODE_TEST_DIR:-}" ]; then args+=(--mount "type=bind,source=$(host_path "$VC_UNICODE_TEST_DIR"),target=/unicode-tests,readonly" -e VC_UNICODE_TEST_DIR=/unicode-tests); fi
    if [ -n "${CARPLAY_HOOK_JAR:-}" ]; then mount_file "$CARPLAY_HOOK_JAR" /inputs/patch CARPLAY_HOOK_JAR; fi
    if [ -n "${JAVA_TEST_JOBS:-}" ]; then args+=(-e "JAVA_TEST_JOBS=$JAVA_TEST_JOBS"); fi
fi
image=${CARPLAY_JAVA_IMAGE:-eclipse-temurin:8-jdk-jammy}
docker image inspect "$image" >/dev/null 2>&1 || docker pull "$image"
image=$(docker image inspect --format '{{.Id}}' "$image")
printf '%s\n' "$image" > "$JAVA_OUTPUT/java-image-id.txt"
case "$mode" in
    build) exec docker "${args[@]}" "$image" bash /src/scripts/java/build.sh ;;
    build-tests) exec docker "${args[@]}" "$image" bash /src/scripts/test_java_build.sh --inside ;;
    test) exec docker "${args[@]}" -e "JAVA_SKIP_BUILD=${JAVA_SKIP_BUILD:-0}" "$image" bash -ec '
        [ "$JAVA_SKIP_BUILD" = 1 ] || bash /src/scripts/java/build.sh
        exec bash /src/scripts/java/test.sh "$@"
        ' -- "$@" ;;
    *) echo "Unknown Java action: $mode" >&2; exit 2 ;;
esac
