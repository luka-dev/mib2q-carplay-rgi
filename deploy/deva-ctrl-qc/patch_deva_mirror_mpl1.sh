#!/bin/sh
# Third deva step: make mpl5_dio_ent's caps record identical to mpl1_int_ent, the device that
# already renders six voices correctly on this unit.
#
# Why this and not a CSD rebuild.  The route/session is configured when Audioman attaches the
# timer, which the unit's slog shows happening 14 seconds BEFORE the PCM channel exists:
#
#   00:00:38  session_csd_setup: ... DEVICE_ID = <4> is being attached
#   00:00:38  pcm format => sample_rate=48000, channels=2, bit_per_sample=24
#   00:00:52  [hook] setup actual=6ch/48000Hz/9216-byte fragment
#
# So the session takes its geometry from the record minima and nothing rebuilds it later.
# Rebuilding it from the PCM path needs session_csd_teardown(session) + session_csd_setup(
# session, timer), and both pointers live in deva_mixer_router_configure_switch_routing's
# stack frame -- not reachable from acquire without more reverse engineering.
#
# Setting the minima to the mpl1 values makes the attach-time geometry correct in the first
# place.  The old objection was that a stereo open would then fall below min_voices and fail;
# the QEMU harness showed QSA quietly inserting a channel conversion instead, which is also
# how mpl1 (min == max == 6) serves stereo clients today.  With every open landing at six
# voices, channel->voices is always 6, so the existing conditional fragment helper keeps
# returning 0x4800 and the geometry stays self-consistent.
#
# Reversible: rerun patch_deva.sh to get back the asymmetric record.
set -e
IN="${1:-$(dirname "$0")/deva-ctrl-qc-csdlive.so}"
OUT="${2:-$(dirname "$0")/deva-ctrl-qc-mirror.so}"
[ -f "$IN" ] || { echo "ABORT: missing $IN"; exit 1; }

python3 - "$IN" "$OUT" <<'PY'
import sys, struct, hashlib
src, dst = sys.argv[1], sys.argv[2]
b = bytearray(open(src,'rb').read())
REC = 0xa6e04                       # mpl5_dio_ent, caps table idx 4
MPL1 = 0xa6d74                      # mpl1_int_ent, idx 1
F = {'min_voices':0x14, 'max_voices':0x18, 'min_frag':0x1c, 'max_frag':0x20,
     'max_buffer':0x24, 'max_frags':0x28}
def rd(base, off): return struct.unpack_from('<I', b, base+off)[0]

before = {k: rd(REC,o) for k,o in F.items()}
ref    = {k: rd(MPL1,o) for k,o in F.items()}
assert before['max_voices'] == 6 and before['max_frag'] == 0x4800, \
    "run patch_deva.sh first: %r" % before
assert before['min_voices'] in (2,6), "unexpected min_voices %r" % before['min_voices']

struct.pack_into('<I', b, REC+F['min_voices'], 6)
struct.pack_into('<I', b, REC+F['min_frag'],  0x4800)
after = {k: rd(REC,o) for k,o in F.items()}
assert after == ref, "mpl5 record still differs from mpl1:\n  mpl5=%r\n  mpl1=%r" % (after, ref)

open(dst,'wb').write(bytes(b))
print("mpl5 record now matches mpl1 exactly:")
for k in F: print("   %-11s %#x" % (k, after[k]))
print("sha256:", hashlib.sha256(bytes(b)).hexdigest())
PY
echo "wrote $OUT"
