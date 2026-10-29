#!/usr/bin/env bash
# Build si_adapter.so (QNX 6.5 ARM) in the pinned docker toolchain.
# Mirrors scripts/build_hook_common.sh conventions: no __thread/emutls,
# compiler-only .init_array, hidden visibility with the si_adapter.map
# export allowlist. Output: build/si-wireless-stage1/libsi_adapter.so.
set -euo pipefail

REPO="$(cd "$(dirname "$0")/../.." && pwd)"
SRC="$REPO/wireless/si"
OUT="$REPO/build/si-wireless-stage1"
IMG="qnx65-armv7-toolchain:latest"
OUT_NAME="libsi_adapter.so"
SI_BUILD_STAGE2=0
CANDIDATE_FLAGS=()
for arg in "$@"; do
    case "$arg" in
        --diagnostic) OUT_NAME="libsi_adapter.diagnostic.so" ;;
        --stage2) SI_BUILD_STAGE2=1; OUT="$REPO/build/si-wireless-stage2"; CANDIDATE_FLAGS=(--stage2) ;;
        *) echo "usage: $0 [--diagnostic] [--stage2]" >&2; exit 2 ;;
    esac
done

mkdir -p "$OUT"
# Generate the exact immutable runtime fingerprint from the current candidate.
python3 "$SRC/build_candidate.py" --apply --outdir "$OUT" "${CANDIDATE_FLAGS[@]}"
# The docker command runs from a generated file so the awk/grep quoting stays
# readable; the heredoc is quoted ('EOF') and passes through verbatim.
cat > "$OUT/.docker_build.sh" <<'EOF'
set -e
export PATH=/opt/qnx650/host/linux/x86/usr/bin:$PATH
export QNX_HOST=/opt/qnx650/host/linux/x86 QNX_TARGET=/opt/qnx650/target/qnx6
CC=arm-unknown-nto-qnx6.5.0eabi-gcc
NM=arm-unknown-nto-qnx6.5.0eabi-nm
READELF=arm-unknown-nto-qnx6.5.0eabi-readelf
OBJ=/tmp/siobj
rm -rf $OBJ; mkdir -p $OBJ
OUTD=/src/build/si-wireless-stage1
[ "${SI_BUILD_STAGE2:-0}" != 1 ] || OUTD=/src/build/si-wireless-stage2
SO=$OUTD/libsi_adapter.so
DIAG_FLAGS=
if [ "${SI_BUILD_DIAGNOSTIC:-0}" = 1 ]; then
    SO=$OUTD/libsi_adapter.diagnostic.so
    DIAG_FLAGS=-DSI_ADAPTER_DIAGNOSTIC
fi

FLAGS="-O2 -fPIC -fvisibility=hidden -std=gnu99 -Wall -Wextra -Werror -mfpu=neon -mfloat-abi=softfp"
EXTRA_OBJECTS=
if [ "${SI_BUILD_STAGE2:-0}" = 1 ]; then
    FLAGS="$FLAGS -DSI_STAGE2"
    for module in si_wire si_controller si_native si_media_watch; do
        $CC $FLAGS -I/src/wireless/si -c /src/wireless/si/$module.c -o $OBJ/$module.o
        EXTRA_OBJECTS="$EXTRA_OBJECTS $OBJ/$module.o"
    done
fi
$CC $FLAGS -I/src/wireless/si -c /src/wireless/si/si_envv.c -o $OBJ/si_envv.o
$CC $FLAGS -I/src/wireless/si -I$OUTD -c /src/wireless/si/si_runtime_guard.c -o $OBJ/si_runtime_guard.o
$CC $FLAGS $DIAG_FLAGS -I/src/wireless/si -c /src/wireless/si/si_adapter.c -o $OBJ/si_adapter.o
$CC $FLAGS -shared \
    -Wl,--version-script=/src/wireless/si/si_adapter.map \
    -Wl,-soname,libsi_adapter.so \
    -o $SO $OBJ/si_envv.o $OBJ/si_runtime_guard.o $OBJ/si_adapter.o $EXTRA_OBJECTS

echo "=== guards ==="
if $NM "$SO" | grep -q emutls; then
    echo "FAIL: emutls symbol present"; exit 1
fi
INIT=$($READELF -SW "$SO" | awk '$2==".init_array" {print $6}')
INIT_CLEAN=$(printf '%d' "0x$INIT" 2>/dev/null || echo 0)
echo ".init_array size: $INIT ($INIT_CLEAN bytes)"
if [ "$INIT_CLEAN" != "4" ]; then
    echo "FAIL: unexpected .init_array"; exit 1
fi
echo "=== dynamic exports ==="
$NM -D --defined-only "$SO"
$NM -D --defined-only "$SO" | awk '{print $3}' | sort > $OBJ/actual_exports
printf '%s\n' spawn system open open64 \
    _ZN8servmngt11Application16reportAppRunModeEj \
    _ZN8servmngt11Application19reportSystemRunModeENS_13SystemRunModeEb \
    _ZN8servmngt11Application17setWatchdogPeriodEj > $OBJ/expected_exports
[ "${SI_BUILD_DIAGNOSTIC:-0}" != 1 ] || echo slogf >> $OBJ/expected_exports
sort -o $OBJ/expected_exports $OBJ/expected_exports
diff -u $OBJ/expected_exports $OBJ/actual_exports
echo "=== undefined (must be libc/dl only) ==="
$NM -D -u "$SO" | awk '{print $2}' | sort
EOF

SI_BUILD_DIAGNOSTIC=0
[ "$OUT_NAME" = libsi_adapter.diagnostic.so ] && SI_BUILD_DIAGNOSTIC=1
docker run --rm --platform=linux/amd64 -e SI_BUILD_STAGE2="$SI_BUILD_STAGE2" -e SI_BUILD_DIAGNOSTIC="$SI_BUILD_DIAGNOSTIC" -v "$REPO":/src "$IMG" bash "/src/${OUT#"$REPO/"}/.docker_build.sh"

echo "built: $OUT/$OUT_NAME"
shasum -a 256 "$OUT/$OUT_NAME"
python3 - "$REPO" "$OUT_NAME" "$OUT" "$SI_BUILD_STAGE2" <<'PY'
import hashlib, json, sys
from pathlib import Path
root, name = Path(sys.argv[1]), sys.argv[2]
out = Path(sys.argv[3])
digest = lambda p: hashlib.sha256(p.read_bytes()).hexdigest()
inputs = ["si_adapter.c", "si_envv.c", "si_envv.h", "si_runtime_guard.c",
          "si_runtime_guard.h", "si_protocol.h", "si_adapter.map", "build_si_adapter.sh"]
if sys.argv[4] == "1":
    inputs += ["si_wire.c", "si_wire.h", "si_controller.c", "si_media_watch.c", "si_media_watch.h", "si_controller.h", "si_child_state.h", "si_native.c", "si_native.h"]
receipt = {"adapter": name, "sha256": digest(out / name),
           "diagnostic": name.endswith(".diagnostic.so"),
           "candidate_sha256": digest(out / "smartphone_integrator.si_wireless_stage1"),
           "guard_sha256": digest(out / "si_candidate_guard.h"),
           "sources": {n: digest(root / "wireless/si" / n) for n in inputs},
           "qemu_validated": False, "hu_deployed": False}
(out / (name + ".receipt.json")).write_text(json.dumps(receipt, indent=2) + "\n")
PY
