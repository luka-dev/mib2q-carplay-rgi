#!/usr/bin/env python3
"""Compare two interleaved S16LE PCM files channel by channel.

Reports per-channel RMS, RMS of the difference and the worst sample error, then
the full cross-channel rms(diff) matrix, so a channel-order difference between
two decoders shows up as a permutation instead of as noise.

usage: compare_pcm.py A.pcm B.pcm [CHANNELS]

Uses numpy when available and falls back to pure Python otherwise.
"""
import struct
import sys

try:
    import numpy as np
except ImportError:
    np = None


def load(path, ch):
    raw = open(path, 'rb').read()
    n = len(raw) // 2
    n -= n % ch
    if np is not None:
        a = np.frombuffer(raw[:n * 2], dtype='<i2').reshape(-1, ch).astype('f8')
        return [a[:, c] for c in range(ch)]
    s = struct.unpack('<%dh' % n, raw[:n * 2])
    return [s[c::ch] for c in range(ch)]


def rms(xs):
    if np is not None:
        return float(np.sqrt(np.mean(xs * xs))) if len(xs) else 0.0
    return (sum(float(x) * x for x in xs) / len(xs)) ** 0.5 if xs else 0.0


def diff_stats(a, b):
    """returns (rms of difference, max |difference|, fraction of exact matches)"""
    n = min(len(a), len(b))
    if n == 0:
        return 0.0, 0.0, 1.0
    if np is not None:
        d = a[:n] - b[:n]
        return float(np.sqrt(np.mean(d * d))), float(np.max(np.abs(d))), \
            float(np.count_nonzero(d == 0)) / n
    tot = 0.0
    mx = 0.0
    same = 0
    for i in range(n):
        d = float(a[i]) - b[i]
        tot += d * d
        if abs(d) > mx:
            mx = abs(d)
        if d == 0:
            same += 1
    return (tot / n) ** 0.5, mx, float(same) / n


def main():
    if len(sys.argv) < 3:
        print(__doc__)
        return 2
    ch = int(sys.argv[3]) if len(sys.argv) > 3 else 6
    a = load(sys.argv[1], ch)
    b = load(sys.argv[2], ch)
    na, nb = len(a[0]), len(b[0])
    print('A = %s   %d frames' % (sys.argv[1], na))
    print('B = %s   %d frames' % (sys.argv[2], nb))
    if na != nb:
        print('WARNING: frame counts differ; comparing the common prefix')

    print('\nper channel, A[c] vs B[c]   (full scale 32768)')
    print('  ch     A rms      B rms   rms(diff)     rel%    max|d|   identical%')
    for c in range(ch):
        d, mx, same = diff_stats(a[c], b[c])
        ra, rb = rms(a[c]), rms(b[c])
        print('  %2d  %9.2f  %9.2f  %10.4f  %7.4f  %7.0f  %9.3f'
              % (c, ra, rb, d, 100.0 * d / max(ra, 1e-9), mx, 100.0 * same))

    print('\nrms(diff) matrix, A rows vs B columns (row minimum starred)')
    print('        ' + ''.join('  B%-9d' % c for c in range(ch)))
    perm = []
    for r in range(ch):
        row = [diff_stats(a[r], b[c])[0] for c in range(ch)]
        # Prefer the diagonal on ties: two channels that are silent in both
        # files match every other silent channel equally well, and that is not
        # evidence of a reordering.
        lo = min(row)
        best = r if row[r] <= lo + 1e-9 else min(range(ch), key=lambda c: row[c])
        perm.append(best)
        cells = ''.join(('*%-10.2f' if c == best else ' %-10.2f') % row[c] for c in range(ch))
        print('  A%-4d %s' % (r, cells))
    print('\nbest permutation: A[c] matches B[%s]' % ','.join(str(p) for p in perm))
    print('channel order identical: %s' % ('YES' if perm == list(range(ch)) else 'NO'))
    silent = [c for c in range(ch) if rms(a[c]) < 1.0 and rms(b[c]) < 1.0]
    if silent:
        print('note: channels %s are silent in both files, so their position '
              'carries no information' % ','.join(str(c) for c in silent))
    return 0


if __name__ == '__main__':
    sys.exit(main())
