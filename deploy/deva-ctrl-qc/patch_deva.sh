#!/bin/sh
# Patch stock deva-ctrl-qc.so so mpl5_dio_ent (the CarPlay entertainment PCM device) can run 6 voices,
# letting discrete multichannel PCM from libairplay reach six_channel_speakerp -> MOST -> B&O amp.
# See docs/reference/AUDIO.md sections 11 and 16. Idempotent + verifies. Does NOT touch the unit.
#
# Caps table (deva-ctrl-qc, platform mib2): VA 0xA7D44, .data VA->file = -0x1000 (file 0xA6D44).
# Per-device record stride 48; device index i (1-based). mpl5_dio_ent = idx 4 -> record 0xA6E04.
# Record layout (confirmed against deva_pcm_and_mixer_device_init_platform_mib @0x5707C, which copies
# these straight into the ado_pcm hw caps at ado_pcm_create time):
#   +0 flags  +4 format  +8 rates  +12/+16 min/max_rate  +20/+24 min/max_voices
#   +28/+32 min/max_fragsize  +36 max_buffer (= max_fragsize * max_frags)  +40 max_frags  +44 ?
#
# Three fields change; ALL of them mirror mpl1_int_ent (idx 1), the proven 6-channel device:
#   max_voices   +24  0xA6E1C  0x02   -> 0x06      (the lever: allows a 6-voice open)
#   max_fragsize +32  0xA6E24  0x1800 -> 0x4800    (6144 B = only 512 frames at 6ch/16-bit vs 1536
#                                                   at stereo; mpl1 uses 0x4800 to keep 1536 frames)
#   max_buffer   +36  0xA6E28  0xF000 -> 0x2D000   (must track: buffer == max_fragsize * max_frags(10))
# min_voices (+20) and min_fragsize (+28) are deliberately LEFT ALONE so stock stereo still opens
# unchanged when the libairplay hook is off — all three edits only WIDEN the accepted range.
set -e
STOCK="${1:-/Users/luka/Desktop/AUDI_2/Firmwares/HU/MHI2Q_US_AUG22_P5087_MU1316/extracted/app/armle/lib/dll/deva-ctrl-qc.so}"
OUT="$(dirname "$0")/deva-ctrl-qc.so"
REC=$((0xA6E04))
NAME_OFF=$((0xA4C54 + 272*4 + 16))   # names VA 0xA5C54 -> file 0xA4C54; verify idx4 == mpl5_dio_ent

rd(){ xxd -s "$1" -l "$2" -p "$3"; }                       # rd <off> <len> <file>
wr(){ printf "$2" | dd of="$3" bs=1 seek="$1" conv=notrunc 2>/dev/null; }   # wr <off> <\x..> <file>

# 1. sanity: right binary, right device, and each field is either stock or already-patched
name=$(dd if="$STOCK" bs=1 skip=$NAME_OFF count=12 2>/dev/null | tr -d '\0')
[ "$name" = "mpl5_dio_ent" ] || { echo "ABORT: idx4 name='$name' != mpl5_dio_ent (wrong binary/offset)"; exit 1; }

v_voices=$(rd $((REC+24)) 4 "$STOCK")
v_frag=$(rd   $((REC+32)) 4 "$STOCK")
v_buf=$(rd    $((REC+36)) 4 "$STOCK")
case "$v_voices" in 02000000|06000000) ;; *) echo "ABORT: max_voices=$v_voices, expected 02000000/06000000"; exit 1;; esac
case "$v_frag"   in 00180000|00480000) ;; *) echo "ABORT: max_fragsize=$v_frag, expected 00180000/00480000"; exit 1;; esac
case "$v_buf"    in 00f00000|00d00200) ;; *) echo "ABORT: max_buffer=$v_buf, expected 00f00000/00d00200"; exit 1;; esac

# 2. clone + patch
cp "$STOCK" "$OUT"
wr $((REC+24)) '\x06'             "$OUT"    # max_voices   -> 6
wr $((REC+33)) '\x48'             "$OUT"    # max_fragsize -> 0x00004800 (byte 1 of the LE dword)
wr $((REC+36)) '\x00\xd0\x02\x00' "$OUT"    # max_buffer   -> 0x0002D000

# 3. verify against the mpl1_int_ent reference (idx 1, record 0xA6D74)
n_voices=$(rd $((REC+24)) 4 "$OUT"); n_frag=$(rd $((REC+32)) 4 "$OUT"); n_buf=$(rd $((REC+36)) 4 "$OUT")
ref_frag=$(rd $((0xA6D74+32)) 4 "$STOCK"); ref_buf=$(rd $((0xA6D74+36)) 4 "$STOCK")
[ "$n_voices" = "06000000" ] || { echo "FAIL: max_voices=$n_voices"; exit 1; }
[ "$n_frag" = "$ref_frag" ]  || { echo "FAIL: max_fragsize=$n_frag != mpl1's $ref_frag"; exit 1; }
[ "$n_buf"  = "$ref_buf" ]   || { echo "FAIL: max_buffer=$n_buf != mpl1's $ref_buf"; exit 1; }
changed=$(cmp -l "$STOCK" "$OUT" | wc -l | tr -d ' ')   # 4 from stock input, 0 if the input was already patched
case "$changed" in 4|0) ;; *) echo "FAIL: $changed bytes differ from input, expected 4 (or 0 if already patched)"; exit 1;; esac

echo "OK: $OUT"
echo "  idx4 mpl5_dio_ent   voices $(rd $((REC+20)) 4 "$OUT")..$n_voices   frag $(rd $((REC+28)) 4 "$OUT")..$n_frag   buf $n_buf"
echo "  (mpl1_int_ent ref                                    frag ..$ref_frag   buf $ref_buf)"
echo "  bytes changed vs stock: $changed"
