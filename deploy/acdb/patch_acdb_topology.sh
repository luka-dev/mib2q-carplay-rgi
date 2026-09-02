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
# Which target.  Chosen on the command line, because what these topologies DO is not proven.
#
# What IS proven, read out of the signed LPASS image (segment va 0xc04b9000 -> file 0x4bc000):
#
#   topology 0x00010314  (device 78, stock)  -> RX descriptor 0xc04e981c
#   topology 0x00010BE3  (DEFAULT_AUDIO_COPP)-> RX descriptor 0xc04e981c   <-- the SAME one
#   topology 0x00010312  (device 15)         -> RX descriptor 0xc04e9804
#
#   0xc04e981c: 11 modules, list 0xc0615980
#   0xc04e9804:  3 modules, list 0xc0615be8
#
# So 0x10314 and 0x10BE3 share one processing graph: swapping to the default would change the
# id and nothing else, and prove nothing.  Only 0x00010312 selects a different graph, and is
# therefore the only target worth testing.
#
# It is still an experiment, and the honest caveats are:
#   - stock ACDB uses 0x10312 only on devices carrying one to three channels; there is no
#     six-channel precedent for it on this platform;
#   - a three-module graph is not the same as "pass-through", and nothing here shows it
#     preserves six channels;
#   - that 0x10314 is what discards C/Ls/Rs is likewise unproven -- it is the only
#     stereo-looking value left, which is suggestive, not conclusive.
#
# Reverting is restoring the original file.
set -e
IN="${1:?usage: patch_acdb_topology.sh <stock Speaker_cal.acdb> [out]}"
OUT="${2:-$(dirname "$0")/Speaker_cal.acdb}"
NEWTOPO="${3:-0x00010312}"

python3 - "$IN" "$OUT" "$NEWTOPO" <<'PY'
import sys, struct, hashlib
src, dst = sys.argv[1], sys.argv[2]
b = bytearray(open(src,'rb').read())
OFFS = (0x78a9, 0x78b1, 0x78b9, 0x78c1)      # the four application-type entries for device 78
OLD, NEW = 0x00010314, int(sys.argv[3], 0)

assert b[:8] == b'QCMSNDDB', "not an ACDB file"
# the device name sits just before the block, UTF-16LE
name = bytes(b[0x7871:0x7899]).decode('utf-16-le', 'replace')
assert '78_TDM_ENTERTAINMENT' in name, "device name mismatch: %r" % name

cur = [struct.unpack_from('<I', b, o)[0] for o in OFFS]
if all(v == NEW for v in cur):
    print("already patched"); open(dst,'wb').write(bytes(b)); sys.exit(0)
assert all(v == OLD for v in cur), "unexpected topology values: %s" % [hex(v) for v in cur]
assert NEW != OLD, "target equals the stock value"

for o in OFFS:
    struct.pack_into('<I', b, o, NEW)
open(dst,'wb').write(bytes(b))
print("device: %s" % name.strip('\x00'))
print("topology %#010x -> %#010x at %s" % (OLD, NEW, ", ".join(hex(o) for o in OFFS)))
print("size unchanged: %d bytes" % len(b))
print("sha256: %s" % hashlib.sha256(bytes(b)).hexdigest())
PY
echo "wrote $OUT"
