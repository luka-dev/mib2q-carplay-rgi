#!/bin/sh
# Second deva patch: make the CSD route/session take its geometry from the LIVE PCM channel
# instead of the capability minima.  Applies on top of patch_deva.sh's output.
#
# Why (docs/Audio/QNX Audio Topology.md): deva_update_session_info_from_list @0x54848, reached
# from session_csd_setup on route attach, copies record min_voices -> session +0x84 (active
# channel count) and min_fragsize -> session +0x6c (active CSD buffer).  mpl1_int_ent has
# min 6/0x4800 so it is right by accident; mpl5_dio_ent has min 2/0x1800, so a six-voice open
# widens the DMA side to 6/0x4800 while CSD stays stereo.  CSD then copies 0x1800 of every
# 0x4800 fragment -- 256 frames of 768 -- and drops the rest: a discontinuity every 16 ms,
# 62.5 Hz, which is the tick heard in the car with the sample clock measured correct.
#
# The fix keeps the record values as the fallback and prefers the live channel when one is
# attached, so stereo is bit-for-bit unchanged.  Widening the minima instead would make every
# stereo open fail.
#
#   site  0x54a14  ldrh r2,[r3,#20]        -> b cave
#   cave  0x8d090  reads deva_timer +0x11c -> pcm channel, +0x10 voices, +0x94 fragment
#   donor 0x8d037  deva_srf_mixer_router.c __FILE__ string, truncated; only slogf text is lost
set -e
IN="${1:-$(dirname "$0")/deva-ctrl-qc.so}"
OUT="${2:-$(dirname "$0")/deva-ctrl-qc-csdlive.so}"
CAVEBIN="${3:-$(dirname "$0")/../../build/cave.bin}"

[ -f "$IN" ] || { echo "ABORT: missing input $IN"; exit 1; }
[ -f "$CAVEBIN" ] || { echo "ABORT: missing assembled cave $CAVEBIN"; exit 1; }

python3 - "$IN" "$OUT" "$CAVEBIN" <<'PY'
import sys, struct
src, dst, cavebin = sys.argv[1], sys.argv[2], sys.argv[3]
b = bytearray(open(src,'rb').read())
body = open(cavebin,'rb').read()

SITE  = 0x54a14      # ldrh r2,[r3,#20]   (record min_voices)
RET   = 0x54a2c      # the b that follows the original assignment block
CAVE  = 0x8d090
DON   = 0x8d037      # deva_srf_mixer_router.c __FILE__
DONEND= DON + 172

def bl_b(site, target):                       # unconditional ARM B
    return struct.pack('<I', 0xEA000000 | (((target - (site + 8)) >> 2) & 0xFFFFFF))

need = len(body) + 4
assert CAVE % 4 == 0, "cave must be word aligned"
assert CAVE + need <= DONEND, "cave does not fit in the donor string"

site_now = bytes(b[SITE:SITE+4])
if site_now == bl_b(SITE, CAVE):
    print("already patched; nothing to do"); open(dst,'wb').write(bytes(b)); sys.exit(0)
assert site_now == bytes.fromhex('b421d3e1'), \
    "unexpected bytes at the patch site: %s" % site_now.hex()
assert b[DON:DON+20] == b'/local/mnt/workspace', "donor string is not where expected"
assert all(32 <= c < 127 for c in b[CAVE:DONEND]), "cave region is not plain ASCII"

b[SITE:SITE+4] = bl_b(SITE, CAVE)
b[CAVE-1]      = 0                            # terminate the donor string before the cave
b[CAVE:CAVE+len(body)] = body
b[CAVE+len(body):CAVE+len(body)+4] = bl_b(CAVE + len(body), RET)

open(dst,'wb').write(bytes(b))
kept = bytes(b[DON:CAVE-1]).decode()
print("patched  site %#x -> cave %#x (%d bytes)" % (SITE, CAVE, need))
print("donor string now ends: ...%s" % kept[-40:])
PY

echo "wrote $OUT"
