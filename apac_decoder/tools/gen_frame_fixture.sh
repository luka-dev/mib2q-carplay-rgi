#!/usr/bin/env bash
#
# Build tests/fixtures/frames.bin: six real packets out of the CarPlay capture
# and the float32 PCM the ORIGINAL library produces for them, so `make check`
# can run the whole-frame differential test with no Docker and no capture.
#
#   apac_decoder/tools/gen_frame_fixture.sh          # needs Docker, ~10 s
#
# WHICH SIX, AND WHY.  Packets 429..434, contiguous, decoded from a fresh
# decoder.  Instrumenting the port and replaying all 3977 packets shows what
# each one exercises; that window is the SHORTEST run that reaches every
# feature this content ever uses except one, and adding ten more packets buys
# only a second short-window grouping:
#
#   429  long blocks, TNS on and off, an absent element, a joint channel pair
#   430  the same
#   431  a LONG_START window - the run-in to eight short windows
#   432  EIGHT_SHORT, one window group
#   433  EIGHT_SHORT, three window groups
#   434  a LONG_STOP window - the run-out
#
# Never reached anywhere in the capture, so not in the fixture either: a channel
# pair WITHOUT the CAC tool (every pair in all 3977 packets is joint), an LRVQ
# frame, and an active bandwidth-extension channel.
#
# The file carries the magic cookie too, so the test parses the same config the
# decoder was configured with rather than a transcription of it.
set -euo pipefail

HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
DEC="$(cd "$HERE/.." && pwd)"
ROOT="$(cd "$DEC/.." && pwd)"
REF="$DEC/reference"
WORK="$REF/.build"
IMAGE="${APAC_IMAGE:-apac-ref}"
CAPTURE="${APAC_CAPTURE:-$ROOT/captures/apac_5_1_live_20260831_0205}"
COOKIE="$CAPTURE/spatial_magiccookie_5_60100d08f9d19abb.bin"
PACKETS="$CAPTURE/apac_packets"
OUT="$DEC/tests/fixtures/frames.bin"

FIRST=${FIRST:-429}
COUNT=${COUNT:-6}

mkdir -p "$WORK"
: > "$WORK/frame_list_host.txt"
: > "$WORK/frame_list_ctr.txt"
for i in $(seq "$FIRST" $((FIRST + COUNT - 1))); do
    printf '%s/p%05d.bin\n' "$PACKETS" "$i" >> "$WORK/frame_list_host.txt"
    printf '/data/apac_packets/p%05d.bin\n' "$i" >> "$WORK/frame_list_ctr.txt"
done

docker run --rm --platform linux/arm64 \
    -v "$REF:/src:ro" -v "$CAPTURE:/data:ro" -v "$WORK:/out" \
    "$IMAGE" bash -c \
    'g++ -std=c++11 -O2 -g -Wall -o /tmp/apac_ref /src/apac_ref.cpp -ldl &&
     exec /tmp/apac_ref --f32 --list /out/frame_list_ctr.txt "$@"' \
    _ "/data/$(basename "$COOKIE")" /out/frames_ref.f32

python3 - "$COOKIE" "$WORK/frame_list_host.txt" "$WORK/frames_ref.f32" "$OUT" <<'PY'
import struct, sys
cookie, listPath, pcmPath, out = sys.argv[1:5]
ck = open(cookie, 'rb').read()
packets = [open(p.strip(), 'rb').read() for p in open(listPath) if p.strip()]
pcm = open(pcmPath, 'rb').read()
channels, frameLength = 6, 1024
want = len(packets) * channels * frameLength * 4
assert len(pcm) == want, (len(pcm), want)
blob = b'APACFRM1'
blob += struct.pack('<III', len(packets), channels, frameLength)
blob += struct.pack('<I', len(ck)) + ck
blob += b''.join(struct.pack('<I', len(p)) for p in packets)
blob += b''.join(packets)
blob += pcm
open(out, 'wb').write(blob)
print('wrote %s: %d packets, %d bytes' % (out, len(packets), len(blob)))
PY
