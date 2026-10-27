#!/bin/sh
# Read-only Android Auto firmware collector for Audi MHI2Q.
#
# Run from M.I.B. -> Advanced settings -> Run custom script. The script writes
# only to the SD card. It never remounts, edits, replaces, or deletes a file on
# the head unit.
#
# QNX 6.5 /bin/sh is pdksh. Keep this inside its portable subset.
set -u
PATH=/proc/boot:/bin:/usr/bin:/usr/sbin:/sbin:/mnt/app/armle/bin:/mnt/app/armle/usr/bin:/mnt/app/armle/sbin:/mnt/app/armle/usr/sbin
export PATH
unset LD_PRELOAD

case $0 in */*) SCRIPT_DIR=${0%/*} ;; *) SCRIPT_DIR=. ;; esac
SCRIPT_DIR=$(cd "$SCRIPT_DIR" && pwd) || exit 1

# M.I.B. normally launches custom scripts on RCC. The requested files live on
# MMX, so relaunch this same script there. COLLECT_ROOT/COLLECT_NO_FORWARD exist
# only for the host-side regression test.
if [ "${COLLECT_NO_FORWARD:-0}" != 1 ] && [ -z "${COLLECT_ROOT:-}" ] \
   && [ ! -d /mnt/app/eso/hmi/lsd ] && [ -d /net/mmx/mnt/app/eso/hmi/lsd ]; then
    exec on -f mmx /bin/sh "$SCRIPT_DIR/custom.sh" "$@"
fi

UNIT_ROOT=${COLLECT_ROOT:-}
CARD=${COLLECT_CARD:-${SCRIPT_DIR%/mod}}

# The card can be mounted read-only by M.I.B. Remount only the card itself.
# No head-unit filesystem is remounted.
can_write() {
    : > "$1/.aa_firmware_collect_write_test" 2>/dev/null || return 1
    rm -f "$1/.aa_firmware_collect_write_test"
}

if ! can_write "$CARD"; then
    for CANDIDATE in "$CARD" /fs/sda0 /fs/sdb0 /fs/usb0_0 \
                     /net/mmx/fs/sda0 /net/mmx/fs/sdb0 /net/mmx/fs/usb0_0; do
        [ -d "$CANDIDATE/mod" ] || continue
        mount -uw "$CANDIDATE" 2>/dev/null
        if can_write "$CANDIDATE"; then
            CARD=$CANDIDATE
            break
        fi
    done
fi
can_write "$CARD" || { echo "FAILED: SD card at $CARD is not writable"; exit 1; }

BASE=$CARD/android_auto_firmware_backup
mkdir -p "$BASE" || { echo "FAILED: cannot create $BASE"; exit 1; }

# Allocate a new numbered folder. Existing collections are never overwritten.
LAST=0
for EXISTING in "$BASE"/*; do
    VALUE=${EXISTING##*/}
    case $VALUE in ''|*[!0-9]*) continue ;; esac
    while :; do case $VALUE in 0?*) VALUE=${VALUE#0} ;; *) break ;; esac; done
    [ "$VALUE" -gt "$LAST" ] && LAST=$VALUE
done
NUMBER=$((LAST+1))
case $NUMBER in ?) RUN=00$NUMBER ;; ??) RUN=0$NUMBER ;; *) RUN=$NUMBER ;; esac
OUT=$BASE/$RUN
mkdir "$OUT" || { echo "FAILED: cannot create $OUT"; exit 1; }
mkdir "$OUT/binaries" "$OUT/config" "$OUT/metadata" || {
    echo "FAILED: cannot create collection folders in $OUT"
    exit 1
}

MANIFEST=$OUT/manifest.txt
STATUS=$OUT/status.txt
FAILURES=0

{
    echo "MIB2Q Android Auto firmware collection"
    echo "run=$RUN"
    echo "date=$(date 2>&1)"
    echo "card=$CARD"
    echo "mode=READ ONLY FROM HEAD UNIT"
    echo "format=result|source|saved_as|bytes"
} > "$MANIFEST"

copy_required() {
    CR_SOURCE=$1
    CR_RELATIVE=$2
    CR_DEST=$OUT/$CR_RELATIVE

    if [ ! -f "$CR_SOURCE" ]; then
        echo "MISSING|$CR_SOURCE|$CR_RELATIVE|0" >> "$MANIFEST"
        echo "MISSING: $CR_SOURCE"
        FAILURES=$((FAILURES+1))
        return
    fi

    if cp "$CR_SOURCE" "$CR_DEST" 2>> "$OUT/copy_errors.txt" \
       && cmp -s "$CR_SOURCE" "$CR_DEST"; then
        CR_BYTES=$(wc -c < "$CR_DEST" 2>/dev/null | tr -d ' ')
        echo "OK|$CR_SOURCE|$CR_RELATIVE|${CR_BYTES:-unknown}" >> "$MANIFEST"
        echo "  OK: $CR_SOURCE -> $CR_RELATIVE (${CR_BYTES:-unknown} bytes)"
    else
        echo "FAILED|$CR_SOURCE|$CR_RELATIVE|0" >> "$MANIFEST"
        echo "COPY FAILED OR DID NOT VERIFY: $CR_SOURCE"
        rm -f "$CR_DEST"
        FAILURES=$((FAILURES+1))
    fi
}

echo "Collecting exact Android Auto firmware files into $OUT"
echo "This reads the unit and writes only to the SD card."

copy_required "$UNIT_ROOT/mnt/app/eso/bin/apps/gal" \
              "binaries/gal"
copy_required "$UNIT_ROOT/mnt/app/eso/lib/libautoreceiver.so" \
              "binaries/libautoreceiver.so"
copy_required "$UNIT_ROOT/ifs/lsd.jxe" \
              "binaries/lsd.jxe"
copy_required "$UNIT_ROOT/mnt/system/etc/eso/production/smartphone_integrator.json" \
              "config/smartphone_integrator.json"
copy_required "$UNIT_ROOT/mnt/system/etc/eso/production/gal.json" \
              "config/gal.json"

# Capture enough context to identify the source unit without collecting logs,
# contacts, navigation history, VIN data, or other user content.
uname -a > "$OUT/metadata/uname.txt" 2>&1
df -k "$CARD" > "$OUT/metadata/card_space.txt" 2>&1
{
    ls -ln "$UNIT_ROOT/mnt/app/eso/bin/apps/gal"
    ls -ln "$UNIT_ROOT/mnt/app/eso/lib/libautoreceiver.so"
    ls -ln "$UNIT_ROOT/ifs/lsd.jxe"
    ls -ln "$UNIT_ROOT/mnt/system/etc/eso/production/smartphone_integrator.json"
    ls -ln "$UNIT_ROOT/mnt/system/etc/eso/production/gal.json"
} > "$OUT/metadata/source_files.txt" 2>&1

# QNX images differ in their checksum tools. Record checksums when cksum is
# available; the desktop checker always computes SHA-256 after the card returns.
if command -v cksum >/dev/null 2>&1; then
    {
        for SAVED in "$OUT/binaries/gal" "$OUT/binaries/libautoreceiver.so" \
                     "$OUT/binaries/lsd.jxe" "$OUT/config/smartphone_integrator.json" \
                     "$OUT/config/gal.json"; do
            [ -f "$SAVED" ] && cksum "$SAVED"
        done
    } > "$OUT/metadata/cksum.txt" 2>&1
else
    echo "cksum is not installed on this QNX image; compute SHA-256 on the desktop." \
        > "$OUT/metadata/cksum.txt"
fi

if [ "$FAILURES" -eq 0 ]; then
    echo "COMPLETE: all 5 files copied and byte-compared successfully" > "$STATUS"
    echo "COMPLETE: all 5 required files copied and verified."
    echo "Return the folder: $OUT"
    sync
    exit 0
fi

echo "INCOMPLETE: $FAILURES required file(s) missing or failed verification" > "$STATUS"
echo "INCOMPLETE: $FAILURES required file(s) missing or failed verification."
echo "Keep $OUT for diagnosis and run the collector again after fixing the problem."
sync
exit 1
