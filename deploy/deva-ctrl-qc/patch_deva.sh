#!/bin/sh
# Patch stock deva-ctrl-qc.so to raise mpl5_dio_ent (CarPlay entertainment PCM) from 2 -> 6 voices,
# enabling discrete 5.1 PCM from libairplay to reach the 6ch mixer path -> six_channel_speakerp -> MOST -> B&O amp.
# See docs/reference/AUDIO.md section 11. Idempotent + verifies. Does NOT touch the unit.
#
# Caps table (deva-ctrl-qc, platform mib2): VA 0xA7D44, .data VA->file = -0x1000 (file 0xA6D44).
# Per-device record stride 48; device index i (1-based). mpl5_dio_ent = idx 4.
#   record   = 0xA6D44 + 48*4 = 0xA6E04
#   min_voices @ +20 = 0xA6E18   (kept at 2, so stock stereo still opens when the hook is off)
#   max_voices @ +24 = 0xA6E1C   (2 -> 6)  <-- the one-byte lever
set -e
STOCK="${1:-/Users/luka/Desktop/AUDI_2/Firmwares/HU/MHI2Q_US_AUG22_P5087_MU1316/extracted/app/armle/lib/dll/deva-ctrl-qc.so}"
OUT="$(dirname "$0")/deva-ctrl-qc.so"
MAXV_OFF=$((0xA6E1C))
NAME_OFF=$((0xA4C54 + 272*4 + 16))   # names VA 0xA5C54 -> file 0xA4C54; verify idx4 == mpl5_dio_ent

rdb(){ xxd -s "$1" -l 1 -p "$2"; }

# 1. sanity: source must be the expected device at idx 4, currently 2 voices
name=$(dd if="$STOCK" bs=1 skip=$NAME_OFF count=12 2>/dev/null | tr -d '\0')
[ "$name" = "mpl5_dio_ent" ] || { echo "ABORT: idx4 name='$name' != mpl5_dio_ent (wrong binary/offset)"; exit 1; }
cur=$(rdb $MAXV_OFF "$STOCK")
[ "$cur" = "02" -o "$cur" = "06" ] || { echo "ABORT: max_voices byte=0x$cur, expected 0x02 (stock) or 0x06 (already patched)"; exit 1; }

# 2. clone + patch (printf writes the single byte 0x06 at the max_voices offset)
cp "$STOCK" "$OUT"
printf '\x06' | dd of="$OUT" bs=1 seek=$MAXV_OFF count=1 conv=notrunc 2>/dev/null

# 3. verify
new=$(rdb $MAXV_OFF "$OUT")
minv=$(rdb $((0xA6E18)) "$OUT")
[ "$new" = "06" ] || { echo "FAIL: post-patch max_voices=0x$new"; exit 1; }
echo "OK: $OUT"
echo "  idx4 = mpl5_dio_ent : min_voices=0x$minv  max_voices=0x$new  (was 0x$cur)"
echo "  bytes changed: 1 (offset 0x$(printf %x $MAXV_OFF))"
