#!/usr/bin/env python3
"""Compare two spectrum dumps record for record.

A record is [u32 channel][u32 windowSequence][1024 float32], written by
reference/spectrum_dump_shim.cpp for the library and by
`apac_decode --dump-spectra` for our decoder.  The frame layer has no other
observable output of its own, so a match here is a match of stages f and g
independent of the transform.

    reference/compare_spectra.py LIB.spec PORT.spec [--first N]
"""
import struct, sys

REC = 8 + 1024 * 4


def records(path):
    d = open(path, 'rb').read()
    n = len(d) // REC
    if len(d) % REC:
        print('%s: %d trailing bytes' % (path, len(d) % REC))
    return d, n


def main():
    a_path, b_path = sys.argv[1], sys.argv[2]
    first = None
    if '--first' in sys.argv:
        first = int(sys.argv[sys.argv.index('--first') + 1])
    a, na = records(a_path)
    b, nb = records(b_path)
    n = min(na, nb)
    if first:
        n = min(n, first)
    print('records: %d and %d, comparing %d' % (na, nb, n))
    bad = 0
    firstBad = None
    maxdiff = 0.0
    identical = 0
    total = 0
    for i in range(n):
        ha = struct.unpack_from('<II', a, i * REC)
        hb = struct.unpack_from('<II', b, i * REC)
        fa = struct.unpack_from('<1024f', a, i * REC + 8)
        fb = struct.unpack_from('<1024f', b, i * REC + 8)
        ra = a[i * REC + 8:(i + 1) * REC]
        rb = b[i * REC + 8:(i + 1) * REC]
        ok = (ha == hb) and (ra == rb)
        if ra == rb:
            identical += 1
        total += 1
        for x, y in zip(fa, fb):
            d = abs(x - y)
            if d > maxdiff:
                maxdiff = d
        if not ok:
            bad += 1
            if firstBad is None:
                firstBad = (i, ha, hb)
    print('bit-identical records: %d of %d' % (identical, total))
    print('max abs coefficient difference: %.6g' % maxdiff)
    if firstBad:
        i, ha, hb = firstBad
        print('first mismatching record %d: lib(ch=%d seq=%d) ours(ch=%d seq=%d)'
              % (i, ha[0], ha[1], hb[0], hb[1]))
        fa = struct.unpack_from('<1024f', a, i * REC + 8)
        fb = struct.unpack_from('<1024f', b, i * REC + 8)
        shown = 0
        for k in range(1024):
            if fa[k] != fb[k]:
                print('   [%4d] lib %+.9g  ours %+.9g' % (k, fa[k], fb[k]))
                shown += 1
                if shown == 12:
                    break
    return 0 if bad == 0 else 1


sys.exit(main())
