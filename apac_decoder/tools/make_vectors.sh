#!/bin/sh
# Generate APAC test vectors on macOS and extract their raw packets and cookie.
#
# Every mandatory encoder parameter must be present: csrc (content source),
# cdrc (DRC compressor configuration) and aspf (ASP interval).  Omitting any of
# them makes afconvert fail with `!dat`, which is what made an earlier round of
# this work conclude, wrongly, that multichannel APAC could not be encoded.
#
# Usage: apac_decoder/tools/make_vectors.sh [outdir]
set -e
OUT=${1:-apac_decoder/vectors}
TOOLS=$(dirname "$0")
mkdir -p "$OUT"

# One distinct tone per speaker so channel order is readable straight off an FFT.
# The LFE slot gets 60 Hz because the encoder band-limits whichever slot the
# layout declares as LFE.
python3 - "$OUT/tones_5_1.wav" <<'PY'
import sys, wave, numpy as np
sr, secs = 48000, 4
t = np.arange(sr * secs) / sr
freqs = [200, 400, 800, 60, 1600, 3200]          # L R C LFE Ls Rs
a = np.stack([0.5 * np.sin(2 * np.pi * f * t) for f in freqs], axis=1)
w = wave.open(sys.argv[1], 'wb')
w.setnchannels(len(freqs)); w.setsampwidth(2); w.setframerate(sr)
w.writeframes((a * 32767).astype('<i2').tobytes()); w.close()
PY

afconvert "$OUT/tones_5_1.wav" "$OUT/tones_5_1.caf" -d LEI16 -l MPEG_5_1_A
afconvert "$OUT/tones_5_1.caf" "$OUT/tones_5_1.mp4" -d apac -l MPEG_5_1_A \
    -u csrc 36 -u cdrc 1 -u aspf 75
python3 "$TOOLS/mp4_extract_apac.py" "$OUT/tones_5_1.mp4" "$OUT/tones_5_1_packets"

echo "vectors in $OUT"
echo "  tones_5_1.wav           reference input, L R C LFE Ls Rs = 200 400 800 60 1600 3200 Hz"
echo "  tones_5_1.mp4           APAC encoded"
echo "  tones_5_1_packets/      cookie.bin plus one raw APAC packet per file"
