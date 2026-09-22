#!/bin/sh
# Patch stock deva-ctrl-qc.so so mpl5_dio_ent (the CarPlay entertainment PCM device) can run 6 voices,
# letting discrete multichannel PCM from libairplay reach six_channel_speakerp -> MOST -> B&O amp.
# See docs/Reference/iOS CarPlay Spatial Wireformat.md sections 11 and 16. Idempotent + verifies. Does NOT touch the unit.
#
# Caps table (deva-ctrl-qc, platform mib2): VA 0xA7D44, .data VA->file = -0x1000 (file 0xA6D44).
# Per-device record stride 48; device index i (1-based). mpl5_dio_ent = idx 4 -> record 0xA6E04.
# Record layout (confirmed against deva_pcm_and_mixer_device_init_platform_mib @0x5707C, which copies
# these straight into the ado_pcm hw caps at ado_pcm_create time):
#   +0 flags  +4 format  +8 rates  +12/+16 min/max_rate  +20/+24 min/max_voices
#   +28/+32 min/max_fragsize  +36 max_buffer (= max_fragsize * max_frags)  +40 max_frags  +44 ?
#
# Three caps fields change; ALL of them mirror mpl1_int_ent (idx 1), the proven 6-channel device:
#   max_voices   +24  0xA6E1C  0x02   -> 0x06      (the lever: allows a 6-voice open)
#   max_fragsize +32  0xA6E24  0x1800 -> 0x4800    (internal MIB format is S32: 16 ms is 0x1800
#                                                   at 2ch and 0x4800 at 6ch)
#   max_buffer   +36  0xA6E28  0xF000 -> 0x2D000   (must track: buffer == max_fragsize * max_frags(10))
# min_voices (+20) and min_fragsize (+28) are deliberately LEFT ALONE so stock stereo still opens
# unchanged when the libairplay hook is off — all three table edits only WIDEN the accepted range.
#
# A dynamic code patch is also required. deva_mib2_aquire @0x563C8 loads ACTIVE frag_size from
# caps.min_fragsize, so the max-only patch opens six voices but still advances 0x1800 bytes every
# fixed 16 ms: exactly 1/3 speed. Replace that load with BL to a five-instruction helper in the
# executable first PT_LOAD's diagnostic-string tail. The helper preserves the stock load, then
# substitutes 0x4800 only when channel->voices (+0x10) is 6. Stock mpl5 stereo stays at 0x1800;
# stock mpl1 6ch was already 0x4800. See RE_TIMING.md.
set -e
STOCK="${1:-$(dirname "$0")/deva-ctrl-qc.so.stock}"
OUT="$(dirname "$0")/deva-ctrl-qc.so"
REC=$((0xA6E04))
NAME_OFF=$((0xA4C54 + 272*4 + 16))   # names VA 0xA5C54 -> file 0xA4C54; verify idx4 == mpl5_dio_ent
PATCH_SITE=$((0x563C8))              # ldr r2,[r5,#0x1c] -> bl 0x8f2c8
CAVE_NUL=$((0x8F2C7))                # terminate the diagnostic __FILE__ string before helper
CAVE=$((0x8F2C8))                    # 20-byte ARM helper; VA == file offset in first PT_LOAD

ORIG_SITE=1c2095e5
PATCHED_SITE=bee300eb
ORIG_CAVE_NUL=63
PATCHED_CAVE_NUL=00
ORIG_CAVE=2f646576615f7372665f706c6174666f726d2e63
PATCHED_CAVE=1c2095e5101096e5060051e3122ba0031eff2fe1

rd(){ xxd -s "$1" -l "$2" -p "$3"; }                       # rd <off> <len> <file>
wr(){ printf '%b' "$2" | dd of="$3" bs=1 seek="$1" conv=notrunc 2>/dev/null; } # wr <off> <\x..> <file>

# 1. sanity: right binary, right device, and each field is either stock or already-patched
name=$(dd if="$STOCK" bs=1 skip=$NAME_OFF count=12 2>/dev/null | tr -d '\0')
[ "$name" = "mpl5_dio_ent" ] || { echo "ABORT: idx4 name='$name' != mpl5_dio_ent (wrong binary/offset)"; exit 1; }

v_voices=$(rd $((REC+24)) 4 "$STOCK")
v_frag=$(rd   $((REC+32)) 4 "$STOCK")
v_buf=$(rd    $((REC+36)) 4 "$STOCK")
case "$v_voices:$v_frag:$v_buf" in
    02000000:00180000:00f00000) caps_state=stock ;;
    06000000:00480000:00d00200) caps_state=patched ;;
    *) echo "ABORT: inconsistent caps voices=$v_voices frag=$v_frag buffer=$v_buf"; exit 1 ;;
esac

v_site=$(rd "$PATCH_SITE" 4 "$STOCK")
v_cave_nul=$(rd "$CAVE_NUL" 1 "$STOCK")
v_cave=$(rd "$CAVE" 20 "$STOCK")
case "$v_site:$v_cave_nul:$v_cave" in
    "$ORIG_SITE:$ORIG_CAVE_NUL:$ORIG_CAVE") code_state=stock ;;
    "$PATCHED_SITE:$PATCHED_CAVE_NUL:$PATCHED_CAVE") code_state=patched ;;
    *) echo "ABORT: dynamic patch site/cave is mixed or unknown"; exit 1 ;;
esac
case "$caps_state:$code_state" in
    stock:stock|patched:stock|patched:patched) ;;
    *) echo "ABORT: invalid caps/code combination $caps_state/$code_state"; exit 1 ;;
esac

# 2. clone + patch
cp "$STOCK" "$OUT"
wr $((REC+24)) '\x06'             "$OUT"    # max_voices   -> 6
wr $((REC+33)) '\x48'             "$OUT"    # max_fragsize -> 0x00004800 (byte 1 of the LE dword)
wr $((REC+36)) '\x00\xd0\x02\x00' "$OUT"    # max_buffer   -> 0x0002D000
wr "$PATCH_SITE" '\xbe\xe3\x00\xeb' "$OUT" # bl 0x8F2C8
wr "$CAVE_NUL"  '\x00'             "$OUT" # terminate diagnostic source path
wr "$CAVE"      '\x1c\x20\x95\xe5\x10\x10\x96\xe5\x06\x00\x51\xe3\x12\x2b\xa0\x03\x1e\xff\x2f\xe1' "$OUT"

# 3. verify against the mpl1_int_ent reference (idx 1, record 0xA6D74)
n_voices=$(rd $((REC+24)) 4 "$OUT"); n_frag=$(rd $((REC+32)) 4 "$OUT"); n_buf=$(rd $((REC+36)) 4 "$OUT")
ref_frag=$(rd $((0xA6D74+32)) 4 "$STOCK"); ref_buf=$(rd $((0xA6D74+36)) 4 "$STOCK")
[ "$n_voices" = "06000000" ] || { echo "FAIL: max_voices=$n_voices"; exit 1; }
[ "$n_frag" = "$ref_frag" ]  || { echo "FAIL: max_fragsize=$n_frag != mpl1's $ref_frag"; exit 1; }
[ "$n_buf"  = "$ref_buf" ]   || { echo "FAIL: max_buffer=$n_buf != mpl1's $ref_buf"; exit 1; }
[ "$(rd "$PATCH_SITE" 4 "$OUT")" = "$PATCHED_SITE" ] || { echo "FAIL: acquire branch"; exit 1; }
[ "$(rd "$CAVE_NUL" 1 "$OUT")" = "$PATCHED_CAVE_NUL" ] || { echo "FAIL: cave string terminator"; exit 1; }
[ "$(rd "$CAVE" 20 "$OUT")" = "$PATCHED_CAVE" ] || { echo "FAIL: dynamic helper"; exit 1; }

changed=$(cmp -l "$STOCK" "$OUT" | wc -l | tr -d ' ')
case "$caps_state:$code_state:$changed" in
    stock:stock:29|patched:stock:25|patched:patched:0) ;;
    *) echo "FAIL: unexpected byte delta $changed for input $caps_state/$code_state"; exit 1 ;;
esac

echo "OK: $OUT"
echo "  idx4 mpl5_dio_ent   voices $(rd $((REC+20)) 4 "$OUT")..$n_voices   frag $(rd $((REC+28)) 4 "$OUT")..$n_frag   buf $n_buf"
echo "  (mpl1_int_ent ref                                    frag ..$ref_frag   buf $ref_buf)"
echo "  acquire @0x563C8 -> helper @0x8F2C8: 6ch active frag 0x4800; other opens keep caps.min_fragsize"
echo "  bytes changed vs stock: $changed"
