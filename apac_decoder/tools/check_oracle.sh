#!/bin/sh
# Smallest thing that fails if the reference chain breaks: generate an APAC
# vector, decode it with Apple's decoder through the raw-packet path, and assert
# every speaker came back on the slot it was encoded on.
#
# Usage: apac_decoder/tools/check_oracle.sh
set -e
ROOT=$(cd "$(dirname "$0")/../.." && pwd)
cd "$ROOT"
OUT=apac_decoder/vectors

[ -x build/apple_apac_oracle ] || clang -std=c11 -O2 -w tools/apple_apac_oracle.c \
    -framework AudioToolbox -framework CoreFoundation -o build/apple_apac_oracle
[ -d "$OUT/tones_5_1_packets" ] || apac_decoder/tools/make_vectors.sh "$OUT" >/dev/null

build/apple_apac_oracle "$OUT/tones_5_1_packets/cookie.bin" "$OUT/tones_5_1.pcm" \
    "$OUT"/tones_5_1_packets/p*.bin >/dev/null

python3 - "$OUT/tones_5_1.pcm" <<'PY'
import sys, numpy as np
expected = [200, 400, 800, 60, 1600, 3200]
d = np.fromfile(sys.argv[1], dtype='<i2').reshape(-1, 6).astype(float)
seg = d[48000:48000 * 3]
win = np.hanning(len(seg))
bins = np.fft.rfftfreq(len(seg), 1 / 48000)
got = []
for c in range(6):
    got.append(int(round(bins[np.argmax(np.abs(np.fft.rfft(seg[:, c] * win)))])))
print('expected', expected)
print('decoded ', got)
assert got == expected, 'channel order or decode broke'
print('OK: Apple oracle round trip is order-preserving')
PY
