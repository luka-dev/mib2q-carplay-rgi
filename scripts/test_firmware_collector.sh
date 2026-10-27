#!/bin/sh
# Host regression test for the read-only M.I.B. firmware collector.
set -eu
ROOT=$(cd "$(dirname "$0")/.." && pwd)
TMP_BASE=${TMPDIR:-/tmp}
T=$(mktemp -d "$TMP_BASE/aa-firmware-collector.XXXXXX")
trap 'rm -rf "$T"' EXIT

fail() { echo "FAIL: $1"; exit 1; }

UNIT=$T/unit
CARD=$T/card
mkdir -p "$UNIT/mnt/app/eso/bin/apps" "$UNIT/mnt/app/eso/lib" "$UNIT/ifs" \
         "$UNIT/mnt/system/etc/eso/production" "$CARD"
cp -R "$ROOT/collect_AndroidAutoFirmware_MoreIncredibleBash/mod" "$CARD/"

printf 'gal-binary\000payload\n' > "$UNIT/mnt/app/eso/bin/apps/gal"
printf 'receiver-binary\000payload\n' > "$UNIT/mnt/app/eso/lib/libautoreceiver.so"
printf 'jxe-binary\000payload\n' > "$UNIT/ifs/lsd.jxe"
printf '{"children":{"gal":{}}}\n' > "$UNIT/mnt/system/etc/eso/production/smartphone_integrator.json"
printf '{"video":{"sink":{}}}\n' > "$UNIT/mnt/system/etc/eso/production/gal.json"

run_collector() {
    COLLECT_ROOT=$UNIT COLLECT_CARD=$CARD COLLECT_NO_FORWARD=1 \
        /bin/sh "$CARD/mod/custom.sh" >/dev/null
}

run_collector || fail "first collection failed"
OUT=$CARD/android_auto_firmware_backup/001
[ -f "$OUT/status.txt" ] && grep -q '^COMPLETE:' "$OUT/status.txt" \
    || fail "first collection is not complete"
cmp -s "$UNIT/mnt/app/eso/bin/apps/gal" "$OUT/binaries/gal" \
    || fail "gal differs"
cmp -s "$UNIT/mnt/app/eso/lib/libautoreceiver.so" "$OUT/binaries/libautoreceiver.so" \
    || fail "libautoreceiver differs"
cmp -s "$UNIT/ifs/lsd.jxe" "$OUT/binaries/lsd.jxe" \
    || fail "lsd.jxe differs"
grep -q '^OK|.*/mnt/app/eso/bin/apps/gal|binaries/gal|' "$OUT/manifest.txt" \
    || fail "manifest lacks gal success"

run_collector || fail "second collection failed"
[ -f "$CARD/android_auto_firmware_backup/002/status.txt" ] \
    || fail "second collection did not use 002"

rm "$UNIT/mnt/app/eso/lib/libautoreceiver.so"
if run_collector; then
    fail "missing required binary returned success"
fi
OUT3=$CARD/android_auto_firmware_backup/003
grep -q '^INCOMPLETE:' "$OUT3/status.txt" || fail "missing file did not mark run incomplete"
grep -q '^MISSING|.*/mnt/app/eso/lib/libautoreceiver.so|binaries/libautoreceiver.so|0' \
    "$OUT3/manifest.txt" || fail "missing receiver not recorded"

echo "firmware collector: exact copies, numbered runs, manifest and missing-file refusal PASS"
