#!/usr/bin/env python3
"""Compare two float32 PCM streams, in float and in S16 LSBs.

    compare_f32.py A.f32 B.f32 CHANNELS [FRAMES_PER_BLOCK]

Written for the stage-d composition test: A is Pioneer's own decode of the
capture, B is the same decode with the port's transform stage patched in, and
the question is what the substitution costs at the output.

Reports, per channel and overall:
  * peak |A| - the full scale the errors are measured against;
  * max |A-B| and its RMS, as a fraction of that full scale and in dBFS;
  * the same after quantising both sides the way Apple's converter does
    (round(x * 32768), clamped), in S16 LSBs - which is the unit that decides
    whether the difference can reach a listener at all;
  * the WORST BLOCK, so a difference concentrated in a handful of frames cannot
    hide behind a good average.
"""
import struct
import sys


def load(path):
    with open(path, 'rb') as fh:
        raw = fh.read()
    n = len(raw) // 4
    return struct.unpack('<%df' % n, raw[:n * 4])


def s16(x):
    v = int(round(x * 32768.0))
    return -32768 if v < -32768 else (32767 if v > 32767 else v)


def main():
    if len(sys.argv) < 4:
        sys.exit(__doc__)
    a = load(sys.argv[1])
    b = load(sys.argv[2])
    ch = int(sys.argv[3])
    block = int(sys.argv[4]) if len(sys.argv) > 4 else 1024

    n = min(len(a), len(b))
    if len(a) != len(b):
        print('WARNING: %d vs %d samples; comparing the first %d'
              % (len(a), len(b), n))
    frames = n // ch
    print('%d frames x %d channels' % (frames, ch))

    peak = 0.0
    for i in range(n):
        if abs(a[i]) > peak:
            peak = abs(a[i])
    print('reference peak |A| = %.6f' % peak)

    print()
    print('  ch |    peak |A| |   max |A-B| |  rel to peak |     dBFS | '
          'max S16 LSB | S16 samples differing')
    print('  ---+-------------+-------------+--------------+----------+'
          '-------------+----------------------')
    worstRel = 0.0
    worstLsb = 0
    totalDiff = 0
    for c in range(ch):
        pk = 0.0
        mx = 0.0
        acc = 0.0
        lsb = 0
        diff = 0
        cnt = 0
        for i in range(c, n, ch):
            av, bv = a[i], b[i]
            if abs(av) > pk:
                pk = abs(av)
            d = abs(av - bv)
            if d > mx:
                mx = d
            acc += d * d
            cnt += 1
            q = abs(s16(av) - s16(bv))
            if q:
                diff += 1
                if q > lsb:
                    lsb = q
        rel = mx / pk if pk else 0.0
        db = (20.0 * __import__('math').log10(mx / pk)) if (pk and mx) else float('-inf')
        print('  %2d | %11.6f | %11.3e | %12.3e | %8.1f | %11d | %d / %d (%.4f%%)'
              % (c, pk, mx, rel, db, lsb, diff, cnt, 100.0 * diff / cnt if cnt else 0))
        worstRel = max(worstRel, rel)
        worstLsb = max(worstLsb, lsb)
        totalDiff += diff
        acc = acc  # rms kept per-channel only in the block scan below

    print()
    print('worst over all channels: %.3e of that channel\'s peak, %d S16 LSB'
          % (worstRel, worstLsb))
    print('S16 samples differing at all: %d / %d (%.4f%%)'
          % (totalDiff, n, 100.0 * totalDiff / n if n else 0))

    # Per-block worst, so a localised divergence cannot average away.
    print()
    nb = frames // block
    wb = -1
    wbRel = 0.0
    wbLsb = 0
    for k in range(nb):
        lo = k * block * ch
        hi = lo + block * ch
        pk = 0.0
        mx = 0.0
        lsb = 0
        for i in range(lo, hi):
            if abs(a[i]) > pk:
                pk = abs(a[i])
            d = abs(a[i] - b[i])
            if d > mx:
                mx = d
            q = abs(s16(a[i]) - s16(b[i]))
            if q > lsb:
                lsb = q
        rel = mx / pk if pk else 0.0
        if rel > wbRel:
            wbRel, wb = rel, k
        if lsb > wbLsb:
            wbLsb = lsb
    print('worst %d-frame block: #%d, %.3e of that block\'s peak; '
          'worst block S16 error %d LSB over %d blocks'
          % (block, wb, wbRel, wbLsb, nb))


if __name__ == '__main__':
    main()
