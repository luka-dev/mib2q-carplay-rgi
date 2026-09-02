#!/bin/sh
# Speaker_cal.acdb: give device 78 (TDM_ENTERTAINMENT1_RX) a pass-through audio topology
# instead of the stereo speaker COPP.
#
# Why.  Every count in the chain is already six: audio_service allows eight, the ACDB entry for
# device 78 declares ChannelConfig 6 with slot offsets 2/5/8/11/14/17 at 24 bits, and ADM opens
# the COPP with dev_num_channel=6 and the map FL FR FC LFE LS RS.  The single stereo-specific
# value left is the topology, 0x00010314 = SPEAKER_STEREO_AUDIO_COPP, selected for all four
# application types.  Six channels in, stereo post-processing out -- which is exactly the
# vehicle result: L and R audible, C/Ls/Rs silent, while the amplifier's own matrix would have
# routed those into the front buses at -3..-8 dB had they arrived.
#
# Why 0x00010312 and not a multichannel topology.  A topology id is keyed to calibration blobs,
# and neither 0x1031b nor 0x10715 appears anywhere in this database -- switching to one would
# point the DSP at calibration that does not exist.  0x00010312 (NONE_AUDIO_COPP) is already
# used in this very file by device 15 (TDM_ANN1), needs no calibration by definition, and does
# no channel-reducing post-processing.  The B&O amplifier does the real processing anyway.
#
# The file has no checksum: QCMSNDDB header, one AVDB chunk whose length is unchanged by a
# dword-for-dword edit.  Reverting is restoring the original file.
set -e
IN="${1:?usage: patch_acdb_topology.sh <stock Speaker_cal.acdb> [out]}"
OUT="${2:-$(dirname "$0")/Speaker_cal.acdb}"

python3 - "$IN" "$OUT" <<'PY'
import sys, struct, hashlib
src, dst = sys.argv[1], sys.argv[2]
b = bytearray(open(src,'rb').read())
OFFS = (0x78a9, 0x78b1, 0x78b9, 0x78c1)      # the four application-type entries for device 78
OLD, NEW = 0x00010314, 0x00010312

assert b[:8] == b'QCMSNDDB', "not an ACDB file"
# the device name sits just before the block, UTF-16LE
name = bytes(b[0x7871:0x7899]).decode('utf-16-le', 'replace')
assert '78_TDM_ENTERTAINMENT' in name, "device name mismatch: %r" % name

cur = [struct.unpack_from('<I', b, o)[0] for o in OFFS]
if all(v == NEW for v in cur):
    print("already patched"); open(dst,'wb').write(bytes(b)); sys.exit(0)
assert all(v == OLD for v in cur), "unexpected topology values: %s" % [hex(v) for v in cur]

for o in OFFS:
    struct.pack_into('<I', b, o, NEW)
open(dst,'wb').write(bytes(b))
print("device: %s" % name.strip('\x00'))
print("topology %#010x -> %#010x at %s" % (OLD, NEW, ", ".join(hex(o) for o in OFFS)))
print("size unchanged: %d bytes" % len(b))
print("sha256: %s" % hashlib.sha256(bytes(b)).hexdigest())
PY
echo "wrote $OUT"
